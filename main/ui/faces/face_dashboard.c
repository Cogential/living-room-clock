/*
 * Dashboard - the plainest HUD: a quiet navy gradient, rounded glass panels and
 * soft accents, nothing competing with the information.
 */
#include "ui/ui.h"
#include "ui/ui_hud.h"
#include "ui/ui_theme.h"

#define INK   0xF2F5FA
#define DIM   0x8B95A7
#define SKY   0x7DD3FC

static const ui_hud_theme_t THEME = {
    .ink = INK, .dim = DIM,
    .panel = 0x111831, .panel_opa = LV_OPA_80,
    .border_w = 1, .border_opa = LV_OPA_40, .radius = 12,
    .weather = SKY, .news = 0xF472B6, .hn = 0xFB923C, .markets = 0xA78BFA,
    .up = 0x4ADE80, .down = 0xF87171,
    .tag_prefix = "", .tag_family = UI_FONT_MEDIUM,
    .clock_family = UI_FONT_THIN, .clock = INK, .clock_align = LV_TEXT_ALIGN_CENTER,
    .date_family = UI_FONT_MEDIUM, .date = SKY,
    .secs_style = HUD_SECS_BAR, .secs = SKY, .secs_track = DIM, .secs_track_opa = LV_OPA_20,
};

static lv_obj_t *dashboard_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0x0B1020), lv_color_hex(0x141A2E));
    ui_add_glow(root, lv_color_hex(0x3B82F6), 560, -200, -260, LV_OPA_10);

    ui_hud_create(root, &THEME);
    return root;
}

const clock_face_t face_dashboard = {
    .name = "Dashboard",
    .light_chrome = true,
    .create = dashboard_create,
    .tick = ui_hud_tick,
    .tap = ui_hud_tap,
};
