/*
 * Cyberpunk - hazard yellow on near-black, a magenta ghost behind the numerals,
 * a panel per feed and scanlines in the gaps.
 */
#include "ui/ui.h"
#include "ui/ui_hud.h"
#include "ui/ui_theme.h"

#define YELLOW  0xFCEE0A
#define CYAN    0x00F0FF
#define MAGENTA 0xFF2A6D
#define DIM     0x8A9BA8

static const ui_hud_theme_t THEME = {
    .ink = 0xE8F4F8, .dim = DIM,
    .panel = 0x0A0C14, .panel_opa = LV_OPA_80,
    .border_w = 1, .border_opa = LV_OPA_40, .radius = 0,
    .weather = CYAN, .news = MAGENTA, .hn = 0xFF8A00, .markets = YELLOW,
    .up = 0x39FF88, .down = 0xFF3B5C,
    .tag_prefix = "// ", .tag_family = UI_FONT_MONO,
    .clock_family = UI_FONT_MONO, .clock = YELLOW, .clock_align = LV_TEXT_ALIGN_CENTER,
    /* Off-register duplicate sells the chromatic split without per-pixel work. */
    .ghost = MAGENTA, .ghost_opa = LV_OPA_50, .ghost_dx = 5, .ghost_dy = 5,
    .date_family = UI_FONT_MONO, .date = CYAN,
    .secs_style = HUD_SECS_BAR, .secs = YELLOW, .secs_track = DIM, .secs_track_opa = LV_OPA_20,
};

static lv_obj_t *cyberpunk_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0x05060A), lv_color_hex(0x0D0716));
    ui_add_glow(root, lv_color_hex(MAGENTA), 520, -180, 240, LV_OPA_10);
    ui_add_glow(root, lv_color_hex(CYAN), 460, 760, -140, LV_OPA_10);

    /* Behind the panels: texture in the gaps, none across the text. */
    ui_add_scanlines(root, 0x000000, 6, LV_OPA_20);

    ui_hud_create(root, &THEME);
    return root;
}

const clock_face_t face_cyberpunk = {
    .name = "Cyberpunk",
    .light_chrome = true,
    .create = cyberpunk_create,
    .tick = ui_hud_tick,
    .tap = ui_hud_tap,
};
