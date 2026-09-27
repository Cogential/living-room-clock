/*
 * Tavern - ink on parchment, roman numeral hour, ornate rules bracketing the
 * clock and a wax seal counting the seconds.
 */
#include "ui/ui.h"
#include "ui/ui_bands.h"
#include "ui/ui_fonts.h"
#include "ui/ui_theme.h"

#define INK      0x3A2A18
#define INK_SOFT 0x6B5638
#define GOLD     0xA8842C
#define WAX      0x8C2F1F

static const char *ROMAN[13] = {
    "", "I", "II", "III", "IV", "V", "VI",
    "VII", "VIII", "IX", "X", "XI", "XII",
};

static ui_bands_t s_bands;
static lv_obj_t *s_time, *s_date, *s_roman, *s_seal;
static int s_last_minute = -1, s_last_yday = -1, s_last_second = -1;

static lv_obj_t *tavern_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0xEDDCB6), lv_color_hex(0xD6C094));

    /* Foxing at the edges so the parchment is not flat. */
    ui_add_glow(root, lv_color_hex(0xB39A6A), 380, -140, -130, LV_OPA_30);
    ui_add_glow(root, lv_color_hex(0xB39A6A), 340, 780, 360, LV_OPA_30);

    ui_add_ornate_rule(root, UI_BAND_FACE_TOP, 520, GOLD, LV_OPA_80);
    ui_add_ornate_rule(root, UI_BAND_FACE_BOTTOM, 520, GOLD, LV_OPA_80);

    s_roman = lv_label_create(root);
    lv_obj_set_style_text_font(s_roman, ui_font(UI_FONT_MEDIUM, UI_TEXT_MD), 0);
    lv_obj_set_style_text_color(s_roman, lv_color_hex(GOLD), 0);
    lv_obj_set_style_text_letter_space(s_roman, 8, 0);
    lv_obj_set_width(s_roman, UI_SCREEN_W - 120);
    lv_obj_set_style_text_align(s_roman, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_roman, 60, UI_BAND_FACE_TOP + 14);
    lv_label_set_text(s_roman, "");

    s_time = ui_face_time_label(root, UI_FONT_THIN, FACE_TIME_THIN, INK);
    s_date = ui_face_date_label(root, INK_SOFT);

    /* Wax seal, bottom right of the face band, with the seconds struck in. */
    ui_add_rect(root, UI_SCREEN_W - 168, UI_BAND_FACE_BOTTOM - 84, 70, 70,
                WAX, LV_OPA_90, 35);
    s_seal = lv_label_create(root);
    lv_obj_set_style_text_font(s_seal, ui_font(UI_FONT_MEDIUM, UI_TEXT_MD), 0);
    lv_obj_set_style_text_color(s_seal, lv_color_hex(0xF3DFC8), 0);
    lv_obj_set_width(s_seal, 70);
    lv_obj_set_style_text_align(s_seal, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_seal, UI_SCREEN_W - 168, UI_BAND_FACE_BOTTOM - 60);
    lv_label_set_text(s_seal, "00");

    const ui_bands_palette_t palette = { .ink = INK, .dim = INK_SOFT, .accent = GOLD };
    ui_bands_create(&s_bands, root, &palette);

    s_last_minute = -1; s_last_yday = -1; s_last_second = -1;
    return root;
}

static void tavern_tick(const clock_time_t *now)
{
    int before = s_last_minute;
    ui_face_update_time(s_time, s_date, now, &s_last_minute, &s_last_yday);
    if (before != s_last_minute) {
        int h12 = now->tm.tm_hour % 12;
        lv_label_set_text(s_roman, ROMAN[h12 == 0 ? 12 : h12]);
    }

    if (now->tm.tm_sec != s_last_second) {
        s_last_second = now->tm.tm_sec;
        lv_label_set_text_fmt(s_seal, "%02d", now->tm.tm_sec);
    }

    ui_bands_tick(&s_bands);
}

const clock_face_t face_tavern = {
    .name = "Tavern",
    .light_chrome = false,
    .create = tavern_create,
    .tick = tavern_tick,
};
