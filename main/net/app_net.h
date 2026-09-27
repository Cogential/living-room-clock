#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NET_MAX_SCAN_RESULTS 20
#define NET_SSID_LEN         32

typedef enum {
    APP_NET_DISABLED = 0,  /* no SSID configured */
    APP_NET_CONNECTING,
    APP_NET_CONNECTED,     /* has IP, time not confirmed yet */
    APP_NET_SYNCED,        /* SNTP has set the clock at least once */
    APP_NET_FAILED,        /* join rejected, most likely a bad password */
} app_net_state_t;

typedef struct {
    char    ssid[NET_SSID_LEN + 1];
    int8_t  rssi;
    bool    secured;
} net_ap_t;

/**
 * Bring up Wi-Fi (via the onboard C6) and SNTP in the background.
 *
 * Returns immediately and never blocks the UI. The radio is started even with
 * no credentials so the settings screen can scan; it simply does not associate.
 */
void app_net_start(void);

app_net_state_t app_net_state(void);
const char *app_net_state_text(void);

/** True once SNTP has set the system clock at least once this boot. */
bool app_net_time_valid(void);

/** Dotted-quad address, or "-" when not connected. */
const char *app_net_ip(void);

/** SSID currently configured, empty string if none. */
const char *app_net_ssid(void);

/**
 * Kick off an access-point scan; results arrive asynchronously.
 *
 * A join still in progress is paused for the scan, because the driver cannot do
 * both at once - except shortly after app_net_connect(), when the request is
 * refused so the user's own join gets its chance. Returns false if no scan was
 * started.
 */
bool app_net_scan_start(void);
bool app_net_scanning(void);

/** Bumped each time a scan finishes, successful or not. */
uint32_t app_net_scan_generation(void);

/** Copy the latest scan results, strongest first. Returns the count written. */
int app_net_get_scan_results(net_ap_t *out, int max);

/** Store new credentials and (re)join. Persists them via app_settings. */
void app_net_connect(const char *ssid, const char *password);

/**
 * Stop and restart the radio.
 *
 * Wi-Fi holds DMA-capable internal buffers and every broadcast frame on the LAN
 * costs the SDIO transport another one. Doom needs that memory and does not
 * need the network, so it suspends it for the session.
 */
void app_net_suspend(void);
void app_net_resume(void);

/** Force an immediate SNTP resync. */
void app_net_sync_now(void);

#ifdef __cplusplus
}
#endif
