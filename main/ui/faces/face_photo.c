/*
 * Photo - a picture behind the clock.
 *
 * The image is stored as raw RGB565 at exactly panel size and resolution, so
 * LVGL blits it straight out of flash with no decode step and no PSRAM copy.
 * A JPEG would be a tenth of the size, but it would cost a decode on every
 * face load and pull in a decoder path; 1.2 MB of an 8 MB partition is the
 * cheaper trade here.
 *
 * To swap the picture, re-run the conversion in the README and rebuild.
 */
#include "ui/ui.h"
#include "ui/ui_bands.h"
#include "ui/ui_fonts.h"
#include "ui/ui_theme.h"

/* Embedded by EMBED_FILES in main/CMakeLists.txt. */
extern const uint8_t photo_start[] asm("_binary_photo_rgb565_start");

#define PHOTO_W 1024
#define PHOTO_H 600

static const lv_image_dsc_t s_photo = {
    .header = {
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_RGB565,
        .w = PHOTO_W,
        .h = PHOTO_H,
        .stride = PHOTO_W * 2,
    },
    .data_size = PHOTO_W * PHOTO_H * 2,
    .data = photo_start,
};

/* Brighter than the other faces' secondary tones: with no scrim underneath, a
 * mid grey disappears into foliage. */
#define INK    0xFFFFFF
#define DIM    0xE4EAF0
#define ACCENT 0xBFEEFF

static ui_bands_t s_bands;
static lv_obj_t *s_time, *s_date;
static int s_last_minute = -1, s_last_yday = -1;

static lv_obj_t *photo_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);

    lv_obj_t *img = lv_image_create(root);
    lv_image_set_src(img, &s_photo);
    lv_obj_set_pos(img, 0, 0);

    /*
     * Nothing is drawn over the photo - no scrims, no tint. The picture is the
     * point of this face, and a translucent slab across the top and bottom reads
     * as a black border rather than as a background.
     *
     * All of the legibility therefore comes from the text itself, via a per-glyph
     * drop shadow. That keeps the darkening tight around the glyphs, where it
     * does the work, instead of spread over a third of the panel.
     */
    s_time = ui_face_time_label(root, UI_FONT_THIN, FACE_TIME_THIN, INK);
    ui_text_shadow(s_time, 8);

    s_date = ui_face_date_label(root, INK);
    ui_text_shadow(s_date, 5);

    const ui_bands_palette_t palette = {
        .ink = INK, .dim = DIM, .accent = ACCENT, .shadow = true,
    };
    ui_bands_create(&s_bands, root, &palette);

    s_last_minute = -1;
    s_last_yday = -1;
    return root;
}

static void photo_tick(const clock_time_t *now)
{
    ui_face_update_time(s_time, s_date, now, &s_last_minute, &s_last_yday);
    ui_bands_tick(&s_bands);
}

const clock_face_t face_photo = {
    .name = "Photo",
    .light_chrome = true,
    .create = photo_create,
    .tick = photo_tick,
};
