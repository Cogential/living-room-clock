/*
 * Outrun - a banded sunset sinking behind the clock onto a neon horizon, violet
 * panels with hot accents.
 */
#include "ui/ui.h"
#include "ui/ui_hud.h"
#include "ui/ui_theme.h"

#define MAGENTA 0xFF2E88
#define CYAN    0x2DE2E6
#define INK     0xFFF3F8
#define DIM     0xB39CCB
#define SKY     0x23082F   /* the gradient's colour behind the sun */

#define SUN_D   220
#define SUN_X   (HUD_CLOCK_X + HUD_CLOCK_W / 2 - SUN_D / 2)
#define SUN_Y   8
#define HORIZON 226

static const ui_hud_theme_t THEME = {
    .ink = INK, .dim = DIM,
    .panel = 0x1A0730, .panel_opa = LV_OPA_80,
    .border_w = 1, .border_opa = LV_OPA_60, .radius = 0,
    .weather = CYAN, .news = MAGENTA, .hn = 0xFF8A3D, .markets = 0xFFD319,
    .up = 0x5CFFB0, .down = 0xFF4F7B,
    .tag_prefix = "", .tag_family = UI_FONT_MONO,
    .clock_family = UI_FONT_MONO, .clock = INK, .clock_align = LV_TEXT_ALIGN_CENTER,
    .ghost = CYAN, .ghost_opa = LV_OPA_40, .ghost_dx = -4, .ghost_dy = 4,
    .date_family = UI_FONT_MONO, .date = CYAN,
    .secs_style = HUD_SECS_BAR, .secs = MAGENTA, .secs_track = DIM, .secs_track_opa = LV_OPA_20,
};

static lv_obj_t *outrun_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0x2B0A3D), lv_color_hex(0x0B0418));
    ui_add_glow(root, lv_color_hex(MAGENTA), 520, 50, HORIZON - 330, LV_OPA_20);

    /* Sun behind the clock: a disc with bands of sky punched across its lower
     * half, cut off at the horizon. Half transparent - opaque, it sat behind the
     * middle of the digits and the date and made both hard to read. */
    ui_add_rect(root, SUN_X, SUN_Y, SUN_D, HORIZON - SUN_Y, 0xFF8A3D, LV_OPA_50, SUN_D / 2);
    for (int i = 0; i < 5; i++) {
        ui_add_rect(root, SUN_X - 4, SUN_Y + 124 + i * 18, SUN_D + 8, 6 + i, SKY,
                    LV_OPA_COVER, 0);
    }
    ui_add_rect(root, HUD_CLOCK_X, HORIZON, HUD_CLOCK_W, 3, CYAN, LV_OPA_90, 0);

    ui_hud_create(root, &THEME);
    return root;
}

const clock_face_t face_outrun = {
    .name = "Outrun",
    .light_chrome = true,
    .create = outrun_create,
    .tick = ui_hud_tick,
    .tap = ui_hud_tap,
};
