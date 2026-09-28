/*
 * Tavern - ink on parchment. The hour in roman numerals leads the date, an
 * ornate rule underlines it, and a wax seal counts the seconds.
 */
#include <stdio.h>

#include "ui/ui.h"
#include "ui/ui_hud.h"
#include "ui/ui_fonts.h"
#include "ui/ui_theme.h"

#define INK      0x3A2A18
#define INK_SOFT 0x6B5638
#define GOLD     0x8A6A1C
#define WAX      0x8C2F1F

/* Bottom right of the clock column, clear of the digits (to y ~178) and of a
 * centred date line. */
#define SEAL_D   60
#define SEAL_X   (HUD_CLOCK_X + HUD_CLOCK_W - SEAL_D - 4)
#define SEAL_Y   190
#define RULE_W   440
#define RULE_X   (HUD_CLOCK_X + (HUD_CLOCK_W - SEAL_D - RULE_W) / 2)
#define RULE_Y   238

static const char *ROMAN[13] = {
    "", "I", "II", "III", "IV", "V", "VI",
    "VII", "VIII", "IX", "X", "XI", "XII",
};

static lv_obj_t *s_seal;
static int s_last_second = -1;

static void roman_date(const struct tm *tm, char *out, size_t len)
{
    int h12 = tm->tm_hour % 12;
    char day[32];
    strftime(day, sizeof(day), "%a %e %b", tm);
    for (char *c = day; *c; c++) {
        if (*c >= 'a' && *c <= 'z') *c -= 32;
    }
    snprintf(out, len, "%s  ·  %s", ROMAN[h12 == 0 ? 12 : h12], day);
}

static const ui_hud_theme_t THEME = {
    .ink = INK, .dim = INK_SOFT,
    .panel = 0xF6EBD2, .panel_opa = LV_OPA_80,
    .border_w = 1, .border_opa = LV_OPA_70, .radius = 4,
    .weather = 0x2F5D62, .news = WAX, .hn = GOLD, .markets = 0x4B5E2A,
    .up = 0x3F6B2A, .down = WAX,
    .tag_prefix = "", .tag_family = UI_FONT_MEDIUM,
    .clock_family = UI_FONT_THIN, .clock = INK, .clock_align = LV_TEXT_ALIGN_CENTER,
    .date_family = UI_FONT_MEDIUM, .date = GOLD, .date_fmt = roman_date,
    .secs_style = HUD_SECS_NONE,
};

static lv_obj_t *tavern_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0xEDDCB6), lv_color_hex(0xD6C094));

    /* Foxing at the edges so the parchment is not flat. */
    ui_add_glow(root, lv_color_hex(0xB39A6A), 380, -140, -130, LV_OPA_30);
    ui_add_glow(root, lv_color_hex(0xB39A6A), 340, 780, 360, LV_OPA_30);

    ui_hud_create(root, &THEME);

    /* Ornate rule where the other faces have a seconds bar: a line with a
     * diamond, which is a square turned on its point. */
    ui_add_rect(root, RULE_X, RULE_Y, RULE_W, 2, GOLD, LV_OPA_80, 0);
    lv_obj_t *d = ui_add_rect(root, RULE_X + RULE_W / 2 - 7, RULE_Y - 6, 14, 14,
                              GOLD, LV_OPA_80, 3);
    lv_obj_set_style_transform_rotation(d, 450, 0);
    lv_obj_set_style_transform_pivot_x(d, 7, 0);
    lv_obj_set_style_transform_pivot_y(d, 7, 0);

    /* Wax seal with the seconds struck in. */
    ui_add_rect(root, SEAL_X, SEAL_Y, SEAL_D, SEAL_D, WAX, LV_OPA_90, SEAL_D / 2);
    s_seal = lv_label_create(root);
    lv_obj_set_style_text_font(s_seal, ui_font(UI_FONT_MEDIUM, UI_TEXT_MD), 0);
    lv_obj_set_style_text_color(s_seal, lv_color_hex(0xF3DFC8), 0);
    lv_obj_set_width(s_seal, SEAL_D);
    lv_obj_set_style_text_align(s_seal, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_seal, SEAL_X, SEAL_Y + (SEAL_D - 25) / 2);
    lv_label_set_text(s_seal, "00");

    s_last_second = -1;
    return root;
}

static void tavern_tick(const clock_time_t *now)
{
    ui_hud_tick(now);
    if (now->tm.tm_sec != s_last_second) {
        s_last_second = now->tm.tm_sec;
        lv_label_set_text_fmt(s_seal, "%02d", now->tm.tm_sec);
    }
}

const clock_face_t face_tavern = {
    .name = "Tavern",
    .light_chrome = false,
    .create = tavern_create,
    .tick = tavern_tick,
    .tap = ui_hud_tap,
};
