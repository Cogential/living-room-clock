#include <stdio.h>
#include <string.h>

#include "app_settings.h"
#include "net/app_net.h"
#include "ui/settings_ui.h"
#include "ui/ui_fonts.h"
#include "ui/ui_theme.h"
#include "ui/ui_widgets.h"

/* Rebuilding the whole list on every poll would fight the user's scroll, so the
 * list is only rebuilt when the set of networks actually changes. */
#define WIFI_POLL_MS 1500

/*
 * Quiet time between automatic scans. A scan pauses any join still retrying
 * (the driver cannot do both), so scanning back to back would starve it - and
 * would starve the join the user starts from this very page.
 */
#define WIFI_RESCAN_MS 10000

static lv_obj_t *s_list;
static lv_obj_t *s_status_label;
static int s_shown_count = -1;
static const char *s_shown_placeholder;
static uint32_t s_last_scan_activity;
static uint32_t s_seen_generation;
static char s_pending_ssid[NET_SSID_LEN + 1];

static void rebuild_list(void);

/* ------------------------------------------------------------- password -- */

static lv_obj_t *s_keyboard;
static lv_obj_t *s_password_field;

static void password_done_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CANCEL) {
        lv_obj_delete(lv_obj_get_parent(s_keyboard));
        s_keyboard = NULL;
        s_password_field = NULL;
        return;
    }

    if (code == LV_EVENT_READY) {
        const char *pass = lv_textarea_get_text(s_password_field);
        app_net_connect(s_pending_ssid, pass);

        lv_obj_delete(lv_obj_get_parent(s_keyboard));
        s_keyboard = NULL;
        s_password_field = NULL;

        if (s_status_label) {
            lv_label_set_text_fmt(s_status_label, "Joining %s", s_pending_ssid);
        }
    }
}

static void show_password_sheet(const char *ssid)
{
    strlcpy(s_pending_ssid, ssid, sizeof(s_pending_ssid));

    /* Full-screen overlay on the active page rather than a new screen: the
     * keyboard is modal and returning must not lose the scan results. */
    lv_obj_t *overlay = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(W_BG), 0);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(overlay);
    lv_label_set_text_fmt(title, "Password for %s", ssid);
    lv_obj_set_style_text_color(title, lv_color_hex(W_TEXT), 0);
    lv_obj_set_style_text_font(title, ui_font(UI_FONT_MEDIUM, UI_TEXT_LG), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 26);

    s_password_field = lv_textarea_create(overlay);
    lv_obj_set_size(s_password_field, 700, 62);
    lv_obj_align(s_password_field, LV_ALIGN_TOP_MID, 0, 74);
    lv_textarea_set_one_line(s_password_field, true);
    lv_textarea_set_password_mode(s_password_field, true);
    lv_textarea_set_placeholder_text(s_password_field, "Network password");
    lv_obj_set_style_bg_color(s_password_field, lv_color_hex(W_CARD), 0);
    lv_obj_set_style_text_color(s_password_field, lv_color_hex(W_TEXT), 0);
    lv_obj_set_style_border_color(s_password_field, lv_color_hex(W_ACCENT), 0);
    lv_obj_set_style_border_width(s_password_field, 2, 0);
    lv_obj_set_style_text_font(s_password_field, ui_font(UI_FONT_MEDIUM, UI_TEXT_MD), 0);

    s_keyboard = lv_keyboard_create(overlay);
    lv_obj_set_size(s_keyboard, UI_SCREEN_W, 330);
    lv_obj_align(s_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(s_keyboard, s_password_field);
    lv_obj_set_style_text_font(s_keyboard, ui_font(UI_FONT_MEDIUM, UI_TEXT_MD), 0);
    lv_obj_add_event_cb(s_keyboard, password_done_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(s_keyboard, password_done_cb, LV_EVENT_CANCEL, NULL);
}

/* ------------------------------------------------------------------ list -- */

static void network_picked_cb(lv_event_t *e)
{
    const net_ap_t *ap = (const net_ap_t *)lv_event_get_user_data(e);
    if (ap->secured) {
        show_password_sheet(ap->ssid);
    } else {
        app_net_connect(ap->ssid, "");
    }
}

/* Signal shown as bars rather than dBm: nobody reads -67 as "fine". */
static const char *signal_bars(int8_t rssi)
{
    if (rssi >= -55) return "....";
    if (rssi >= -67) return "...";
    if (rssi >= -78) return "..";
    return ".";
}

static net_ap_t s_aps[NET_MAX_SCAN_RESULTS];

static bool same_networks(const net_ap_t *a, const net_ap_t *b, int count)
{
    for (int i = 0; i < count; i++) {
        if (strcmp(a[i].ssid, b[i].ssid) != 0) {
            return false;
        }
    }
    return true;
}

static void rebuild_list(void)
{
    /* Compared by name, not just by count: a list of the same length after a
     * move is a different list, and used to never be shown. */
    net_ap_t fresh[NET_MAX_SCAN_RESULTS];
    int count = app_net_get_scan_results(fresh, NET_MAX_SCAN_RESULTS);

    if (count == 0) {
        /* The placeholder tracks the scan too, or "Scanning..." outlives a scan
         * that found nothing. */
        const char *text = app_net_scanning() ? "Scanning..." : "No networks found";
        if (s_shown_count == 0 && s_shown_placeholder == text) {
            return;
        }
        s_shown_count = 0;
        s_shown_placeholder = text;
        lv_obj_clean(s_list);
        w_placeholder(s_list, text);
        return;
    }

    if (count == s_shown_count && same_networks(fresh, s_aps, count)) {
        return;
    }
    memcpy(s_aps, fresh, count * sizeof(net_ap_t));
    s_shown_count = count;
    s_shown_placeholder = NULL;

    lv_obj_clean(s_list);

    for (int i = 0; i < count; i++) {
        char value[24];
        snprintf(value, sizeof(value), "%s%s",
                 s_aps[i].secured ? LV_SYMBOL_WIFI " " : "",
                 signal_bars(s_aps[i].rssi));

        lv_obj_t *value_label = NULL;
        lv_obj_t *row = w_nav_row(s_list, s_aps[i].ssid, value,
                                  network_picked_cb, &s_aps[i], &value_label);
        /* This one string mixes a symbol with text, so it needs the built-in
         * font that actually has the symbol glyph. */
        lv_obj_set_style_text_font(value_label, ui_symbol_font(UI_TEXT_MD), 0);

        if (strcmp(s_aps[i].ssid, app_net_ssid()) == 0) {
            lv_obj_t *title = lv_obj_get_child(row, 0);
            lv_obj_set_style_text_color(title, lv_color_hex(W_ACCENT), 0);
        }
    }
}

static void poll_cb(lv_timer_t *timer)
{
    (void)timer;

    if (s_status_label) {
        const char *ssid = app_net_ssid();
        if (ssid[0]) {
            lv_label_set_text_fmt(s_status_label, "%s - %s", ssid, app_net_state_text());
        } else {
            lv_label_set_text(s_status_label, "Not configured");
        }
    }

    rebuild_list();

    /* Keep the list fresh while the page is open, counting the gap from when
     * the last scan finished rather than from when it started. */
    uint32_t generation = app_net_scan_generation();
    if (generation != s_seen_generation) {
        s_seen_generation = generation;
        s_last_scan_activity = lv_tick_get();
    }
    if (!app_net_scanning() && lv_tick_elaps(s_last_scan_activity) >= WIFI_RESCAN_MS) {
        s_last_scan_activity = lv_tick_get();
        app_net_scan_start();
    }
}

static void scan_now(void)
{
    s_seen_generation = app_net_scan_generation();
    s_last_scan_activity = lv_tick_get();
    app_net_scan_start();
}

static void page_deleted_cb(lv_event_t *e)
{
    lv_timer_t *timer = (lv_timer_t *)lv_event_get_user_data(e);
    if (timer) {
        lv_timer_delete(timer);
    }
    s_list = NULL;
    s_status_label = NULL;
    s_keyboard = NULL;
    s_password_field = NULL;
    s_shown_count = -1;
    s_shown_placeholder = NULL;
}

static void back_cb(lv_event_t *e)
{
    (void)e;
    settings_ui_open();
}

static void rescan_cb(lv_event_t *e)
{
    (void)e;
    s_shown_count = -1;
    scan_now();
}

void settings_ui_open_wifi(void)
{
    lv_obj_t *page = w_page();
    w_header(page, "Wi-Fi", back_cb, NULL);
    lv_obj_t *body = w_body(page);

    w_section(body, "STATUS");
    w_info_row(body, "Network", "", &s_status_label);
    w_action_row(body, "Scan again", W_ACCENT, rescan_cb, NULL);

    w_section(body, "NETWORKS");

    /* The scan results live in their own column so the list can be rebuilt
     * without disturbing the rows above it. */
    s_list = lv_obj_create(body);
    lv_obj_remove_style_all(s_list);
    lv_obj_set_width(s_list, LV_PCT(100));
    lv_obj_set_height(s_list, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_list, 10, 0);
    lv_obj_remove_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);

    s_shown_count = -1;
    s_shown_placeholder = NULL;
    scan_now();
    rebuild_list();

    lv_timer_t *timer = lv_timer_create(poll_cb, WIFI_POLL_MS, NULL);
    lv_obj_add_event_cb(page, page_deleted_cb, LV_EVENT_DELETE, timer);
    poll_cb(NULL);

    settings_ui_push(page, true);
}
