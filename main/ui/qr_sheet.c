#include "ui/qr_sheet.h"

#include <string.h>

#include "lvgl.h"
#include "ui/ui_fonts.h"
#include "ui/ui_theme.h"

#define BG       0x05060A
#define INK      0xE8F4F8
#define DIM      0x8A9BA8

/* Left alone on a wall, the sheet would hide the clock indefinitely. */
#define AUTO_CLOSE_MS 60000

/*
 * 360 px for the code itself. A ~130 character URL is a version 7 code, 45
 * modules plus the quiet zone, so each module is about 7 px - comfortably
 * scannable from a phone held at arm's length.
 */
#define CARD_X   56
#define CARD_Y   100
#define CARD_SZ  400
#define QR_SZ    368
#define TEXT_X   (CARD_X + CARD_SZ + 44)
#define TEXT_W   (UI_SCREEN_W - TEXT_X - 56)

static lv_obj_t *s_sheet;
static lv_timer_t *s_timer;

static void close_cb(lv_event_t *e)
{
    (void)e;
    ui_qr_sheet_close();
}

static void timer_cb(lv_timer_t *t)
{
    (void)t;
    ui_qr_sheet_close();
}

static void deleted_cb(lv_event_t *e)
{
    (void)e;
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    s_sheet = NULL;
}

static lv_obj_t *label(lv_obj_t *parent, int x, int y, int w,
                       ui_font_family_t family, int size, uint32_t colour, const char *s)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, ui_font(family, size), 0);
    lv_obj_set_style_text_color(l, lv_color_hex(colour), 0);
    if (w > 0) {
        lv_obj_set_width(l, w);
    }
    lv_obj_set_pos(l, x, y);
    lv_label_set_text(l, s);
    return l;
}

/* The white card holding the code, or a note where the code would be. */
static void build_code(lv_obj_t *sheet, const char *url)
{
    lv_obj_t *card = ui_add_rect(sheet, CARD_X, CARD_Y, CARD_SZ, CARD_SZ,
                                 0xFFFFFF, LV_OPA_COVER, 12);

    if (url && url[0]) {
        lv_obj_t *qr = lv_qrcode_create(card);
        lv_qrcode_set_size(qr, QR_SZ);
        lv_qrcode_set_dark_color(qr, lv_color_hex(0x000000));
        lv_qrcode_set_light_color(qr, lv_color_hex(0xFFFFFF));
        lv_qrcode_set_quiet_zone(qr, true);
        if (lv_qrcode_update(qr, url, strlen(url)) == LV_RESULT_OK) {
            lv_obj_center(qr);
            return;
        }
        lv_obj_delete(qr);
    }

    lv_obj_set_style_bg_color(card, lv_color_hex(0x1A1D26), 0);
    lv_obj_t *note = label(card, 0, 0, CARD_SZ - 60, UI_FONT_MEDIUM, UI_TEXT_MD, DIM,
                           "No link for this story");
    lv_obj_set_style_text_align(note, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(note);
}

void ui_qr_sheet_close(void)
{
    if (s_sheet) {
        lv_obj_delete(s_sheet);  /* deleted_cb clears the statics */
    }
}

void ui_qr_sheet_show(uint32_t accent, const char *heading, const char *title,
                      const char *detail, const char *url)
{
    ui_qr_sheet_close();

    s_sheet = lv_obj_create(lv_layer_top());
    ui_make_layer(s_sheet);
    lv_obj_set_style_bg_color(s_sheet, lv_color_hex(BG), 0);
    lv_obj_set_style_bg_opa(s_sheet, LV_OPA_COVER, 0);
    /* Clickable so taps land here instead of falling through to the chrome
     * underneath; every tap closes it. */
    lv_obj_add_flag(s_sheet, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_sheet, close_cb, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(s_sheet, deleted_cb, LV_EVENT_DELETE, NULL);

    build_code(s_sheet, url);

    ui_add_rect(s_sheet, TEXT_X, CARD_Y + 8, 64, 4, accent, LV_OPA_COVER, 0);
    lv_obj_t *h = label(s_sheet, TEXT_X, CARD_Y + 24, 0, UI_FONT_MONO, UI_TEXT_SM,
                        accent, heading);
    lv_obj_set_style_text_letter_space(h, 3, 0);

    /* Five lines of Medium 28 is 161 px; clipped there. */
    lv_obj_t *t = label(s_sheet, TEXT_X, CARD_Y + 60, TEXT_W, UI_FONT_MEDIUM, UI_TEXT_LG,
                        INK, title ? title : "");
    lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
    lv_obj_set_height(t, 5 * 33);

    if (detail && detail[0]) {
        lv_obj_t *d = label(s_sheet, TEXT_X, CARD_Y + 236, TEXT_W, UI_FONT_MONO, UI_TEXT_SM,
                            DIM, detail);
        lv_label_set_long_mode(d, LV_LABEL_LONG_DOT);
        lv_obj_set_height(d, UI_TEXT_SM);
    }

    ui_add_rect(s_sheet, TEXT_X, CARD_Y + 282, TEXT_W, 1, accent, LV_OPA_30, 0);
    label(s_sheet, TEXT_X, CARD_Y + 300, TEXT_W, UI_FONT_MEDIUM, UI_TEXT_MD, INK,
          "Scan with your phone to read the full article");
    label(s_sheet, TEXT_X, CARD_Y + 366, 0, UI_FONT_MONO, UI_TEXT_SM, DIM,
          "TAP ANYWHERE TO CLOSE");

    /* Not a one-shot repeat count: LVGL would then free the timer itself after
     * the callback, and deleted_cb would free it a second time. Deleting it from
     * deleted_cb, even from inside timer_cb, is safe. */
    s_timer = lv_timer_create(timer_cb, AUTO_CLOSE_MS, NULL);
}
