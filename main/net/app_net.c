#include "net/app_net.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "app_settings.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "net";

/* Give up announcing "connecting" after this many rejected joins so the UI can
 * say something useful instead of spinning forever. */
#define MAX_JOIN_ATTEMPTS 5
#define REJOIN_DELAY_MS   3000

/* After the user picks a network, scans are refused for this long so the page's
 * periodic rescan cannot abort the join it just asked for. */
#define JOIN_GRACE_US     (20 * 1000 * 1000)

/*
 * The rejoin is deferred through a private event rather than a delay in the
 * handler: sleeping inside the default event loop stalled every Wi-Fi event
 * behind it, SCAN_DONE included. The timer only posts; esp_wifi_connect() still
 * runs on the event loop, as it always did.
 */
ESP_EVENT_DEFINE_BASE(APP_NET_EVENT);
enum { APP_NET_EVENT_REJOIN };

static volatile app_net_state_t s_state = APP_NET_DISABLED;
static volatile bool s_time_valid = false;
static volatile bool s_scanning = false;
static volatile bool s_suspended = false;
static int s_join_attempts = 0;
static uint32_t s_scan_generation = 0;
static int64_t s_join_grace_until = 0;
static esp_timer_handle_t s_rejoin_timer;

static char s_ip[16] = "-";
static SemaphoreHandle_t s_scan_lock;
static net_ap_t s_scan_results[NET_MAX_SCAN_RESULTS];
static int s_scan_count = 0;

static void start_join(void);

static void rejoin_timer_cb(void *arg)
{
    (void)arg;
    esp_event_post(APP_NET_EVENT, APP_NET_EVENT_REJOIN, NULL, 0, 0);
}

static void schedule_rejoin(void)
{
    esp_timer_stop(s_rejoin_timer);
    esp_timer_start_once(s_rejoin_timer, REJOIN_DELAY_MS * 1000ULL);
}

/* Station is associating or retrying, rather than idle or joined. */
static bool joining(void)
{
    return s_state == APP_NET_CONNECTING || s_state == APP_NET_FAILED;
}

/* ---------------------------------------------------------------- events -- */

static void collect_scan_results(void)
{
    uint16_t found = 0;
    esp_wifi_scan_get_ap_num(&found);
    if (found > NET_MAX_SCAN_RESULTS) {
        found = NET_MAX_SCAN_RESULTS;
    }

    wifi_ap_record_t *records = calloc(found ? found : 1, sizeof(wifi_ap_record_t));
    if (records == NULL) {
        esp_wifi_clear_ap_list();  /* the driver holds them until read or cleared */
        return;
    }

    uint16_t count = found;
    if (esp_wifi_scan_get_ap_records(&count, records) == ESP_OK) {
        xSemaphoreTake(s_scan_lock, portMAX_DELAY);
        s_scan_count = 0;
        for (int i = 0; i < count && s_scan_count < NET_MAX_SCAN_RESULTS; i++) {
            if (records[i].ssid[0] == '\0') {
                continue;  /* hidden network, nothing to show or tap */
            }
            /* The driver can report the same SSID once per band or per AP in a
             * mesh; keep only the strongest of each name. */
            bool duplicate = false;
            for (int j = 0; j < s_scan_count; j++) {
                if (strcmp(s_scan_results[j].ssid, (char *)records[i].ssid) == 0) {
                    duplicate = true;
                    if (records[i].rssi > s_scan_results[j].rssi) {
                        s_scan_results[j].rssi = records[i].rssi;
                    }
                    break;
                }
            }
            if (duplicate) {
                continue;
            }
            net_ap_t *ap = &s_scan_results[s_scan_count++];
            strlcpy(ap->ssid, (char *)records[i].ssid, sizeof(ap->ssid));
            ap->rssi = records[i].rssi;
            ap->secured = records[i].authmode != WIFI_AUTH_OPEN;
        }

        /* Strongest first. */
        for (int i = 1; i < s_scan_count; i++) {
            net_ap_t key = s_scan_results[i];
            int j = i - 1;
            while (j >= 0 && s_scan_results[j].rssi < key.rssi) {
                s_scan_results[j + 1] = s_scan_results[j];
                j--;
            }
            s_scan_results[j + 1] = key;
        }
        xSemaphoreGive(s_scan_lock);
    }

    free(records);
    ESP_LOGI(TAG, "scan found %d networks", s_scan_count);
}

static void on_scan_done(void)
{
    collect_scan_results();
    s_scan_generation++;
    s_scanning = false;
    /* The scan paused the retry loop; pick it back up. */
    if (settings()->wifi_ssid[0] != '\0' && joining()) {
        schedule_rejoin();
    }
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == APP_NET_EVENT && id == APP_NET_EVENT_REJOIN) {
        /* A scan that started meanwhile owns the radio; it reschedules us. */
        if (!s_scanning && settings()->wifi_ssid[0] != '\0') {
            esp_wifi_connect();
        }
        return;
    }

    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            start_join();
            break;

        case WIFI_EVENT_SCAN_DONE:
            on_scan_done();
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            strlcpy(s_ip, "-", sizeof(s_ip));
            if (s_suspended || settings()->wifi_ssid[0] == '\0') {
                s_state = APP_NET_DISABLED;
                break;
            }
            if (s_scanning) {
                /* We dropped the join ourselves so the scan could run. It is
                 * not a rejection, and SCAN_DONE resumes the retries. */
                if (s_state != APP_NET_FAILED) {
                    s_state = APP_NET_CONNECTING;
                }
                break;
            }
            if (++s_join_attempts > MAX_JOIN_ATTEMPTS) {
                /* Keep retrying quietly, but stop claiming to be connecting. */
                s_state = APP_NET_FAILED;
            } else {
                s_state = APP_NET_CONNECTING;
            }
            schedule_rejoin();
            break;

        default:
            break;
        }
        return;
    }

    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&ev->ip_info.ip));
        ESP_LOGI(TAG, "got ip %s", s_ip);
        s_join_attempts = 0;
        s_state = s_time_valid ? APP_NET_SYNCED : APP_NET_CONNECTED;
    }
}

static void on_time_synced(struct timeval *tv)
{
    (void)tv;
    s_time_valid = true;
    s_state = APP_NET_SYNCED;
    ESP_LOGI(TAG, "clock synced via sntp");
}

/* ------------------------------------------------------------------ join -- */

static void start_join(void)
{
    const settings_t *cfg = settings();
    if (cfg->wifi_ssid[0] == '\0') {
        ESP_LOGW(TAG, "no ssid configured, running offline on the rtc");
        s_state = APP_NET_DISABLED;
        return;
    }

    wifi_config_t wifi_cfg = { 0 };
    strlcpy((char *)wifi_cfg.sta.ssid, cfg->wifi_ssid, sizeof(wifi_cfg.sta.ssid));
    strlcpy((char *)wifi_cfg.sta.password, cfg->wifi_pass, sizeof(wifi_cfg.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));

    s_state = APP_NET_CONNECTING;
    esp_wifi_connect();
}

void app_net_connect(const char *ssid, const char *password)
{
    settings_t *cfg = settings();
    strlcpy(cfg->wifi_ssid, ssid, sizeof(cfg->wifi_ssid));
    strlcpy(cfg->wifi_pass, password ? password : "", sizeof(cfg->wifi_pass));
    settings_save();

    s_join_attempts = 0;
    s_join_grace_until = esp_timer_get_time() + JOIN_GRACE_US;
    esp_timer_stop(s_rejoin_timer);
    esp_wifi_disconnect();
    start_join();
}

/* ----------------------------------------------------------------- start -- */

static void net_task(void *arg)
{
    (void)arg;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &on_event, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &on_event, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        APP_NET_EVENT, ESP_EVENT_ANY_ID, &on_event, NULL, NULL));

    const esp_timer_create_args_t timer_args = {
        .callback = rejoin_timer_cb,
        .name = "rejoin",
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_rejoin_timer));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    /* Started unconditionally: with no credentials we still want the radio up
     * so the settings screen can scan for networks. */
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_sntp_config_t sntp_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_CLOCK_NTP_SERVER);
    sntp_cfg.start = true;
    sntp_cfg.server_from_dhcp = true;
    sntp_cfg.sync_cb = on_time_synced;
    esp_netif_sntp_init(&sntp_cfg);

    vTaskDelete(NULL);
}

void app_net_start(void)
{
    s_scan_lock = xSemaphoreCreateMutex();

    /* NVS is brought up by settings_init(), which runs first and needs it. */
    s_state = APP_NET_CONNECTING;
    xTaskCreate(net_task, "net", 5120, NULL, 5, NULL);
}

/* ----------------------------------------------------------------- query -- */

bool app_net_scan_start(void)
{
    if (s_scanning) {
        return true;
    }
    if (s_suspended || s_rejoin_timer == NULL) {
        return false;  /* net_task has not finished bringing Wi-Fi up */
    }

    /*
     * The driver refuses to scan while the station is connecting
     * (ESP_ERR_WIFI_STATE), and a connect aborts a scan in flight. With the
     * stored network out of range the retry loop never stops connecting, so
     * the scan never ran and the list stayed empty - exactly when you most
     * need it. Pause the retries and drop the pending join; SCAN_DONE resumes.
     *
     * A join the user has just asked for is left alone for a grace period.
     */
    if (joining()) {
        if (esp_timer_get_time() < s_join_grace_until) {
            return false;
        }
        /* Set first: the DISCONNECTED this causes must read as ours. */
        s_scanning = true;
        esp_timer_stop(s_rejoin_timer);
        esp_wifi_disconnect();
    }

    s_scanning = true;
    wifi_scan_config_t cfg = { .show_hidden = false };
    esp_err_t err = esp_wifi_scan_start(&cfg, false);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "scan start failed: %s", esp_err_to_name(err));
        s_scanning = false;
        if (settings()->wifi_ssid[0] != '\0' && joining()) {
            schedule_rejoin();
        }
        return false;
    }
    return true;
}

uint32_t app_net_scan_generation(void)
{
    return s_scan_generation;
}

bool app_net_scanning(void)
{
    return s_scanning;
}

int app_net_get_scan_results(net_ap_t *out, int max)
{
    xSemaphoreTake(s_scan_lock, portMAX_DELAY);
    int n = s_scan_count < max ? s_scan_count : max;
    memcpy(out, s_scan_results, n * sizeof(net_ap_t));
    xSemaphoreGive(s_scan_lock);
    return n;
}

void app_net_suspend(void)
{
    ESP_LOGI(TAG, "suspending wifi");
    s_suspended = true;
    esp_timer_stop(s_rejoin_timer);
    esp_wifi_disconnect();
    esp_wifi_stop();
    s_state = APP_NET_DISABLED;
    strlcpy(s_ip, "-", sizeof(s_ip));
}

void app_net_resume(void)
{
    ESP_LOGI(TAG, "resuming wifi");
    s_suspended = false;
    s_join_attempts = 0;
    /* STA_START brings start_join() round again, so the stored credentials are
     * reapplied without repeating them here. */
    esp_wifi_start();
}

void app_net_sync_now(void)
{
    esp_netif_sntp_start();
}

app_net_state_t app_net_state(void)
{
    return s_state;
}

const char *app_net_state_text(void)
{
    switch (s_state) {
    case APP_NET_DISABLED:   return "Not configured";
    case APP_NET_CONNECTING: return "Connecting";
    case APP_NET_CONNECTED:  return "Connected";
    case APP_NET_SYNCED:     return "Connected";
    default:                 return "Check password";
    }
}

bool app_net_time_valid(void)
{
    return s_time_valid;
}

const char *app_net_ip(void)
{
    return s_ip;
}

const char *app_net_ssid(void)
{
    return settings()->wifi_ssid;
}
