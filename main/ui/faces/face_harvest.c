/*
 * Harvest - a green field with the clock carved into a wooden sign and every
 * feed on its own wooden board. The date reads "Fall 28", not 28/09.
 */
#include <stdio.h>

#include "ui/ui.h"
#include "ui/ui_hud.h"
#include "ui/ui_theme.h"

#define WOOD_DARK  0x5A3A22
#define WOOD       0x8B5A2B
#define WOOD_LIGHT 0xB07D4A
#define CREAM      0xF7E7C4
#define GOLD       0xFFD166

/* Farm-calendar seasons rather than the date; mapping by month
 * keeps it recognisable rather than accurate. */
static const char *SEASONS[12] = {
    "Winter", "Winter", "Spring", "Spring", "Spring", "Summer",
    "Summer", "Summer", "Fall",   "Fall",   "Fall",   "Winter",
};

static void season_date(const struct tm *tm, char *out, size_t len)
{
    snprintf(out, len, "%s %d", SEASONS[tm->tm_mon % 12], tm->tm_mday);
}

static const ui_hud_theme_t THEME = {
    .ink = CREAM, .dim = 0xE0C9A0,
    .panel = WOOD, .panel_opa = LV_OPA_COVER,
    .border_w = 2, .border_opa = LV_OPA_70, .radius = 8,
    .shadow = 0x2E1D10, .shadow_dx = 0, .shadow_dy = 5,
    .weather = 0x9BD8FF, .news = GOLD, .hn = 0xFF8C69, .markets = 0xA8E07A,
    .up = 0xA8E07A, .down = 0xFF8C69,
    .tag_prefix = "", .tag_family = UI_FONT_MEDIUM,
    .clock_family = UI_FONT_MONO, .clock = CREAM, .clock_align = LV_TEXT_ALIGN_CENTER,
    /* Carved: a dark offset reads as the numerals cut into the board. */
    .ghost = 0x3B2716, .ghost_opa = LV_OPA_60, .ghost_dx = 4, .ghost_dy = 4,
    .date_family = UI_FONT_MONO, .date = GOLD, .date_fmt = season_date,
    .secs_style = HUD_SECS_BAR, .secs = GOLD, .secs_track = 0x3B2716, .secs_track_opa = LV_OPA_60,
};

static lv_obj_t *harvest_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0x8FCB6E), lv_color_hex(0x40733A));
    ui_add_glow(root, lv_color_hex(0xFFD98A), 480, 380, -240, LV_OPA_30);

    /* Bevelled sign behind the clock column, nested rectangles. */
    const int x = HUD_CLOCK_X, y = 4, w = HUD_CLOCK_W, h = HUD_CLOCK_H - 6;
    ui_add_rect(root, x, y + 5, w, h, 0x2E1D10, LV_OPA_COVER, 14);
    ui_add_rect(root, x, y, w, h, WOOD_DARK, LV_OPA_COVER, 14);
    ui_add_rect(root, x + 8, y + 8, w - 16, h - 16, WOOD, LV_OPA_COVER, 12);
    ui_add_rect(root, x + 16, y + 16, w - 32, h - 32, WOOD_LIGHT, LV_OPA_40, 10);

    ui_hud_create(root, &THEME);
    return root;
}

const clock_face_t face_harvest = {
    .name = "Harvest",
    /* Light: the icons land on the dark wood of the weather board. */
    .light_chrome = true,
    .create = harvest_create,
    .tick = ui_hud_tick,
    .tap = ui_hud_tap,
};
