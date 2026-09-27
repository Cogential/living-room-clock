/*
 * Pip-Boy - green phosphor CRT. Rules bracket the face's own band, and the
 * blinking block cursor is the only thing that moves.
 */
#include "ui/ui.h"
#include "ui/ui_bands.h"
#include "ui/ui_fonts.h"
#include "ui/ui_theme.h"

#define PHOS     0x4CFF7A
#define PHOS_DIM 0x1E6B36

static ui_bands_t s_bands;
static lv_obj_t *s_time, *s_date, *s_cursor;
static int s_last_minute = -1, s_last_yday = -1;

static lv_obj_t *pipboy_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0x04120A), lv_color_hex(0x020905));
    ui_add_glow(root, lv_color_hex(PHOS), 640, 190, 90, LV_OPA_10);

    ui_add_rect(root, 60, UI_BAND_FACE_TOP - 6, UI_SCREEN_W - 120, 3, PHOS, LV_OPA_80, 0);
    ui_add_rect(root, 60, UI_BAND_FACE_BOTTOM, UI_SCREEN_W - 120, 3, PHOS, LV_OPA_80, 0);

    s_time = ui_face_time_label(root, UI_FONT_MONO, FACE_TIME_MONO, PHOS);
    s_date = ui_face_date_label(root, PHOS_DIM);

    /* Block cursor parked just past the end of the clock line. The numerals
     * reach x 774 at FACE_TIME_MONO, so this has to start beyond that or the
     * cursor lands on top of the minutes. */
    s_cursor = ui_add_rect(root, UI_SCREEN_W / 2 + 272, UI_BAND_TIME_Y + 134,
                           30, 60, PHOS, LV_OPA_COVER, 0);

    const ui_bands_palette_t palette = { .ink = PHOS, .dim = PHOS_DIM, .accent = PHOS };
    ui_bands_create(&s_bands, root, &palette);

    ui_add_scanlines(root, 0x000000, 6, LV_OPA_30);

    s_last_minute = -1; s_last_yday = -1;
    return root;
}

static void pipboy_tick(const clock_time_t *now)
{
    ui_face_update_time(s_time, s_date, now, &s_last_minute, &s_last_yday);

    /* Half-second duty cycle, driven off the clock rather than a timer. */
    if (now->millis < 500) {
        lv_obj_remove_flag(s_cursor, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_cursor, LV_OBJ_FLAG_HIDDEN);
    }

    ui_bands_tick(&s_bands);
}

const clock_face_t face_pipboy = {
    .name = "Pip-Boy",
    .light_chrome = true,
    .create = pipboy_create,
    .tick = pipboy_tick,
};
