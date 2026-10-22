#include "net/app_api.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app_settings.h"
#include "cJSON.h"
#include "esp_attr.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "net/app_net.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef CLOCK_API_BASE
#define CLOCK_API_BASE ""
#endif
#ifndef CLOCK_API_TOKEN
#define CLOCK_API_TOKEN ""
#endif

static const char *TAG = "api";

/* No rate-limit headers are published, so these are deliberately unhurried.
 * None of this data is worth hammering a shared gateway for. */
#define WEATHER_PERIOD_S  (10 * 60)
#define NEWS_PERIOD_S     (15 * 60)
#define MARKETS_PERIOD_S  (10 * 60)
#define HN_PERIOD_S       (15 * 60)
#define SUN_PERIOD_S      (6 * 60 * 60)

/* Bodies land in PSRAM: news in particular runs to tens of kilobytes and
 * internal RAM is the scarce pool on this board. The buffer only lives for one
 * request, so the cap is generous; api_get() logs if a body still exceeds it. */
#define RESPONSE_MAX 65536

static const char *WATCHLIST[] = { "AAPL", "MSFT", "NVDA" };
#define WATCHLIST_COUNT ((int)(sizeof(WATCHLIST) / sizeof(WATCHLIST[0])))

static SemaphoreHandle_t s_lock;
static api_weather_t s_weather;
/* Headline structs live in PSRAM: with URLs they are ~2 KB apiece, and internal
 * RAM is the budget Wi-Fi, BLE and USB host all compete for. */
EXT_RAM_BSS_ATTR static api_news_t s_news;
EXT_RAM_BSS_ATTR static api_hn_t s_hn;
static api_markets_t s_markets;
static volatile uint32_t s_generation;
static const char *s_status = "Idle";

bool app_api_configured(void)
{
    return CLOCK_API_BASE[0] != '\0';
}

uint32_t app_api_generation(void)
{
    return s_generation;
}

const char *app_api_status(void)
{
    return s_status;
}

/* ------------------------------------------------------------------ fetch -- */

/* Returns a NUL-terminated body the caller frees, or NULL. */
static char *api_get(const char *path)
{
    char url[256];
    snprintf(url, sizeof(url), "%s%s", CLOCK_API_BASE, path);

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 15000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 2048,
        .buffer_size_tx = 1024,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return NULL;
    }

    if (CLOCK_API_TOKEN[0] != '\0') {
        char auth[128];
        snprintf(auth, sizeof(auth), "Bearer %s", CLOCK_API_TOKEN);
        esp_http_client_set_header(client, "Authorization", auth);
    }

    char *body = NULL;
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "open %s failed: %s", path, esp_err_to_name(err));
        goto done;
    }

    esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGW(TAG, "%s -> HTTP %d", path, status);
        goto done;
    }

    body = heap_caps_malloc(RESPONSE_MAX, MALLOC_CAP_SPIRAM);
    if (body == NULL) {
        goto done;
    }

    int total = 0;
    while (total < RESPONSE_MAX - 1) {
        int n = esp_http_client_read(client, body + total, RESPONSE_MAX - 1 - total);
        if (n <= 0) {
            break;
        }
        total += n;
    }
    body[total] = '\0';

    /* A body cut off at the cap would only fail in cJSON_Parse as an anonymous
     * "fetch failed" and retry forever, so say why and give up on it. */
    if (total == RESPONSE_MAX - 1 && !esp_http_client_is_complete_data_received(client)) {
        ESP_LOGE(TAG, "%s: response larger than RESPONSE_MAX (%d bytes), dropped",
                 path, RESPONSE_MAX);
        heap_caps_free(body);
        body = NULL;
    } else if (total == 0) {
        heap_caps_free(body);
        body = NULL;
    }

done:
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return body;
}

/* --------------------------------------------------------------- helpers -- */

static void copy_str(char *dst, size_t dst_len, const cJSON *obj, const char *key)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(v) && v->valuestring) {
        strlcpy(dst, v->valuestring, dst_len);
    } else {
        dst[0] = '\0';
    }
}

/* All or nothing: a truncated URL is worse than none. */
static void copy_url(char *dst, size_t dst_len, const cJSON *obj, const char *key)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(v) && v->valuestring && strlen(v->valuestring) < dst_len) {
        strlcpy(dst, v->valuestring, dst_len);
    } else {
        dst[0] = '\0';
    }
}

static int get_int(const cJSON *obj, const char *key, int fallback)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(v) ? (int)(v->valuedouble + (v->valuedouble < 0 ? -0.5 : 0.5))
                             : fallback;
}

static float get_float(const cJSON *obj, const char *key, float fallback)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(v) ? (float)v->valuedouble : fallback;
}

/*
 * Days since 1970-01-01 for a civil date (Howard Hinnant's algorithm).
 *
 * timegm() is not available here, and the mktime-with-TZ-set-to-UTC trick is
 * neither thread-safe nor reversible mid-run, so the arithmetic is done
 * directly.
 */
static int64_t days_from_civil(int y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153u * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

static int64_t utc_epoch(const struct tm *tm)
{
    int64_t days = days_from_civil(tm->tm_year + 1900, (unsigned)(tm->tm_mon + 1),
                                   (unsigned)tm->tm_mday);
    return days * 86400 + tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
}

/*
 * "2026-08-31T23:56:34+00:00" -> local "HH:MM".
 *
 * The gateway documents these as UTC, so they are parsed as UTC and converted
 * with the configured timezone rather than shown raw.
 */
static void iso_utc_to_local_hhmm(const char *iso, char *out, size_t out_len)
{
    out[0] = '\0';
    if (iso == NULL || strlen(iso) < 19) {
        return;
    }

    struct tm tm = { 0 };
    if (sscanf(iso, "%4d-%2d-%2dT%2d:%2d:%2d",
               &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) {
        return;
    }
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;

    time_t utc = (time_t)utc_epoch(&tm);
    struct tm local;
    localtime_r(&utc, &local);
    strftime(out, out_len, "%H:%M", &local);
}

/* ---------------------------------------------------------------- parsers -- */

static bool fetch_weather(void)
{
    const settings_t *cfg = settings();
    if (cfg->place[0] == '\0') {
        return false;  /* no location set (CLOCK_PLACE in secrets.h) */
    }
    char path[128];
    snprintf(path, sizeof(path), "/weather?lat=%.4f&lon=%.4f", cfg->latitude, cfg->longitude);

    char *body = api_get(path);
    if (body == NULL) {
        return false;
    }

    cJSON *root = cJSON_Parse(body);
    heap_caps_free(body);
    if (root == NULL) {
        return false;
    }

    api_weather_t w = { 0 };
    w.temp = get_int(root, "temp", 0);
    w.feels = get_int(root, "feels", 0);
    w.humidity = get_int(root, "humidity", 0);
    w.wind = get_int(root, "wind", 0);
    w.aqi = get_int(root, "aqi", -1);
    w.hi = get_int(root, "hi", 0);
    w.lo = get_int(root, "lo", 0);
    copy_str(w.desc, sizeof(w.desc), root, "desc");

    const cJSON *fc = cJSON_GetObjectItemCaseSensitive(root, "forecast");
    int i = 0;
    const cJSON *day = NULL;
    cJSON_ArrayForEach(day, fc) {
        if (i >= 3) {
            break;
        }
        copy_str(w.forecast[i].day, sizeof(w.forecast[i].day), day, "d");
        w.forecast[i].hi = get_int(day, "hi", 0);
        w.forecast[i].lo = get_int(day, "lo", 0);
        i++;
    }

    cJSON_Delete(root);

    xSemaphoreTake(s_lock, portMAX_DELAY);
    /* Sun times come from a different endpoint on a slower cycle; keep them. */
    strlcpy(w.sunrise, s_weather.sunrise, sizeof(w.sunrise));
    strlcpy(w.sunset, s_weather.sunset, sizeof(w.sunset));
    w.ok = true;
    w.fetched = (uint32_t)time(NULL);
    s_weather = w;
    xSemaphoreGive(s_lock);
    return true;
}

static bool fetch_sun(void)
{
    const settings_t *cfg = settings();
    if (cfg->place[0] == '\0') {
        return false;  /* no location set (CLOCK_PLACE in secrets.h) */
    }
    char path[128];
    snprintf(path, sizeof(path), "/space/sun?lat=%.4f&lon=%.4f", cfg->latitude, cfg->longitude);

    char *body = api_get(path);
    if (body == NULL) {
        return false;
    }

    cJSON *root = cJSON_Parse(body);
    heap_caps_free(body);
    if (root == NULL) {
        return false;
    }

    char rise[6] = "", set[6] = "";
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, "sunrise");
    if (cJSON_IsString(v)) {
        iso_utc_to_local_hhmm(v->valuestring, rise, sizeof(rise));
    }
    v = cJSON_GetObjectItemCaseSensitive(root, "sunset");
    if (cJSON_IsString(v)) {
        iso_utc_to_local_hhmm(v->valuestring, set, sizeof(set));
    }
    cJSON_Delete(root);

    xSemaphoreTake(s_lock, portMAX_DELAY);
    strlcpy(s_weather.sunrise, rise, sizeof(s_weather.sunrise));
    strlcpy(s_weather.sunset, set, sizeof(s_weather.sunset));
    xSemaphoreGive(s_lock);
    return true;
}

static bool fetch_news(void)
{
    char *body = api_get("/news");
    if (body == NULL) {
        return false;
    }

    cJSON *root = cJSON_Parse(body);
    heap_caps_free(body);
    if (root == NULL) {
        return false;
    }

    /* Static rather than on the task stack: with URLs this is ~2.3 KB, and only
     * the api task ever runs this. In PSRAM, like s_news. */
    EXT_RAM_BSS_ATTR static api_news_t n;
    memset(&n, 0, sizeof(n));
    const cJSON *items = cJSON_GetObjectItemCaseSensitive(root, "items");
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, items) {
        if (n.count >= API_NEWS_MAX) {
            break;
        }
        copy_str(n.items[n.count].title, sizeof(n.items[n.count].title), item, "title");
        copy_str(n.items[n.count].source, sizeof(n.items[n.count].source), item, "source");
        copy_url(n.items[n.count].url, sizeof(n.items[n.count].url), item, "link");
        if (n.items[n.count].title[0]) {
            n.count++;
        }
    }
    cJSON_Delete(root);

    if (n.count == 0) {
        return false;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    n.ok = true;
    n.fetched = (uint32_t)time(NULL);
    s_news = n;
    xSemaphoreGive(s_lock);
    return true;
}

static bool fetch_hn(void)
{
    /* The gateway ignores ?limit and always returns 20; at ~6 KB that is well
     * inside RESPONSE_MAX. API_HN_MAX keeps all of them. */
    char *body = api_get("/hn/top");
    if (body == NULL) {
        return false;
    }

    cJSON *root = cJSON_Parse(body);
    heap_caps_free(body);
    if (root == NULL) {
        return false;
    }

    EXT_RAM_BSS_ATTR static api_hn_t h;  /* see fetch_news */
    memset(&h, 0, sizeof(h));
    const cJSON *items = cJSON_GetObjectItemCaseSensitive(root, "items");
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, items) {
        if (h.count >= API_HN_MAX) {
            break;
        }
        copy_str(h.items[h.count].title, sizeof(h.items[h.count].title), item, "title");
        /* "job" posts carry a null comment count, so -1 means "do not show". */
        h.items[h.count].score    = get_int(item, "score", -1);
        h.items[h.count].comments = get_int(item, "comments", -1);
        copy_url(h.items[h.count].url, sizeof(h.items[h.count].url), item, "url");
        if (h.items[h.count].url[0] == '\0') {
            copy_url(h.items[h.count].url, sizeof(h.items[h.count].url), item, "hn_url");
        }
        if (h.items[h.count].title[0]) {
            h.count++;
        }
    }
    cJSON_Delete(root);

    if (h.count == 0) {
        return false;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    h.ok = true;
    h.fetched = (uint32_t)time(NULL);
    s_hn = h;
    xSemaphoreGive(s_lock);
    return true;
}

static bool fetch_markets(void)
{
    api_markets_t m = { 0 };

    for (int i = 0; i < WATCHLIST_COUNT && m.count < API_QUOTE_MAX; i++) {
        char path[96];
        snprintf(path, sizeof(path), "/finance/quote?symbol=%s", WATCHLIST[i]);

        char *body = api_get(path);
        if (body == NULL) {
            continue;
        }
        cJSON *root = cJSON_Parse(body);
        heap_caps_free(body);
        if (root == NULL) {
            continue;
        }

        copy_str(m.quotes[m.count].symbol, sizeof(m.quotes[m.count].symbol), root, "symbol");
        m.quotes[m.count].price = get_float(root, "price", 0.0f);
        m.quotes[m.count].change_pct = get_float(root, "changePct", 0.0f);
        if (m.quotes[m.count].symbol[0]) {
            m.count++;
        }
        cJSON_Delete(root);
    }

    char *body = api_get("/finance/fear-greed");
    if (body) {
        cJSON *root = cJSON_Parse(body);
        heap_caps_free(body);
        if (root) {
            m.fear_greed = get_float(root, "score", -1.0f);
            copy_str(m.fear_greed_rating, sizeof(m.fear_greed_rating), root, "rating");
            cJSON_Delete(root);
        }
    }

    body = api_get("/finance/crypto?ids=bitcoin");
    if (body) {
        cJSON *root = cJSON_Parse(body);
        heap_caps_free(body);
        if (root) {
            const cJSON *prices = cJSON_GetObjectItemCaseSensitive(root, "prices");
            const cJSON *btc = cJSON_GetObjectItemCaseSensitive(prices, "bitcoin");
            if (btc) {
                m.btc_usd = get_float(btc, "usd", 0.0f);
                m.btc_change_pct = get_float(btc, "usd_24h_change", 0.0f);
            }
            cJSON_Delete(root);
        }
    }

    if (m.count == 0 && m.btc_usd == 0.0f) {
        return false;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    m.ok = true;
    m.fetched = (uint32_t)time(NULL);
    s_markets = m;
    xSemaphoreGive(s_lock);
    return true;
}

/* ------------------------------------------------------------------ task -- */

static void api_task(void *arg)
{
    (void)arg;

    uint32_t next_weather = 0, next_news = 0, next_markets = 0, next_sun = 0;
    uint32_t next_hn = 0;

    while (true) {
        /* Nothing to do until there is a network and a real clock: the fetch
         * schedule is keyed off wall time. */
        app_net_state_t net = app_net_state();
        bool online = net == APP_NET_CONNECTED || net == APP_NET_SYNCED;
        if (!online || time(NULL) < 1700000000) {
            s_status = "Waiting for network";
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        uint32_t now = (uint32_t)time(NULL);
        bool fetched = false;

        if (now >= next_weather) {
            s_status = "Updating weather";
            fetched = fetch_weather();
            next_weather = now + (fetched ? WEATHER_PERIOD_S : 60);
        } else if (now >= next_sun) {
            s_status = "Updating sun";
            fetched = fetch_sun();
            next_sun = now + (fetched ? SUN_PERIOD_S : 300);
        } else if (now >= next_news) {
            s_status = "Updating news";
            fetched = fetch_news();
            next_news = now + (fetched ? NEWS_PERIOD_S : 60);
        } else if (now >= next_hn) {
            s_status = "Updating Hacker News";
            fetched = fetch_hn();
            next_hn = now + (fetched ? HN_PERIOD_S : 60);
        } else if (now >= next_markets) {
            s_status = "Updating markets";
            fetched = fetch_markets();
            next_markets = now + (fetched ? MARKETS_PERIOD_S : 60);
        }

        /* Only new data counts: every bump makes the HUD redraw and restart
         * its headline rotation, so a failing gateway must not bump it. */
        if (fetched) {
            s_generation++;
        }
        s_status = "Idle";

        /* One request at a time, spaced out: the UI is never waiting on this. */
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_api_start(void)
{
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        return;
    }

    if (!app_api_configured()) {
        ESP_LOGW(TAG, "no gateway configured, see main/secrets.h.example");
        s_status = "No gateway";
        return;
    }

    /* PSRAM stack: TLS needs a deep one and internal RAM is the scarce pool. */
    xTaskCreate(api_task, "api", 8192, NULL, 4, NULL);
}

/* --------------------------------------------------------------- getters -- */

bool app_api_weather(api_weather_t *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_weather;
    xSemaphoreGive(s_lock);
    return out->ok;
}

bool app_api_news(api_news_t *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_news;
    xSemaphoreGive(s_lock);
    return out->ok;
}

bool app_api_hn(api_hn_t *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_hn;
    xSemaphoreGive(s_lock);
    return out->ok;
}

bool app_api_markets(api_markets_t *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_markets;
    xSemaphoreGive(s_lock);
    return out->ok;
}
