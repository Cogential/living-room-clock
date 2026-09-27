#include "ui/settings_ui.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app_settings.h"
#include "ble/app_ble.h"
#include "doom/app_doom.h"
#include "input/app_pad.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "net/app_api.h"
#include "net/app_net.h"
#include "ui/ui.h"
#include "ui/ui_fonts.h"
#include "ui/ui_widgets.h"

#define PAGE_ANIM_MS 260

/* Live labels on the root page, refreshed once a second while it is visible. */
static lv_obj_t *s_ip_label;
static lv_obj_t *s_uptime_label;
static lv_obj_t *s_mem_label;
static lv_obj_t *s_wifi_row_value;
static lv_obj_t *s_ble_row_value;
static lv_obj_t *s_pad_row_value;
static lv_obj_t *s_api_row_value;
static lv_obj_t *s_night_start_label;
static lv_obj_t *s_night_end_label;

void settings_ui_push(lv_obj_t *page, bool forward)
{
    lv_screen_load_anim(page,
                        forward ? LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_MOVE_RIGHT,
                        PAGE_ANIM_MS, 0, true);
}

/* ------------------------------------------------------------- callbacks -- */

static void back_to_clock_cb(lv_event_t *e)
{
    (void)e;
    settings_save();
    ui_close_settings();
}

static void back_to_root_cb(lv_event_t *e)
{
    (void)e;
    settings_save();
    settings_ui_open();
}

static void brightness_changed_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    lv_obj_t *value_label = (lv_obj_t *)lv_event_get_user_data(e);

    int v = lv_slider_get_value(slider);
    settings()->brightness = (uint8_t)v;
    lv_label_set_text_fmt(value_label, "%d%%", v);

    /* Apply live so dragging the slider is its own preview. Night dimming can
     * override this later; passing -1 means "ignore the schedule for now". */
    settings_apply_brightness(-1);
}

static void night_brightness_changed_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    lv_obj_t *value_label = (lv_obj_t *)lv_event_get_user_data(e);

    int v = lv_slider_get_value(slider);
    settings()->night_brightness = (uint8_t)v;
    lv_label_set_text_fmt(value_label, "%d%%", v);
}

/* Sliders fire continuously while dragged; only touch NVS once, on release. */
static void slider_released_cb(lv_event_t *e)
{
    (void)e;
    settings_save();
}

static void night_toggle_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    settings()->night_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    settings_save();
}

static void hour_24_toggle_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    settings()->hour_24 = lv_obj_has_state(sw, LV_STATE_CHECKED);
    settings_save();
}

static void update_night_labels(void)
{
    if (s_night_start_label) {
        lv_label_set_text_fmt(s_night_start_label, "%02d:00", settings()->night_start_hour);
    }
    if (s_night_end_label) {
        lv_label_set_text_fmt(s_night_end_label, "%02d:00", settings()->night_end_hour);
    }
}

static void night_start_minus_cb(lv_event_t *e)
{
    (void)e;
    settings()->night_start_hour = (settings()->night_start_hour + 23) % 24;
    update_night_labels();
    settings_save();
}

static void night_start_plus_cb(lv_event_t *e)
{
    (void)e;
    settings()->night_start_hour = (settings()->night_start_hour + 1) % 24;
    update_night_labels();
    settings_save();
}

static void night_end_minus_cb(lv_event_t *e)
{
    (void)e;
    settings()->night_end_hour = (settings()->night_end_hour + 23) % 24;
    update_night_labels();
    settings_save();
}

static void night_end_plus_cb(lv_event_t *e)
{
    (void)e;
    settings()->night_end_hour = (settings()->night_end_hour + 1) % 24;
    update_night_labels();
    settings_save();
}

static void open_faces_cb(lv_event_t *e)
{
    (void)e;
    settings_ui_open_faces();
}

static void open_timezone_cb(lv_event_t *e)
{
    (void)e;
    settings_ui_open_timezone();
}

static void open_wifi_cb(lv_event_t *e)
{
    (void)e;
    settings_ui_open_wifi();
}

static void open_games_cb(lv_event_t *e)
{
    (void)e;
    settings_ui_open_games();
}

static void open_pad_cb(lv_event_t *e)
{
    (void)e;
    settings_ui_open_pad();
}

static void open_ble_cb(lv_event_t *e)
{
    (void)e;
    settings_ui_open_ble();
}

static void sync_now_cb(lv_event_t *e)
{
    lv_obj_t *row = lv_event_get_target(e);
    app_net_sync_now();

    /* Momentary acknowledgement: the row is the only affordance here. */
    lv_obj_t *label = lv_obj_get_child(row, 0);
    if (label) {
        lv_label_set_text(label, "Requested");
    }
}

/* ----------------------------------------------------------------- ticks -- */

static void refresh_cb(lv_timer_t *timer)
{
    (void)timer;

    if (s_ip_label) {
        lv_label_set_text(s_ip_label, app_net_ip());
    }
    if (s_wifi_row_value) {
        const char *ssid = app_net_ssid();
        lv_label_set_text(s_wifi_row_value,
                          ssid[0] ? ssid : app_net_state_text());
    }
    if (s_ble_row_value) {
        lv_label_set_text(s_ble_row_value, app_ble_state_text());
    }
    if (s_api_row_value) {
        lv_label_set_text(s_api_row_value,
                          app_api_configured() ? app_api_status() : "No API token");
    }
    if (s_pad_row_value) {
        pad_state_t pad = app_pad_state();
        lv_label_set_text(s_pad_row_value,
                          pad == PAD_STATE_READY       ? app_pad_name() :
                          pad == PAD_STATE_UNSUPPORTED ? "Unknown device" :
                                                         "Not connected");
    }
    if (s_uptime_label) {
        int64_t secs = esp_timer_get_time() / 1000000;
        lv_label_set_text_fmt(s_uptime_label, "%lldh %lldm",
                              secs / 3600, (secs % 3600) / 60);
    }
    if (s_mem_label) {
        lv_label_set_text_fmt(s_mem_label, "%u KB",
                              (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    }
}

/* The refresh timer belongs to the page; LVGL deletes the page out from under
 * us on navigation, so tear the timer down with it. */
static void page_deleted_cb(lv_event_t *e)
{
    lv_timer_t *timer = (lv_timer_t *)lv_event_get_user_data(e);
    if (timer) {
        lv_timer_delete(timer);
    }
    s_ip_label = NULL;
    s_uptime_label = NULL;
    s_mem_label = NULL;
    s_wifi_row_value = NULL;
    s_ble_row_value = NULL;
    s_pad_row_value = NULL;
    s_api_row_value = NULL;
    s_night_start_label = NULL;
    s_night_end_label = NULL;
}

/* ------------------------------------------------------------------ root -- */

void settings_ui_open(void)
{
    settings_t *cfg = settings();

    lv_obj_t *page = w_page();
    w_header(page, "Settings", back_to_clock_cb, NULL);
    lv_obj_t *body = w_body(page);

    /* ---- face ---- */
    w_section(body, "CLOCK FACE");
    w_nav_row(body, "Face", ui_face_name(ui_face_selected()), open_faces_cb, NULL, NULL);

    /* ---- display ---- */
    w_section(body, "DISPLAY");

    lv_obj_t *slider, *value_label;
    w_slider_row(body, "Brightness", BRIGHTNESS_MIN, BRIGHTNESS_MAX, cfg->brightness,
                 &slider, &value_label);
    lv_obj_add_event_cb(slider, brightness_changed_cb, LV_EVENT_VALUE_CHANGED, value_label);
    lv_obj_add_event_cb(slider, slider_released_cb, LV_EVENT_RELEASED, NULL);

    w_switch_row(body, "Night dimming", cfg->night_enabled, night_toggle_cb, NULL, NULL);

    lv_obj_t *night_slider, *night_value;
    w_slider_row(body, "Night brightness", BRIGHTNESS_MIN, BRIGHTNESS_MAX,
                 cfg->night_brightness, &night_slider, &night_value);
    lv_obj_add_event_cb(night_slider, night_brightness_changed_cb,
                        LV_EVENT_VALUE_CHANGED, night_value);
    lv_obj_add_event_cb(night_slider, slider_released_cb, LV_EVENT_RELEASED, NULL);

    w_stepper_row(body, "Dim from", "", night_start_minus_cb, night_start_plus_cb,
                  NULL, &s_night_start_label);
    w_stepper_row(body, "Back to normal at", "", night_end_minus_cb, night_end_plus_cb,
                  NULL, &s_night_end_label);
    update_night_labels();

    /* ---- time ---- */
    w_section(body, "TIME");

    w_switch_row(body, "24-hour time", cfg->hour_24, hour_24_toggle_cb, NULL, NULL);

    const char *tz_label = cfg->tz_index >= 0 && cfg->tz_index < SETTINGS_TIMEZONE_COUNT
                               ? SETTINGS_TIMEZONES[cfg->tz_index].label
                               : "Custom";
    w_nav_row(body, "Time zone", tz_label, open_timezone_cb, NULL, NULL);
    w_action_row(body, "Sync time now", W_ACCENT, sync_now_cb, NULL);

    /* ---- network ---- */
    w_section(body, "NETWORK");

    w_nav_row(body, "Wi-Fi", "", open_wifi_cb, NULL, &s_wifi_row_value);
    w_nav_row(body, "Bluetooth", "", open_ble_cb, NULL, &s_ble_row_value);
    w_nav_row(body, "Controller", "", open_pad_cb, NULL, &s_pad_row_value);

    w_section(body, "DATA");
    w_info_row(body, "Feeds", app_api_configured() ? app_api_status() : "No API token",
               &s_api_row_value);
    w_info_row(body, "Location", cfg->place, NULL);

    w_section(body, "GAMES");
    w_nav_row(body, "DOOM", app_doom_available() ? "Ready" : "No WAD",
              open_games_cb, NULL, NULL);

    /* ---- system ---- */
    w_section(body, "SYSTEM");

    w_info_row(body, "IP address", app_net_ip(), &s_ip_label);
    w_info_row(body, "Uptime", "", &s_uptime_label);
    w_info_row(body, "Free PSRAM", "", &s_mem_label);

    const esp_app_desc_t *desc = esp_app_get_description();
    w_info_row(body, "Firmware", desc->version, NULL);
    w_info_row(body, "ESP-IDF", desc->idf_ver, NULL);

    lv_timer_t *timer = lv_timer_create(refresh_cb, 1000, NULL);
    lv_obj_add_event_cb(page, page_deleted_cb, LV_EVENT_DELETE, timer);
    refresh_cb(NULL);

    settings_ui_push(page, false);
}

/* ----------------------------------------------------------------- faces -- */

static void face_pick_cb(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);

    /* Only record the choice here. Loading the face would swap the screen out
     * from under the settings stack; ui_close_settings() applies it on the way
     * out, so the list stays browsable. */
    ui_face_select(index);
    /* Straight back to the menu: the choice is made, and re-showing the list
     * just leaves the user to find "back" themselves. */
    settings_ui_open();
}

void settings_ui_open_faces(void)
{
    lv_obj_t *page = w_page();
    w_header(page, "Clock face", back_to_root_cb, NULL);
    lv_obj_t *body = w_body(page);

    int current = ui_face_selected();
    int count = ui_face_count();

    for (int i = 0; i < count; i++) {
        w_choice_row(body, ui_face_name(i), i == current,
                     face_pick_cb, (void *)(intptr_t)i);
    }

    lv_obj_t *note = lv_label_create(body);
    lv_label_set_text(note, "Swipe left or right on the clock to change face.");
    lv_obj_set_style_text_color(note, lv_color_hex(W_DIM), 0);
    lv_obj_set_style_text_font(note, ui_font(UI_FONT_MEDIUM, UI_TEXT_SM), 0);
    lv_obj_set_style_pad_all(note, 12, 0);

    settings_ui_push(page, true);
}

/* -------------------------------------------------------------- timezone -- */

static void timezone_pick_cb(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);

    settings_t *cfg = settings();
    cfg->tz_index = index;
    strlcpy(cfg->timezone, SETTINGS_TIMEZONES[index].posix, sizeof(cfg->timezone));
    settings_save();
    settings_apply_timezone();

    settings_ui_open();
}

void settings_ui_open_timezone(void)
{
    lv_obj_t *page = w_page();
    w_header(page, "Time zone", back_to_root_cb, NULL);
    lv_obj_t *body = w_body(page);

    int current = settings()->tz_index;

    for (int i = 0; i < SETTINGS_TIMEZONE_COUNT; i++) {
        w_choice_row(body, SETTINGS_TIMEZONES[i].label, i == current,
                     timezone_pick_cb, (void *)(intptr_t)i);
    }

    settings_ui_push(page, true);
}
