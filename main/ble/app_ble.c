#include "ble/app_ble.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

static const char *TAG = "ble";

#define NVS_NAMESPACE  "clock"
#define NVS_KEY_PEER   "ble_peer"

/* The controller lives on the C6 across a hosted link. If it never syncs, the
 * slave firmware was built without BT and there is nothing we can do host-side,
 * so give up rather than sit in "starting" forever. */
#define SYNC_TIMEOUT_MS 8000

typedef struct {
    uint8_t addr[6];
    uint8_t addr_type;
} saved_peer_t;

static volatile ble_state_t s_state = BLE_STATE_OFF;
static SemaphoreHandle_t s_lock;

static ble_device_t s_devices[BLE_MAX_DEVICES];
static int s_device_count;

static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static char s_peer_name[BLE_NAME_LEN + 1];
static saved_peer_t s_saved_peer;
static bool s_has_saved_peer;
static uint8_t s_own_addr_type;

static int gap_event(struct ble_gap_event *event, void *arg);

/* ------------------------------------------------------------------ store -- */

static void load_saved_peer(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    size_t len = sizeof(s_saved_peer);
    s_has_saved_peer = (nvs_get_blob(h, NVS_KEY_PEER, &s_saved_peer, &len) == ESP_OK &&
                        len == sizeof(s_saved_peer));
    nvs_close(h);
}

static void store_saved_peer(const uint8_t addr[6], uint8_t addr_type)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    saved_peer_t peer;
    memcpy(peer.addr, addr, 6);
    peer.addr_type = addr_type;
    nvs_set_blob(h, NVS_KEY_PEER, &peer, sizeof(peer));
    nvs_commit(h);
    nvs_close(h);

    s_saved_peer = peer;
    s_has_saved_peer = true;
}

bool app_ble_has_saved_peer(void)
{
    return s_has_saved_peer;
}

void app_ble_forget_peer(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_key(h, NVS_KEY_PEER);
        nvs_commit(h);
        nvs_close(h);
    }
    s_has_saved_peer = false;
    app_ble_disconnect();
}

/* ------------------------------------------------------------- discovery -- */

/* Advertisement names arrive as non-terminated fields inside the payload. */
static void extract_name(const struct ble_hs_adv_fields *fields, char *out, size_t out_len)
{
    out[0] = '\0';
    if (fields->name != NULL && fields->name_len > 0) {
        size_t n = fields->name_len;
        if (n > out_len - 1) {
            n = out_len - 1;
        }
        memcpy(out, fields->name, n);
        out[n] = '\0';
    }
}

static void record_device(const struct ble_gap_disc_desc *disc)
{
    struct ble_hs_adv_fields fields;
    if (ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data) != 0) {
        memset(&fields, 0, sizeof(fields));
    }

    char name[BLE_NAME_LEN + 1];
    extract_name(&fields, name, sizeof(name));

    xSemaphoreTake(s_lock, portMAX_DELAY);

    ble_device_t *slot = NULL;
    for (int i = 0; i < s_device_count; i++) {
        if (memcmp(s_devices[i].addr, disc->addr.val, 6) == 0) {
            slot = &s_devices[i];
            break;
        }
    }

    if (slot == NULL) {
        if (s_device_count < BLE_MAX_DEVICES) {
            slot = &s_devices[s_device_count++];
            memset(slot, 0, sizeof(*slot));
        } else {
            /* Full: recycle whichever entry has the weakest signal, so a strong
             * nearby device can still displace distant noise. */
            int weakest = 0;
            for (int i = 1; i < s_device_count; i++) {
                if (s_devices[i].rssi < s_devices[weakest].rssi) {
                    weakest = i;
                }
            }
            if (s_devices[weakest].rssi >= disc->rssi) {
                xSemaphoreGive(s_lock);
                return;
            }
            slot = &s_devices[weakest];
            memset(slot, 0, sizeof(*slot));
        }
        memcpy(slot->addr, disc->addr.val, 6);
        slot->addr_type = disc->addr.type;
    }

    slot->rssi = disc->rssi;
    slot->last_seen_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    slot->connectable = (disc->event_type == BLE_HCI_ADV_RPT_EVTYPE_ADV_IND ||
                         disc->event_type == BLE_HCI_ADV_RPT_EVTYPE_DIR_IND);
    /* Scan responses often carry the name when the advert did not. */
    if (name[0] != '\0') {
        strlcpy(slot->name, name, sizeof(slot->name));
    }

    xSemaphoreGive(s_lock);
}

int app_ble_get_devices(ble_device_t *out, int max)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);

    int n = s_device_count < max ? s_device_count : max;
    memcpy(out, s_devices, n * sizeof(ble_device_t));

    xSemaphoreGive(s_lock);

    /* Strongest first: short list, so an insertion sort is plenty. */
    for (int i = 1; i < n; i++) {
        ble_device_t key = out[i];
        int j = i - 1;
        while (j >= 0 && out[j].rssi < key.rssi) {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = key;
    }
    return n;
}

int app_ble_device_count(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int n = s_device_count;
    xSemaphoreGive(s_lock);
    return n;
}

void app_ble_scan_start(void)
{
    if (s_state != BLE_STATE_READY) {
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_device_count = 0;
    xSemaphoreGive(s_lock);

    struct ble_gap_disc_params params = {
        .itvl = BLE_GAP_SCAN_ITVL_MS(160),
        .window = BLE_GAP_SCAN_WIN_MS(80),
        .filter_policy = BLE_HCI_SCAN_FILT_NO_WL,
        .limited = 0,
        /* Active scanning so we also get scan responses, which is where most
         * devices put their name. */
        .passive = 0,
        .filter_duplicates = 0,
    };

    int rc = ble_gap_disc(s_own_addr_type, BLE_HS_FOREVER, &params, gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "scan start failed (%d)", rc);
        return;
    }
    s_state = BLE_STATE_SCANNING;
}

void app_ble_scan_stop(void)
{
    if (s_state == BLE_STATE_SCANNING) {
        ble_gap_disc_cancel();
        s_state = BLE_STATE_READY;
    }
}

/* ------------------------------------------------------------ connection -- */

esp_err_t app_ble_connect(const uint8_t addr[6], uint8_t addr_type)
{
    if (s_state == BLE_STATE_SCANNING) {
        app_ble_scan_stop();
    }
    if (s_state != BLE_STATE_READY) {
        return ESP_ERR_INVALID_STATE;
    }

    ble_addr_t peer = { .type = addr_type };
    memcpy(peer.val, addr, 6);

    /* Remember the name we discovered so the UI has something to show while the
     * link is still coming up. */
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_peer_name[0] = '\0';
    for (int i = 0; i < s_device_count; i++) {
        if (memcmp(s_devices[i].addr, addr, 6) == 0) {
            strlcpy(s_peer_name, s_devices[i].name, sizeof(s_peer_name));
            break;
        }
    }
    xSemaphoreGive(s_lock);

    int rc = ble_gap_connect(s_own_addr_type, &peer, 10000, NULL, gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "connect failed (%d)", rc);
        return ESP_FAIL;
    }

    s_state = BLE_STATE_CONNECTING;
    store_saved_peer(addr, addr_type);
    return ESP_OK;
}

void app_ble_disconnect(void)
{
    if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
}

const char *app_ble_peer_name(void)
{
    return s_peer_name;
}

/* ---------------------------------------------------------------- events -- */

static int gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        record_device(&event->disc);
        break;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        if (s_state == BLE_STATE_SCANNING) {
            s_state = BLE_STATE_READY;
        }
        break;

    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            s_state = BLE_STATE_CONNECTED;
            ESP_LOGI(TAG, "connected to %s", s_peer_name);
        } else {
            ESP_LOGW(TAG, "connect failed, status %d", event->connect.status);
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            s_state = BLE_STATE_READY;
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnected, reason %d", event->disconnect.reason);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_state = BLE_STATE_READY;
        break;

    default:
        break;
    }
    return 0;
}

/* ------------------------------------------------------------------ init -- */

static void on_sync(void)
{
    if (ble_hs_util_ensure_addr(0) != 0) {
        ESP_LOGE(TAG, "no usable bluetooth address");
        s_state = BLE_STATE_UNAVAILABLE;
        return;
    }
    if (ble_hs_id_infer_auto(0, &s_own_addr_type) != 0) {
        ESP_LOGE(TAG, "failed to infer address type");
        s_state = BLE_STATE_UNAVAILABLE;
        return;
    }

    s_state = BLE_STATE_READY;
    ESP_LOGI(TAG, "controller synced, ble ready");

    if (s_has_saved_peer) {
        app_ble_connect(s_saved_peer.addr, s_saved_peer.addr_type);
    }
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "controller reset, reason %d", reason);
    s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    s_state = BLE_STATE_STARTING;
}

static void host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void watchdog_task(void *param)
{
    vTaskDelay(pdMS_TO_TICKS(SYNC_TIMEOUT_MS));
    if (s_state == BLE_STATE_STARTING) {
        ESP_LOGW(TAG, "controller never synced - the C6 slave firmware "
                      "is probably built without bluetooth");
        s_state = BLE_STATE_UNAVAILABLE;
    }
    vTaskDelete(NULL);
}

void app_ble_start(void)
{
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        s_state = BLE_STATE_UNAVAILABLE;
        return;
    }

    load_saved_peer();

    s_state = BLE_STATE_STARTING;

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(err));
        s_state = BLE_STATE_UNAVAILABLE;
        return;
    }

    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    nimble_port_freertos_init(host_task);
    xTaskCreate(watchdog_task, "ble_wd", 2048, NULL, 3, NULL);
}

ble_state_t app_ble_state(void)
{
    return s_state;
}

const char *app_ble_state_text(void)
{
    switch (s_state) {
    case BLE_STATE_OFF:         return "Off";
    case BLE_STATE_STARTING:    return "Starting";
    case BLE_STATE_READY:       return "Ready";
    case BLE_STATE_SCANNING:    return "Scanning";
    case BLE_STATE_CONNECTING:  return "Connecting";
    case BLE_STATE_CONNECTED:   return s_peer_name[0] ? s_peer_name : "Connected";
    default:                    return "Unavailable";
    }
}
