/*
 * Food - look up a product by barcode.
 *
 * There is no camera wired to the MIPI-CSI connector, so the code is typed on a
 * numeric keypad. Nutri-Score and NOVA are the headline numbers; the per-100g
 * table is underneath.
 */
#include <stdio.h>
#include <string.h>

#include "bsp/esp-bsp.h"
#include "net/app_api.h"
#include "net/app_net.h"
#include "ui/food_ui.h"
#include "ui/ui.h"
#include "ui/ui_fonts.h"
#include "ui/ui_theme.h"
#include "ui/ui_widgets.h"

#define POLL_MS 300

static lv_obj_t *s_entry;
static lv_obj_t *s_status;
static lv_obj_t *s_name;
static lv_obj_t *s_brand;
static lv_obj_t *s_score;
static lv_obj_t *s_score_label;
static lv_obj_t *s_nova;
static lv_obj_t *s_nutrients;
static char s_code[16];
static bool s_awaiting;

/* Nutri-Score's own colour ramp, A (dark green) through E (red). */
static uint32_t nutriscore_colour(char grade)
{
    switch (grade) {
    case 'A': case 'a': return 0x2E7D32;
    case 'B': case 'b': return 0x7CB342;
    case 'C': case 'c': return 0xF9A825;
    case 'D': case 'd': return 0xEF6C00;
    case 'E': case 'e': return 0xC62828;
    default:            return 0x39404E;
    }
}

static void show_result(void)
{
    api_food_t f;
    /* Every way out of here must replace "Looking up...", or the page sits on
     * it forever: poll_cb has already stopped waiting. */
    bool ok = app_food_result(&f);
    if (strcmp(f.code, s_code) != 0) {
        /* The code was edited while an earlier lookup was still running. */
        lv_label_set_text(s_status, "Press GO to look this code up");
        return;
    }
    if (!ok) {
        app_net_state_t net = app_net_state();
        lv_label_set_text(s_status, net == APP_NET_CONNECTED || net == APP_NET_SYNCED
                                        ? "Lookup failed - try again"
                                        : "Lookup failed - no network");
        return;
    }

    if (!f.found) {
        lv_label_set_text_fmt(s_status, "No product for %s", f.code);
        lv_label_set_text(s_name, "");
        lv_label_set_text(s_brand, "");
        lv_label_set_text(s_nutrients, "");
        lv_label_set_text(s_score_label, "?");
        lv_obj_set_style_bg_color(s_score, lv_color_hex(nutriscore_colour(0)), 0);
        lv_label_set_text(s_nova, "");
        return;
    }

    lv_label_set_text(s_status, "");
    lv_label_set_text(s_name, f.name[0] ? f.name : "(unnamed product)");
    lv_label_set_text(s_brand, f.brands);

    char grade = f.nutriscore[0];
    lv_label_set_text_fmt(s_score_label, "%c", grade ? (grade >= 'a' ? grade - 32 : grade) : '?');
    lv_obj_set_style_bg_color(s_score, lv_color_hex(nutriscore_colour(grade)), 0);

    if (f.nova > 0) {
        /* NOVA 4 is "ultra-processed"; saying so beats printing a bare digit. */
        static const char *NOVA_TEXT[5] = {
            "", "Unprocessed", "Culinary ingredient", "Processed", "Ultra-processed",
        };
        lv_label_set_text_fmt(s_nova, "NOVA %d  ·  %s", f.nova,
                              NOVA_TEXT[f.nova > 4 ? 4 : f.nova]);
    } else {
        lv_label_set_text(s_nova, "");
    }

    char table[256] = "";
    if (f.energy_kcal >= 0) {
        char part[48];
        snprintf(part, sizeof(part), "Energy   %.0f kcal\n", f.energy_kcal);
        strlcat(table, part, sizeof(table));
    }
    struct { const char *label; float value; const char *unit; } rows[] = {
        { "Fat",       f.fat,           "g" },
        { "  of which saturates", f.saturated_fat, "g" },
        { "Carbs",     f.carbohydrates, "g" },
        { "  of which sugars",    f.sugars, "g" },
        { "Protein",   f.proteins,      "g" },
        { "Salt",      f.salt,          "g" },
    };
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        if (rows[i].value < 0) {
            continue;
        }
        char part[64];
        snprintf(part, sizeof(part), "%-22s %.1f %s\n",
                 rows[i].label, rows[i].value, rows[i].unit);
        strlcat(table, part, sizeof(table));
    }
    lv_label_set_text(s_nutrients, table[0] ? table : "No nutrition data");
}

static void poll_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!s_awaiting) {
        return;
    }
    if (app_food_busy()) {
        lv_label_set_text(s_status, "Looking up...");
        return;
    }
    s_awaiting = false;
    show_result();
}

/* ------------------------------------------------------------------ entry -- */

static void keypad_cb(lv_event_t *e)
{
    const char *key = (const char *)lv_event_get_user_data(e);

    if (strcmp(key, "<") == 0) {
        size_t len = strlen(s_code);
        if (len > 0) {
            s_code[len - 1] = '\0';
        }
    } else if (strcmp(key, "GO") == 0) {
        if (strlen(s_code) >= 8) {
            if (app_food_request(s_code)) {
                s_awaiting = true;
                lv_label_set_text(s_status, "Looking up...");
            } else if (app_food_busy()) {
                lv_label_set_text(s_status, "Still looking up the last code");
            } else {
                lv_label_set_text(s_status, app_api_configured()
                                                ? "Could not start the lookup"
                                                : "Food lookup is not set up");
            }
        } else {
            lv_label_set_text(s_status, "Barcodes are at least 8 digits");
        }
        return;
    } else if (strlen(s_code) < sizeof(s_code) - 1) {
        strlcat(s_code, key, sizeof(s_code));
    }

    lv_label_set_text(s_entry, s_code[0] ? s_code : "—");
}

static void build_keypad(lv_obj_t *parent)
{
    /* Digits only: barcodes are numeric, and a full keyboard would waste the
     * space this layout needs for the result. */
    static const char *KEYS[12] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "<", "0", "GO",
    };

    for (int i = 0; i < 12; i++) {
        int col = i % 3, row = i / 3;
        lv_obj_t *b = lv_button_create(parent);
        lv_obj_remove_style_all(b);
        lv_obj_set_size(b, 92, 62);
        lv_obj_set_pos(b, col * 100, row * 70);
        lv_obj_set_style_radius(b, 12, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(
            strcmp(KEYS[i], "GO") == 0 ? W_ACCENT : 0x2A323F), 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(W_CARD_PRESS), LV_STATE_PRESSED);
        lv_obj_add_event_cb(b, keypad_cb, LV_EVENT_CLICKED, (void *)KEYS[i]);

        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, KEYS[i]);
        lv_obj_set_style_text_font(l, ui_font(UI_FONT_MEDIUM, UI_TEXT_MD), 0);
        lv_obj_set_style_text_color(l, lv_color_hex(W_TEXT), 0);
        lv_obj_center(l);
    }
}

static void page_deleted_cb(lv_event_t *e)
{
    lv_timer_t *timer = (lv_timer_t *)lv_event_get_user_data(e);
    if (timer) {
        lv_timer_delete(timer);
    }
    s_entry = s_status = s_name = s_brand = NULL;
    s_score = s_score_label = s_nova = s_nutrients = NULL;
    s_awaiting = false;
}

static void back_cb(lv_event_t *e)
{
    (void)e;
    ui_close_app();
}

void food_ui_open(void)
{
    s_code[0] = '\0';
    s_awaiting = false;

    lv_obj_t *page = w_page();
    w_header(page, "Food", back_cb, NULL);

    /* Fixed two-column layout rather than the scrolling settings body: the
     * keypad wants to stay put while the result changes beside it. */
    lv_obj_t *body = lv_obj_create(page);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, UI_SCREEN_W, UI_SCREEN_H - W_HEADER_H);
    lv_obj_set_pos(body, 0, W_HEADER_H);
    lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- left: entry ---- */
    lv_obj_t *pad = lv_obj_create(body);
    lv_obj_remove_style_all(pad);
    lv_obj_set_size(pad, 300, 290);
    lv_obj_set_pos(pad, 56, 128);
    lv_obj_remove_flag(pad, LV_OBJ_FLAG_SCROLLABLE);
    build_keypad(pad);

    s_entry = lv_label_create(body);
    lv_obj_set_style_text_font(s_entry, ui_font(UI_FONT_MONO, UI_TEXT_LG), 0);
    lv_obj_set_style_text_color(s_entry, lv_color_hex(W_TEXT), 0);
    lv_obj_set_width(s_entry, 300);
    lv_obj_set_pos(s_entry, 56, 78);
    lv_label_set_text(s_entry, "—");

    /* ---- right: result ---- */
    s_name = lv_label_create(body);
    lv_obj_set_style_text_font(s_name, ui_font(UI_FONT_MEDIUM, UI_TEXT_LG), 0);
    lv_obj_set_style_text_color(s_name, lv_color_hex(W_TEXT), 0);
    lv_obj_set_width(s_name, 520);
    lv_label_set_long_mode(s_name, LV_LABEL_LONG_DOT);
    lv_obj_set_height(s_name, 68);
    lv_obj_set_pos(s_name, 420, 72);
    lv_label_set_text(s_name, "");

    s_brand = lv_label_create(body);
    lv_obj_set_style_text_font(s_brand, ui_font(UI_FONT_MEDIUM, UI_TEXT_SM), 0);
    lv_obj_set_style_text_color(s_brand, lv_color_hex(W_DIM), 0);
    lv_obj_set_width(s_brand, 520);
    lv_label_set_long_mode(s_brand, LV_LABEL_LONG_DOT);
    lv_obj_set_height(s_brand, 26);
    lv_obj_set_pos(s_brand, 420, 140);
    lv_label_set_text(s_brand, "");

    s_score = ui_add_rect(body, 420, 178, 72, 72, 0x39404E, LV_OPA_COVER, 12);
    s_score_label = lv_label_create(s_score);
    lv_obj_set_style_text_font(s_score_label, ui_font(UI_FONT_MEDIUM, FACE_LABEL_L), 0);
    lv_obj_set_style_text_color(s_score_label, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(s_score_label, "");
    lv_obj_center(s_score_label);

    s_nova = lv_label_create(body);
    lv_obj_set_style_text_font(s_nova, ui_font(UI_FONT_MEDIUM, UI_TEXT_MD), 0);
    lv_obj_set_style_text_color(s_nova, lv_color_hex(W_DIM), 0);
    lv_obj_set_pos(s_nova, 512, 202);
    lv_label_set_text(s_nova, "");

    s_nutrients = lv_label_create(body);
    lv_obj_set_style_text_font(s_nutrients, ui_font(UI_FONT_MONO, UI_TEXT_SM), 0);
    lv_obj_set_style_text_color(s_nutrients, lv_color_hex(W_TEXT), 0);
    lv_obj_set_pos(s_nutrients, 420, 270);
    lv_label_set_text(s_nutrients, "");

    s_status = lv_label_create(body);
    lv_obj_set_style_text_font(s_status, ui_font(UI_FONT_MEDIUM, UI_TEXT_SM), 0);
    lv_obj_set_style_text_color(s_status, lv_color_hex(W_DIM), 0);
    lv_obj_set_width(s_status, 520);
    lv_obj_set_pos(s_status, 420, 462);
    lv_label_set_text(s_status, app_api_configured()
        ? "Type a barcode, then GO" : "No API token");

    lv_timer_t *timer = lv_timer_create(poll_cb, POLL_MS, NULL);
    lv_obj_add_event_cb(page, page_deleted_cb, LV_EVENT_DELETE, timer);

    lv_screen_load_anim(page, LV_SCR_LOAD_ANIM_MOVE_LEFT, 260, 0, true);
}
