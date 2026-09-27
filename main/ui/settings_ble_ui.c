#include <stdio.h>
#include <string.h>

#include "ble/app_ble.h"
#include "ui/settings_ui.h"
#include "ui/ui_fonts.h"
#include "ui/ui_theme.h"
#include "ui/ui_widgets.h"

#define BLE_POLL_MS 1200

static lv_obj_t *s_list;
static lv_obj_t *s_status_label;
static int s_shown_count = -1;
static ble_device_t s_devices[BLE_MAX_DEVICES];

static void device_picked_cb(lv_event_t *e)
{
    const ble_device_t *dev = (const ble_device_t *)lv_event_get_user_data(e);
    app_ble_connect(dev->addr, dev->addr_type);
}

static void rebuild_list(void)
{
    int count = app_ble_get_devices(s_devices, BLE_MAX_DEVICES);
    if (count == s_shown_count) {
        return;
    }
    s_shown_count = count;

    lv_obj_clean(s_list);

    if (app_ble_state() == BLE_STATE_UNAVAILABLE) {
        w_placeholder(s_list, "Bluetooth controller unavailable");
        return;
    }
    if (count == 0) {
        w_placeholder(s_list, app_ble_state() == BLE_STATE_SCANNING
                                  ? "Scanning..."
                                  : "Nothing found yet");
        return;
    }

    for (int i = 0; i < count; i++) {
        char label[BLE_NAME_LEN + 24];
        if (s_devices[i].name[0]) {
            snprintf(label, sizeof(label), "%s", s_devices[i].name);
        } else {
            /* Plenty of devices advertise without a name; the address is all we
             * can honestly show. */
            snprintf(label, sizeof(label), "%02X:%02X:%02X:%02X:%02X:%02X",
                     s_devices[i].addr[5], s_devices[i].addr[4], s_devices[i].addr[3],
                     s_devices[i].addr[2], s_devices[i].addr[1], s_devices[i].addr[0]);
        }

        char value[16];
        snprintf(value, sizeof(value), "%d dBm", s_devices[i].rssi);

        lv_obj_t *row = w_nav_row(s_list, label, value,
                                  device_picked_cb, &s_devices[i], NULL);

        if (!s_devices[i].connectable) {
            /* Beacons and the like: still listed, but dimmed so it is obvious
             * they will not accept a connection. */
            lv_obj_t *title = lv_obj_get_child(row, 0);
            lv_obj_set_style_text_color(title, lv_color_hex(W_DIM), 0);
        }
    }
}

static void poll_cb(lv_timer_t *timer)
{
    (void)timer;

    if (s_status_label) {
        lv_label_set_text(s_status_label, app_ble_state_text());
    }
    rebuild_list();
}

static void scan_toggle_cb(lv_event_t *e)
{
    lv_obj_t *row = lv_event_get_target(e);
    lv_obj_t *label = lv_obj_get_child(row, 0);

    if (app_ble_state() == BLE_STATE_SCANNING) {
        app_ble_scan_stop();
        lv_label_set_text(label, "Scan for devices");
    } else {
        s_shown_count = -1;
        app_ble_scan_start();
        lv_label_set_text(label, "Stop scanning");
    }
}

static void forget_cb(lv_event_t *e)
{
    (void)e;
    app_ble_forget_peer();
    s_shown_count = -1;
}

static void page_deleted_cb(lv_event_t *e)
{
    lv_timer_t *timer = (lv_timer_t *)lv_event_get_user_data(e);
    if (timer) {
        lv_timer_delete(timer);
    }
    /* Scanning is expensive and pointless once the list is gone. */
    app_ble_scan_stop();

    s_list = NULL;
    s_status_label = NULL;
    s_shown_count = -1;
}

static void back_cb(lv_event_t *e)
{
    (void)e;
    settings_ui_open();
}

void settings_ui_open_ble(void)
{
    lv_obj_t *page = w_page();
    w_header(page, "Bluetooth", back_cb, NULL);
    lv_obj_t *body = w_body(page);

    w_section(body, "STATUS");
    w_info_row(body, "State", "", &s_status_label);

    if (app_ble_state() == BLE_STATE_UNAVAILABLE) {
        w_placeholder(body,
                      "No Bluetooth controller. The ESP32-C6 slave firmware\n"
                      "does not expose BT over the hosted link.");
    } else {
        w_action_row(body, "Scan for devices", W_ACCENT, scan_toggle_cb, NULL);
        if (app_ble_has_saved_peer()) {
            w_action_row(body, "Forget saved device", W_DANGER, forget_cb, NULL);
        }
    }

    /* Set expectations up front rather than letting someone hunt for a speaker
     * that this radio cannot ever talk to. */
    w_section(body, "DEVICES");
    lv_obj_t *note = lv_label_create(body);
    lv_label_set_text(note,
                      "BLE only - the C6 has no Classic Bluetooth, so audio\n"
                      "devices such as speakers and headphones cannot pair.");
    lv_obj_set_style_text_color(note, lv_color_hex(W_DIM), 0);
    lv_obj_set_style_text_font(note, ui_font(UI_FONT_MEDIUM, UI_TEXT_SM), 0);
    lv_obj_set_style_pad_left(note, 6, 0);
    lv_obj_set_style_pad_bottom(note, 6, 0);

    s_list = lv_obj_create(body);
    lv_obj_remove_style_all(s_list);
    lv_obj_set_width(s_list, LV_PCT(100));
    lv_obj_set_height(s_list, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_list, 10, 0);
    lv_obj_remove_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);

    s_shown_count = -1;
    rebuild_list();

    lv_timer_t *timer = lv_timer_create(poll_cb, BLE_POLL_MS, NULL);
    lv_obj_add_event_cb(page, page_deleted_cb, LV_EVENT_DELETE, timer);
    poll_cb(NULL);

    settings_ui_push(page, true);
}
