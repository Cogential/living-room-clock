#include "app_settings.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bsp/display.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif
/* No location: weather and sun are simply not fetched until one is set. */
#ifndef CLOCK_LATITUDE
#define CLOCK_LATITUDE  0.0f
#define CLOCK_LONGITUDE 0.0f
#define CLOCK_PLACE     ""
#endif

static const char *TAG = "settings";

#define NVS_NAMESPACE "clock"
#define NVS_KEY_BLOB  "settings"

/*
 * Wi-Fi credentials live outside the versioned blob.
 *
 * Bumping SETTINGS_VERSION discards the blob, which is the right thing for
 * preferences whose meaning has changed - but it also threw away the network
 * password, and that is the one setting that costs real effort to re-enter on
 * a touchscreen. Twice was enough. Everything else can go back to defaults.
 */
#define NVS_KEY_SSID  "wifi_ssid"
#define NVS_KEY_PASS  "wifi_pass"

/* Bump when the struct layout changes so stale blobs are discarded rather than
 * reinterpreted. */
/* 2: face stored by name rather than list index.
 * 3: added location for the data feeds. */
#define SETTINGS_VERSION 3

const timezone_entry_t SETTINGS_TIMEZONES[] = {
    { "Toronto / New York", "EST5EDT,M3.2.0,M11.1.0" },
    { "Chicago",            "CST6CDT,M3.2.0,M11.1.0" },
    { "Denver",             "MST7MDT,M3.2.0,M11.1.0" },
    { "Los Angeles",        "PST8PDT,M3.2.0,M11.1.0" },
    { "Halifax",            "AST4ADT,M3.2.0,M11.1.0" },
    { "St John's",          "NST3:30NDT,M3.2.0,M11.1.0" },
    { "London",             "GMT0BST,M3.5.0/1,M10.5.0" },
    { "Central Europe",     "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Athens / Helsinki",  "EET-2EEST,M3.5.0/3,M10.5.0/4" },
    { "Mumbai",             "IST-5:30" },
    { "Beijing",            "CST-8" },
    { "Tokyo",              "JST-9" },
    { "Sydney",             "AEST-10AEDT,M10.1.0,M4.1.0/3" },
    { "UTC",                "UTC0" },
};
const int SETTINGS_TIMEZONE_COUNT =
    (int)(sizeof(SETTINGS_TIMEZONES) / sizeof(SETTINGS_TIMEZONES[0]));

typedef struct {
    uint32_t   version;
    settings_t s;
} stored_t;

/*
 * Version 2's layout, kept so an existing blob can be migrated rather than
 * discarded. Identical to settings_t before latitude/longitude/place were
 * inserted ahead of the credentials - which is exactly why the offsets moved
 * and a plain reinterpret would read nonsense.
 *
 * Only the credentials are carried over, and only when they look sane: a
 * mis-guessed layout should end up as defaults, never as garbage on screen.
 */
typedef struct {
    uint8_t brightness;
    bool    night_enabled;
    uint8_t night_brightness;
    uint8_t night_start_hour;
    uint8_t night_end_hour;
    bool    hour_24;
    char    timezone[SETTINGS_TZ_LEN];
    int     tz_index;
    char    face_name[24];
    char    wifi_ssid[SETTINGS_SSID_LEN + 1];
    char    wifi_pass[SETTINGS_PASS_LEN + 1];
} settings_v2_t;

typedef struct {
    uint32_t      version;
    settings_v2_t s;
} stored_v2_t;

static bool plausible_ssid(const char *s)
{
    size_t len = strnlen(s, SETTINGS_SSID_LEN + 1);
    if (len == 0 || len > SETTINGS_SSID_LEN) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if ((unsigned char)s[i] < 0x20 || (unsigned char)s[i] > 0x7E) {
            return false;
        }
    }
    return true;
}

static settings_t s_settings;

static void load_defaults(void)
{
    memset(&s_settings, 0, sizeof(s_settings));

    s_settings.brightness = 85;
    s_settings.night_enabled = true;
    s_settings.night_brightness = 20;
    s_settings.night_start_hour = 22;
    s_settings.night_end_hour = 7;
    s_settings.hour_24 = CONFIG_CLOCK_24_HOUR ? true : false;
    s_settings.face_name[0] = '\0';  /* unset: ui.c picks the default face */
    s_settings.tz_index = -1;
    /* From secrets.h, so a public build carries nobody's home town. */
    s_settings.latitude = CLOCK_LATITUDE;
    s_settings.longitude = CLOCK_LONGITUDE;
    strlcpy(s_settings.place, CLOCK_PLACE, sizeof(s_settings.place));

    strlcpy(s_settings.timezone, CONFIG_CLOCK_TIMEZONE, sizeof(s_settings.timezone));
    strlcpy(s_settings.wifi_ssid, CONFIG_CLOCK_WIFI_SSID, sizeof(s_settings.wifi_ssid));
    strlcpy(s_settings.wifi_pass, CONFIG_CLOCK_WIFI_PASSWORD, sizeof(s_settings.wifi_pass));

    /* Match the build-time timezone to a table row so the picker opens on it. */
    for (int i = 0; i < SETTINGS_TIMEZONE_COUNT; i++) {
        if (strcmp(SETTINGS_TIMEZONES[i].posix, s_settings.timezone) == 0) {
            s_settings.tz_index = i;
            break;
        }
    }
}

void settings_init(void)
{
    load_defaults();

    /*
     * NVS has to be up before anything opens it, and settings is the first
     * consumer. Previously this lived in app_net_start(), which runs *after*
     * settings_init(): every boot the read failed, defaults were used, and
     * saved Wi-Fi credentials looked like they had been erased even though
     * they were sitting in flash the whole time.
     */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "nvs partition unusable, erasing");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "no stored settings, using defaults");
        return;
    }

    stored_t stored;
    size_t len = sizeof(stored);
    err = nvs_get_blob(h, NVS_KEY_BLOB, &stored, &len);
    nvs_close(h);

    if (err == ESP_OK && len == sizeof(stored) && stored.version == SETTINGS_VERSION) {
        s_settings = stored.s;
        ESP_LOGI(TAG, "settings loaded");
    } else if (err == ESP_OK) {
        ESP_LOGW(TAG, "stored settings incompatible, using defaults");

        /* Rescue the credentials from a version 2 blob before letting go. */
        stored_v2_t old;
        size_t old_len = sizeof(old);
        if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
            if (nvs_get_blob(h, NVS_KEY_BLOB, &old, &old_len) == ESP_OK &&
                old_len == sizeof(old) && old.version == 2 &&
                plausible_ssid(old.s.wifi_ssid)) {
                strlcpy(s_settings.wifi_ssid, old.s.wifi_ssid,
                        sizeof(s_settings.wifi_ssid));
                strlcpy(s_settings.wifi_pass, old.s.wifi_pass,
                        sizeof(s_settings.wifi_pass));
                ESP_LOGI(TAG, "migrated wifi credentials from v2");
            }
            nvs_close(h);
        }
    }

    /* Overlay the separately-stored credentials, so they survive the above. */
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
        size_t n = sizeof(s_settings.wifi_ssid);
        if (nvs_get_str(h, NVS_KEY_SSID, s_settings.wifi_ssid, &n) == ESP_OK) {
            n = sizeof(s_settings.wifi_pass);
            if (nvs_get_str(h, NVS_KEY_PASS, s_settings.wifi_pass, &n) != ESP_OK) {
                s_settings.wifi_pass[0] = '\0';
            }
            ESP_LOGI(TAG, "wifi credentials restored for \"%s\"", s_settings.wifi_ssid);
        }
        nvs_close(h);
    }
}

settings_t *settings(void)
{
    return &s_settings;
}

void settings_save(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGE(TAG, "cannot open nvs for write");
        return;
    }

    stored_t stored = { .version = SETTINGS_VERSION, .s = s_settings };
    esp_err_t err = nvs_set_blob(h, NVS_KEY_BLOB, &stored, sizeof(stored));

    /* Written again on their own keys so a future version bump cannot lose
     * them; see the note by NVS_KEY_SSID. */
    if (err == ESP_OK) {
        nvs_set_str(h, NVS_KEY_SSID, s_settings.wifi_ssid);
        nvs_set_str(h, NVS_KEY_PASS, s_settings.wifi_pass);
        err = nvs_commit(h);
    }
    nvs_close(h);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(err));
    }
}

void settings_apply_timezone(void)
{
    setenv("TZ", s_settings.timezone, 1);
    tzset();
}

bool settings_is_night(int hour)
{
    /* A negative hour means "no schedule" - used while previewing the day
     * brightness from the settings slider. Without this the wrapping window
     * below would treat -1 as being before the morning cutoff and dim. */
    if (!s_settings.night_enabled || hour < 0) {
        return false;
    }

    int start = s_settings.night_start_hour;
    int end = s_settings.night_end_hour;

    if (start == end) {
        return false;
    }
    if (start < end) {
        return hour >= start && hour < end;
    }
    /* Window wraps past midnight, e.g. 22:00 -> 07:00. */
    return hour >= start || hour < end;
}

void settings_apply_brightness(int current_hour)
{
    uint8_t level = settings_is_night(current_hour) ? s_settings.night_brightness
                                                    : s_settings.brightness;
    if (level < BRIGHTNESS_MIN) {
        level = BRIGHTNESS_MIN;
    }
    if (level > BRIGHTNESS_MAX) {
        level = BRIGHTNESS_MAX;
    }
    bsp_display_brightness_set(level);
}
