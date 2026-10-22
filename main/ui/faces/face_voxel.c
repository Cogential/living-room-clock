/*
 * Voxel - daylight over a grass line. Square stone panels with a hard
 * offset shadow instead of a blur, blocky clouds, a strip of turf under the
 * clock.
 */
#include "ui/ui.h"
#include "ui/ui_hud.h"
#include "ui/ui_theme.h"

#define BLOCK      16
#define GRASS_TOP  0x5D9C3C
#define GRASS_SIDE 0x7BB661
#define DIRT       0x8B6141
#define DIRT_DARK  0x6E4B31
#define GROUND_Y   246

static const ui_hud_theme_t THEME = {
    .ink = 0xFFFFFF, .dim = 0xC3CBD1,
    .panel = 0x3C3F44, .panel_opa = LV_OPA_90,
    .border_w = 0, .radius = 0,
    .shadow = 0x1F2226, .shadow_dx = 6, .shadow_dy = 6,
    .weather = 0x69B7EC, .news = 0xFFF06A, .hn = 0xFF8A3D, .markets = 0x7BD650,
    .up = 0x7BD650, .down = 0xFF5A4F,
    .tag_prefix = "", .tag_family = UI_FONT_MONO,
    .clock_family = UI_FONT_MONO, .clock = 0xFFFFFF, .clock_align = LV_TEXT_ALIGN_CENTER,
    .ghost = 0x2F2F2F, .ghost_opa = LV_OPA_50, .ghost_dx = 6, .ghost_dy = 7,
    .date_family = UI_FONT_MONO, .date = 0x11313F,
    .secs_style = HUD_SECS_BAR, .secs = 0xFFFFFF, .secs_track = 0x11313F,
    .secs_track_opa = LV_OPA_20,
};

static lv_obj_t *voxel_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0x69B7EC), lv_color_hex(0xAEDCF5));

    /* Blocky clouds drifting behind the digits. */
    ui_add_rect(root, 60, 34, 120, 24, 0xFFFFFF, LV_OPA_70, 0);
    ui_add_rect(root, 84, 16, 72, 20, 0xFFFFFF, LV_OPA_70, 0);
    ui_add_rect(root, 420, 96, 136, 24, 0xFFFFFF, LV_OPA_60, 0);
    ui_add_rect(root, 452, 76, 64, 20, 0xFFFFFF, LV_OPA_60, 0);

    /* Turf along the foot of the clock column: grass top, grass side, dirt. */
    for (int x = HUD_CLOCK_X; x < HUD_CLOCK_X + HUD_CLOCK_W; x += BLOCK) {
        ui_add_rect(root, x, GROUND_Y, BLOCK, 4, GRASS_TOP, LV_OPA_COVER, 0);
        ui_add_rect(root, x, GROUND_Y + 4, BLOCK, 6, GRASS_SIDE, LV_OPA_COVER, 0);
        bool dark = ((x / BLOCK) % 2) == 0;
        ui_add_rect(root, x, GROUND_Y + 10, BLOCK, 6, dark ? DIRT_DARK : DIRT,
                    LV_OPA_COVER, 0);
    }

    ui_hud_create(root, &THEME);
    return root;
}

const clock_face_t face_voxel = {
    .name = "Voxel",
    /* Light: the icons land on the dark stone of the weather panel. */
    .light_chrome = true,
    .create = voxel_create,
    .tick = ui_hud_tick,
    .tap = ui_hud_tap,
};
