#include "input/app_pad.h"

#include <stdio.h>
#include <string.h>

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "usb/hid_host.h"

static const char *TAG = "pad";

#define VID_SONY        0x054C
#define PID_DUALSENSE   0x0CE6
#define PID_DUALSENSE_E 0x0DF2   /* DualSense Edge */

/* Sticks rest near centre but never exactly on it; below this they read zero. */
#define STICK_DEADZONE 0.08f

static volatile pad_state_t s_state = PAD_STATE_ABSENT;
static SemaphoreHandle_t s_lock;

static pad_input_t s_input;
static bool s_input_valid;
static uint8_t s_raw[PAD_RAW_MAX];
static int s_raw_len;
static char s_name[PAD_NAME_MAX] = "";
static hid_host_device_handle_t s_open_dev;
static bool s_is_dualsense;

/* ---------------------------------------------------------------- parsing -- */

static float axis(uint8_t v)
{
    /* 0..255 with 128 nominally centred. */
    float f = ((float)v - 127.5f) / 127.5f;
    if (f > -STICK_DEADZONE && f < STICK_DEADZONE) {
        return 0.0f;
    }
    return f < -1.0f ? -1.0f : (f > 1.0f ? 1.0f : f);
}

/*
 * DualSense USB input report, report ID 0x01:
 *   1..4  LX LY RX RY      5..6 L2 R2      7 counter
 *   8     dpad hat in the low nibble, face buttons in the high nibble
 *   9     L1 R1 L2 R2 Create Options L3 R3
 *   10    PS, touchpad click, mute
 */
static void parse_dualsense(const uint8_t *d, int len)
{
    if (len < 11 || d[0] != 0x01) {
        return;
    }

    pad_input_t in = { 0 };
    in.lx = axis(d[1]);
    in.ly = axis(d[2]);
    in.rx = axis(d[3]);
    in.ry = axis(d[4]);
    in.l2 = d[5];
    in.r2 = d[6];

    uint8_t hat = d[8] & 0x0F;
    in.dpad = (hat <= 7) ? (int8_t)hat : -1;

    if (d[8] & 0x10) in.buttons |= PAD_BTN_SQUARE;
    if (d[8] & 0x20) in.buttons |= PAD_BTN_CROSS;
    if (d[8] & 0x40) in.buttons |= PAD_BTN_CIRCLE;
    if (d[8] & 0x80) in.buttons |= PAD_BTN_TRIANGLE;

    if (d[9] & 0x01) in.buttons |= PAD_BTN_L1;
    if (d[9] & 0x02) in.buttons |= PAD_BTN_R1;
    if (d[9] & 0x04) in.buttons |= PAD_BTN_L2;
    if (d[9] & 0x08) in.buttons |= PAD_BTN_R2;
    if (d[9] & 0x10) in.buttons |= PAD_BTN_CREATE;
    if (d[9] & 0x20) in.buttons |= PAD_BTN_OPTIONS;
    if (d[9] & 0x40) in.buttons |= PAD_BTN_L3;
    if (d[9] & 0x80) in.buttons |= PAD_BTN_R3;

    if (d[10] & 0x01) in.buttons |= PAD_BTN_PS;
    if (d[10] & 0x02) in.buttons |= PAD_BTN_TOUCHPAD;
    if (d[10] & 0x04) in.buttons |= PAD_BTN_MUTE;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_input = in;
    s_input_valid = true;
    xSemaphoreGive(s_lock);
}

/* ----------------------------------------------------------------- events -- */

static void store_raw(const uint8_t *data, int len)
{
    int n = len < PAD_RAW_MAX ? len : PAD_RAW_MAX;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    memcpy(s_raw, data, n);
    s_raw_len = n;
    xSemaphoreGive(s_lock);
}

static void interface_cb(hid_host_device_handle_t dev,
                         const hid_host_interface_event_t event, void *arg)
{
    (void)arg;

    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT: {
        uint8_t data[64];
        size_t len = 0;
        if (hid_host_device_get_raw_input_report_data(dev, data, sizeof(data),
                                                      &len) != ESP_OK) {
            break;
        }
        store_raw(data, (int)len);
        if (s_is_dualsense) {
            parse_dualsense(data, (int)len);
        }
        break;
    }

    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "controller disconnected");
        hid_host_device_close(dev);
        s_open_dev = NULL;
        s_is_dualsense = false;
        s_state = PAD_STATE_ABSENT;
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_input_valid = false;
        s_raw_len = 0;
        s_name[0] = '\0';
        xSemaphoreGive(s_lock);
        break;

    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        ESP_LOGW(TAG, "transfer error");
        break;

    default:
        break;
    }
}

/* wchar_t descriptor strings are UTF-16-ish; take the low byte of each unit,
 * which is enough for the ASCII product names these devices report. */
static void narrow(const wchar_t *src, char *dst, size_t dst_len)
{
    size_t i = 0;
    for (; src[i] && i < dst_len - 1; i++) {
        wchar_t c = src[i];
        dst[i] = (c >= 32 && c < 127) ? (char)c : '?';
    }
    dst[i] = '\0';
}

static void driver_cb(hid_host_device_handle_t dev,
                      const hid_host_driver_event_t event, void *arg)
{
    (void)arg;

    if (event != HID_HOST_DRIVER_EVENT_CONNECTED) {
        return;
    }

    const hid_host_device_config_t cfg = {
        .callback = interface_cb,
        .callback_arg = NULL,
    };

    if (hid_host_device_open(dev, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "failed to open hid interface");
        return;
    }

    hid_host_dev_info_t info = { 0 };
    if (hid_host_get_device_info(dev, &info) == ESP_OK) {
        s_is_dualsense = (info.VID == VID_SONY &&
                          (info.PID == PID_DUALSENSE || info.PID == PID_DUALSENSE_E));

        xSemaphoreTake(s_lock, portMAX_DELAY);
        narrow(info.iProduct, s_name, sizeof(s_name));
        if (s_name[0] == '\0') {
            snprintf(s_name, sizeof(s_name), "%04X:%04X", info.VID, info.PID);
        }
        xSemaphoreGive(s_lock);

        ESP_LOGI(TAG, "connected %04X:%04X \"%s\"%s", info.VID, info.PID, s_name,
                 s_is_dualsense ? " (dualsense)" : "");
    } else {
        s_is_dualsense = false;
        strlcpy(s_name, "Unknown HID device", sizeof(s_name));
    }

    if (hid_host_device_start(dev) != ESP_OK) {
        ESP_LOGE(TAG, "failed to start hid interface");
        hid_host_device_close(dev);
        return;
    }

    s_open_dev = dev;
    /* Anything that is not a pad we know still reports raw, so the settings
     * page can show what it is sending. */
    s_state = s_is_dualsense ? PAD_STATE_READY : PAD_STATE_UNSUPPORTED;
}

/* ------------------------------------------------------------------ start -- */

static void pad_task(void *arg)
{
    (void)arg;

    /* The host port is powered from the board's own supply. If the board
     * browns out when a controller is plugged in, feed it from a stronger
     * source than a laptop USB port. */
    esp_err_t err = bsp_usb_host_start(BSP_USB_HOST_POWER_MODE_USB_DEV, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "usb host start failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }

    const hid_host_driver_config_t cfg = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = driver_cb,
        .callback_arg = NULL,
    };

    err = hid_host_install(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "hid host install failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "usb hid host ready");
    }

    vTaskDelete(NULL);
}

void app_pad_start(void)
{
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        return;
    }
    xTaskCreate(pad_task, "pad", 4096, NULL, 4, NULL);
}

/* ------------------------------------------------------------------ query -- */

pad_state_t app_pad_state(void)
{
    return s_state;
}

const char *app_pad_name(void)
{
    return s_name;
}

bool app_pad_get(pad_input_t *out)
{
    bool valid;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    valid = s_input_valid;
    if (valid) {
        *out = s_input;
    }
    xSemaphoreGive(s_lock);
    return valid;
}

int app_pad_raw(uint8_t *out, int max)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int n = s_raw_len < max ? s_raw_len : max;
    memcpy(out, s_raw, n);
    xSemaphoreGive(s_lock);
    return n;
}
