/*
 * Phosphor - green phosphor CRT, monochrome throughout: sections are told apart
 * by their headings rather than by colour. The blinking block cursor after the
 * time is the only thing that moves.
 */
#include "ui/ui.h"
#include "ui/ui_hud.h"
#include "ui/ui_theme.h"

#define PHOS     0x4CFF7A
#define PHOS_MID 0x2FA352

/* Just past the digits, which end at x ~567 when centred, with a gap. */
#define CURSOR_X 580
#define CURSOR_Y 122

static lv_obj_t *s_cursor;

static const ui_hud_theme_t THEME = {
    .ink = PHOS, .dim = PHOS_MID,
    .panel = 0x031A0C, .panel_opa = LV_OPA_80,
    .border_w = 2, .border_opa = LV_OPA_60, .radius = 0,
    .weather = PHOS, .news = PHOS, .hn = PHOS, .markets = PHOS,
    /* Amber for a fall: the one exception to the monochrome, because a minus
     * sign alone is too easy to miss from across the room. */
    .up = PHOS, .down = 0xFFB641,
    .tag_prefix = "> ", .tag_family = UI_FONT_MONO,
    .clock_family = UI_FONT_MONO, .clock = PHOS, .clock_align = LV_TEXT_ALIGN_CENTER,
    .date_family = UI_FONT_MONO, .date = PHOS_MID,
    .secs_style = HUD_SECS_BAR, .secs = PHOS, .secs_track = PHOS, .secs_track_opa = LV_OPA_20,
};

static lv_obj_t *phosphor_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0x04120A), lv_color_hex(0x020905));
    ui_add_glow(root, lv_color_hex(PHOS), 640, -60, -200, LV_OPA_10);
    ui_add_scanlines(root, 0x000000, 6, LV_OPA_30);

    ui_hud_create(root, &THEME);

    s_cursor = ui_add_rect(root, CURSOR_X, CURSOR_Y, 16, 56, PHOS, LV_OPA_COVER, 0);
    return root;
}

static void phosphor_tick(const clock_time_t *now)
{
    ui_hud_tick(now);

    /* Half-second duty cycle, driven off the clock rather than a timer. */
    if (now->millis < 500) {
        lv_obj_remove_flag(s_cursor, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_cursor, LV_OBJ_FLAG_HIDDEN);
    }
}

const clock_face_t face_phosphor = {
    .name = "Phosphor",
    .light_chrome = true,
    .create = phosphor_create,
    .tick = phosphor_tick,
    .tap = ui_hud_tap,
};
