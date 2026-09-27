/*
 * Blockcraft - daylight over a grass line, everything on a 32 px grid with
 * square corners and a hard offset shadow instead of a blur.
 */
#include "ui/ui.h"
#include "ui/ui_bands.h"
#include "ui/ui_fonts.h"
#include "ui/ui_theme.h"

#define BLOCK      32
#define GRASS_TOP  0x5D9C3C
#define GRASS_SIDE 0x7BB661
#define DIRT       0x8B6141
#define DIRT_DARK  0x6E4B31

static ui_bands_t s_bands;
static lv_obj_t *s_time, *s_shadow, *s_date;
static int s_last_minute = -1, s_last_yday = -1;

static lv_obj_t *blockcraft_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0x69B7EC), lv_color_hex(0xAEDCF5));

    /* Blocky sun and a cloud, up in the sky either side of the weather band. */
    ui_add_rect(root, 84, 78, 64, 64, 0xFFF06A, LV_OPA_COVER, 0);
    ui_add_rect(root, 72, 90, 88, 40, 0xFFF06A, LV_OPA_COVER, 0);
    ui_add_rect(root, 830, 92, 112, 28, 0xFFFFFF, LV_OPA_80, 0);
    ui_add_rect(root, 858, 64, 84, 28, 0xFFFFFF, LV_OPA_80, 0);

    /* Ground: one row of grass tops then dirt, sitting on the floor of the
     * face's own band rather than across the whole panel. */
    const int ground_y = UI_BAND_FACE_BOTTOM - BLOCK - 8;
    for (int x = 0; x < UI_SCREEN_W; x += BLOCK) {
        ui_add_rect(root, x, ground_y, BLOCK, 8, GRASS_TOP, LV_OPA_COVER, 0);
        ui_add_rect(root, x, ground_y + 8, BLOCK, BLOCK, GRASS_SIDE, LV_OPA_COVER, 0);
        bool dark = ((x / BLOCK) % 2) == 0;
        ui_add_rect(root, x, ground_y + 8 + BLOCK, BLOCK, 8,
                    dark ? DIRT_DARK : DIRT, LV_OPA_COVER, 0);
    }

    s_shadow = ui_face_time_label(root, UI_FONT_MONO, FACE_TIME_MONO, 0x2F2F2F);
    lv_obj_set_style_text_opa(s_shadow, LV_OPA_50, 0);
    lv_obj_set_pos(s_shadow, 66, UI_BAND_TIME_Y + 7);

    s_time = ui_face_time_label(root, UI_FONT_MONO, FACE_TIME_MONO, 0xFFFFFF);
    s_date = ui_face_date_label(root, 0xFFFFFF);

    const ui_bands_palette_t palette = { .ink = 0x11313F, .dim = 0x2D4E5C, .accent = 0x11313F };
    ui_bands_create(&s_bands, root, &palette);

    s_last_minute = -1; s_last_yday = -1;
    return root;
}

static void blockcraft_tick(const clock_time_t *now)
{
    int before = s_last_minute;
    ui_face_update_time(s_time, s_date, now, &s_last_minute, &s_last_yday);
    if (before != s_last_minute) {
        lv_label_set_text(s_shadow, lv_label_get_text(s_time));
    }
    ui_bands_tick(&s_bands);
}

const clock_face_t face_blockcraft = {
    .name = "Blockcraft",
    .light_chrome = false,
    .create = blockcraft_create,
    .tick = blockcraft_tick,
};
