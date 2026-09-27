#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SETTINGS_SSID_LEN 32
#define SETTINGS_PASS_LEN 64
#define SETTINGS_TZ_LEN   48

/* Below roughly 5% the backlight is off rather than dim, so clamp there. */
#define BRIGHTNESS_MIN 5
#define BRIGHTNESS_MAX 100

typedef struct {
    uint8_t brightness;
    bool    night_enabled;
    uint8_t night_brightness;
    uint8_t night_start_hour;   /* dim from this hour ... */
    uint8_t night_end_hour;     /* ... until this hour, wrapping past midnight */

    bool    hour_24;
    char    timezone[SETTINGS_TZ_LEN];  /* POSIX TZ string */
    int     tz_index;                   /* row in the timezone table, -1 if custom */

    /* Stored by name, not index: the face list gets reordered and extended, and
     * a positional index silently starts pointing at a different face. Empty
     * means "not chosen yet" and the UI picks its default. */
    char    face_name[24];

    /* Weather, sun and air quality all need an explicit point - the gateway
     * has no default of its own. */
    float   latitude;
    float   longitude;
    char    place[32];

    char    wifi_ssid[SETTINGS_SSID_LEN + 1];
    char    wifi_pass[SETTINGS_PASS_LEN + 1];
} settings_t;

typedef struct {
    const char *label;
    const char *posix;
} timezone_entry_t;

extern const timezone_entry_t SETTINGS_TIMEZONES[];
extern const int SETTINGS_TIMEZONE_COUNT;

/** Load from NVS, falling back to the Kconfig values on first boot. */
void settings_init(void);

/** The live settings. Mutate then call settings_save(). */
settings_t *settings(void);

void settings_save(void);

/** Apply the TZ string to the C library. */
void settings_apply_timezone(void);

/**
 * Push the right backlight level for the current hour.
 *
 * Cheap and idempotent - call it whenever brightness or the hour changes.
 * Pass a negative hour to ignore the night schedule and preview the day level.
 */
void settings_apply_brightness(int current_hour);

/** True when `hour` falls inside the configured night window. Negative: false. */
bool settings_is_night(int hour);

#ifdef __cplusplus
}
#endif
