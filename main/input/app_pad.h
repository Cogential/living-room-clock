#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * USB gamepad input.
 *
 * The board's USB-A port is a real host, which is the only way a PlayStation
 * controller can talk to this hardware: the DualSense speaks Bluetooth Classic
 * (BR/EDR) and the ESP32-C6 is BLE-only, so wireless pairing is impossible
 * regardless of firmware. Plugged in by cable it is an ordinary HID gamepad.
 */

#define PAD_RAW_MAX  32
#define PAD_NAME_MAX 40

/* Face buttons keep their PlayStation names; the layout is what people know. */
#define PAD_BTN_SQUARE   (1u << 0)
#define PAD_BTN_CROSS    (1u << 1)
#define PAD_BTN_CIRCLE   (1u << 2)
#define PAD_BTN_TRIANGLE (1u << 3)
#define PAD_BTN_L1       (1u << 4)
#define PAD_BTN_R1       (1u << 5)
#define PAD_BTN_L2       (1u << 6)
#define PAD_BTN_R2       (1u << 7)
#define PAD_BTN_CREATE   (1u << 8)
#define PAD_BTN_OPTIONS  (1u << 9)
#define PAD_BTN_L3       (1u << 10)
#define PAD_BTN_R3       (1u << 11)
#define PAD_BTN_PS       (1u << 12)
#define PAD_BTN_TOUCHPAD (1u << 13)
#define PAD_BTN_MUTE     (1u << 14)

typedef enum {
    PAD_STATE_ABSENT = 0,   /* nothing plugged into the host port */
    PAD_STATE_UNSUPPORTED,  /* a HID device, but not one we know how to read */
    PAD_STATE_READY,
} pad_state_t;

typedef struct {
    float    lx, ly;     /* -1.0 .. 1.0, centred and deadzoned */
    float    rx, ry;
    uint8_t  l2, r2;     /* analogue triggers, 0..255 */
    uint32_t buttons;    /* PAD_BTN_* bitmask */
    int8_t   dpad;       /* -1 for centred, else 0..7 clockwise from up */
} pad_input_t;

/** Start the USB host and HID driver. Returns immediately. */
void app_pad_start(void);

pad_state_t app_pad_state(void);

/** Product string if the device gave one, else "VID:PID". */
const char *app_pad_name(void);

/** Latest decoded state. False when no supported pad is connected. */
bool app_pad_get(pad_input_t *out);

/**
 * Copy the last raw input report.
 *
 * Report layouts are taken from documentation rather than measured on the
 * device, so the settings page shows these bytes: if a button lands in an
 * unexpected bit, the raw view is what proves it.
 */
int app_pad_raw(uint8_t *out, int max);

#ifdef __cplusplus
}
#endif
