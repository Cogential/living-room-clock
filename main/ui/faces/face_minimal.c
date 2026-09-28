/*
 * Minimal - warm paper and hairline ink. The one that reads in daylight: white
 * paper panels, muted section colours, thin numerals.
 */
#include "ui/ui.h"
#include "ui/ui_hud.h"
#include "ui/ui_theme.h"

#define INK  0x1A1917
#define DIM  0x7D786F

static const ui_hud_theme_t THEME = {
    .ink = INK, .dim = DIM,
    .panel = 0xFFFFFF, .panel_opa = LV_OPA_70,
    .border_w = 1, .border_opa = LV_OPA_40, .radius = 0,
    .weather = 0x3E6A9E, .news = 0xB5533C, .hn = 0xC0842B, .markets = 0x5E7D4A,
    .up = 0x2E7D32, .down = 0xC62828,
    .tag_prefix = "", .tag_family = UI_FONT_MEDIUM,
    .clock_family = UI_FONT_THIN, .clock = INK, .clock_align = LV_TEXT_ALIGN_CENTER,
    .date_family = UI_FONT_MEDIUM, .date = DIM,
    .secs_style = HUD_SECS_BAR, .secs = INK, .secs_track = 0x9A958C, .secs_track_opa = LV_OPA_30,
};

static lv_obj_t *minimal_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0xFBFAF7), lv_color_hex(0xEFEDE7));

    ui_hud_create(root, &THEME);
    return root;
}

const clock_face_t face_minimal = {
    .name = "Minimal",
    .light_chrome = false,
    .create = minimal_create,
    .tick = ui_hud_tick,
    .tap = ui_hud_tap,
};
