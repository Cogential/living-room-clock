#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * BLE central for the clock.
 *
 * The ESP32-C6 co-processor is BLE-only: it has no Classic BR/EDR radio, so
 * audio profiles (A2DP) are impossible on this board no matter the firmware.
 * This is for sensors, buttons and remotes.
 */

#define BLE_MAX_DEVICES 24
#define BLE_NAME_LEN    31

typedef struct {
    uint8_t  addr[6];
    uint8_t  addr_type;
    char     name[BLE_NAME_LEN + 1];
    int8_t   rssi;
    bool     connectable;
    uint32_t last_seen_ms;
} ble_device_t;

typedef enum {
    BLE_STATE_OFF = 0,      /* stack not started */
    BLE_STATE_STARTING,     /* waiting for the controller to sync */
    BLE_STATE_READY,        /* idle, usable */
    BLE_STATE_SCANNING,
    BLE_STATE_CONNECTING,
    BLE_STATE_CONNECTED,
    BLE_STATE_UNAVAILABLE,  /* no controller: C6 firmware without BT */
} ble_state_t;

/** Start the NimBLE host. Returns immediately; poll app_ble_state(). */
void app_ble_start(void);

ble_state_t app_ble_state(void);

/** Human-readable one-liner for the settings UI. */
const char *app_ble_state_text(void);

void app_ble_scan_start(void);
void app_ble_scan_stop(void);

/**
 * Copy the current discovery results, strongest signal first.
 * Safe to call from the UI task; returns the number written.
 */
int app_ble_get_devices(ble_device_t *out, int max);

/** Number of devices discovered so far. */
int app_ble_device_count(void);

esp_err_t app_ble_connect(const uint8_t addr[6], uint8_t addr_type);
void app_ble_disconnect(void);

/** Name of the connected (or connecting) peer, empty string if none. */
const char *app_ble_peer_name(void);

/** Remembered peer from NVS, reconnected automatically at boot. */
bool app_ble_has_saved_peer(void);
void app_ble_forget_peer(void);

#ifdef __cplusplus
}
#endif
