/*
 * Rapture - art deco under the sea. Brass on oxidised teal, a stepped ziggurat
 * crowning the clock, square brass-edged panels.
 */
#include "ui/ui.h"
#include "ui/ui_hud.h"
#include "ui/ui_theme.h"

#define BRASS 0xC9A227
#define CREAM 0xF2E4C2

static const ui_hud_theme_t THEME = {
    .ink = CREAM, .dim = 0x8FB0A8,
    .panel = 0x06222A, .panel_opa = LV_OPA_80,
    .border_w = 2, .border_opa = LV_OPA_60, .radius = 0,
    .weather = 0x4FD1B5, .news = BRASS, .hn = 0xE07A5F, .markets = 0x9CC5E0,
    .up = 0x4FD1B5, .down = 0xF06060,
    .tag_prefix = "", .tag_family = UI_FONT_MEDIUM,
    .clock_family = UI_FONT_THIN, .clock = CREAM, .clock_align = LV_TEXT_ALIGN_CENTER,
    .date_family = UI_FONT_MEDIUM, .date = BRASS,
    .secs_style = HUD_SECS_BAR, .secs = BRASS, .secs_track = BRASS, .secs_track_opa = LV_OPA_20,
};

/* Stepped deco ornament: three bars narrowing downward, above the digits
 * (which begin at y ~26). */
static void add_ziggurat(lv_obj_t *root)
{
    static const int widths[3] = { 150, 108, 66 };
    const int cx = HUD_CLOCK_X + HUD_CLOCK_W / 2;
    for (int i = 0; i < 3; i++) {
        ui_add_rect(root, cx - widths[i] / 2, 2 + i * 8, widths[i], 4, BRASS, LV_OPA_80, 0);
    }
}

static lv_obj_t *rapture_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0x0A2A2E), lv_color_hex(0x06181C));
    ui_add_glow(root, lv_color_hex(0x2FA98F), 560, 30, -250, LV_OPA_10);

    add_ziggurat(root);
    ui_hud_create(root, &THEME);
    return root;
}

const clock_face_t face_rapture = {
    .name = "Rapture",
    .light_chrome = true,
    .create = rapture_create,
    .tick = ui_hud_tick,
    .tap = ui_hud_tap,
};
