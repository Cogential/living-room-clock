/*
 * Lab - clean laboratory signage: white plates on a seamed wall, and two
 * accent colours closing on the middle as the minute runs out.
 */
#include "ui/ui.h"
#include "ui/ui_hud.h"
#include "ui/ui_theme.h"

#define SEAM   0xD3D8DD
#define CHAR   0x24282C
#define ORANGE 0xF28E1C
#define BLUE   0x2FA8E0

static const ui_hud_theme_t THEME = {
    .ink = CHAR, .dim = 0x6C7378,
    .panel = 0xFFFFFF, .panel_opa = LV_OPA_90,
    .border_w = 0, .radius = 6,
    .shadow = 0xC4CBD1, .shadow_dx = 0, .shadow_dy = 3,
    .weather = BLUE, .news = ORANGE, .hn = CHAR, .markets = 0x1B6E9B,
    .up = 0x2E9E5B, .down = 0xD9480F,
    .tag_prefix = "", .tag_family = UI_FONT_MEDIUM,
    .clock_family = UI_FONT_THIN, .clock = CHAR, .clock_align = LV_TEXT_ALIGN_CENTER,
    .date_family = UI_FONT_MEDIUM, .date = 0x6C7378,
    .secs_style = HUD_SECS_DUAL, .secs = BLUE, .secs2 = ORANGE,
    .secs_track = SEAM, .secs_track_opa = LV_OPA_COVER,
};

static lv_obj_t *lab_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0xF5F7F8), lv_color_hex(0xE2E6EA));

    /* Wall panel seams on a coarse grid. */
    for (int x = 0; x <= UI_SCREEN_W; x += 128) {
        ui_add_rect(root, x, 0, 2, UI_SCREEN_H, SEAM, LV_OPA_COVER, 0);
    }
    for (int y = 0; y <= UI_SCREEN_H; y += 128) {
        ui_add_rect(root, 0, y, UI_SCREEN_W, 2, SEAM, LV_OPA_COVER, 0);
    }

    /* Signage plate the clock sits on. */
    ui_add_rect(root, HUD_CLOCK_X, 9, HUD_CLOCK_W, HUD_CLOCK_H - 6, 0xC4CBD1,
                LV_OPA_COVER, 6);
    ui_add_rect(root, HUD_CLOCK_X, 6, HUD_CLOCK_W, HUD_CLOCK_H - 6, 0xFFFFFF,
                LV_OPA_COVER, 6);
    ui_add_rect(root, HUD_CLOCK_X, 6, HUD_CLOCK_W, 6, CHAR, LV_OPA_COVER, 0);

    ui_hud_create(root, &THEME);
    return root;
}

const clock_face_t face_lab = {
    .name = "Lab",
    .light_chrome = false,
    .create = lab_create,
    .tick = ui_hud_tick,
    .tap = ui_hud_tap,
};
