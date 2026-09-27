/*
 * Cyberpunk - hazard yellow on near-black, a magenta ghost behind the numerals,
 * cyan corner brackets and scanlines over the lot.
 */
#include <stdio.h>

#include "ui/ui.h"
#include "ui/ui_bands.h"
#include "ui/ui_fonts.h"
#include "ui/ui_theme.h"

#define YELLOW  0xFCEE0A
#define CYAN    0x00F0FF
#define MAGENTA 0xFF003C
#define INK     0xE8F4F8
#define DIM     0x6F7F8A

static ui_bands_t s_bands;
static lv_obj_t *s_time, *s_ghost, *s_date;
static int s_last_minute = -1, s_last_yday = -1;

static lv_obj_t *cyberpunk_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0x05060A), lv_color_hex(0x0D0716));

    ui_add_glow(root, lv_color_hex(MAGENTA), 520, -180, 240, LV_OPA_10);
    ui_add_glow(root, lv_color_hex(CYAN), 460, 760, -140, LV_OPA_10);
    ui_add_corner_brackets(root, 20, 52, 4, CYAN, LV_OPA_70);

    /* Hazard stripe marking the top of the face's own band. */
    for (int x = 300; x < 724; x += 34) {
        ui_add_rect(root, x, UI_BAND_FACE_TOP - 6, 18, 5, YELLOW, LV_OPA_40, 0);
    }

    /* Off-register duplicate behind the numerals sells the chromatic split
     * without any per-pixel work. */
    s_ghost = ui_face_time_label(root, UI_FONT_MONO, FACE_TIME_MONO, MAGENTA);
    lv_obj_set_style_text_opa(s_ghost, LV_OPA_50, 0);
    lv_obj_set_pos(s_ghost, 54, UI_BAND_TIME_Y + 5);

    s_time = ui_face_time_label(root, UI_FONT_MONO, FACE_TIME_MONO, YELLOW);
    s_date = ui_face_date_label(root, CYAN);

    const ui_bands_palette_t palette = { .ink = INK, .dim = DIM, .accent = CYAN };
    ui_bands_create(&s_bands, root, &palette);

    ui_add_scanlines(root, 0x000000, 6, LV_OPA_20);

    s_last_minute = -1; s_last_yday = -1;
    return root;
}

static void cyberpunk_tick(const clock_time_t *now)
{
    int before = s_last_minute;
    ui_face_update_time(s_time, s_date, now, &s_last_minute, &s_last_yday);
    if (before != s_last_minute) {
        lv_label_set_text(s_ghost, lv_label_get_text(s_time));
    }
    ui_bands_tick(&s_bands);
}

const clock_face_t face_cyberpunk = {
    .name = "Cyberpunk",
    .light_chrome = true,
    .create = cyberpunk_create,
    .tick = cyberpunk_tick,
};
