/*
 * Photo - a picture behind the HUD.
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
#include "ui/ui_hud.h"
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

/*
 * Dark glass panels carry the feeds, so the photo shows in the gaps and behind
 * the clock. The clock itself sits on the picture with a per-glyph shadow, which
 * keeps the darkening tight around the glyphs instead of a slab over the photo.
 */
static const ui_hud_theme_t THEME = {
    .ink = 0xFFFFFF, .dim = 0xD5DEE6,
    .panel = 0x000000, .panel_opa = LV_OPA_50,
    .border_w = 1, .border_opa = LV_OPA_50, .radius = 12,
    .weather = 0x7DD3FC, .news = 0xFDA4AF, .hn = 0xFDBA74, .markets = 0xFDE68A,
    .up = 0x86EFAC, .down = 0xFCA5A5,
    .tag_prefix = "", .tag_family = UI_FONT_MEDIUM,
    .clock_family = UI_FONT_THIN, .clock = 0xFFFFFF, .clock_align = LV_TEXT_ALIGN_CENTER,
    .text_shadow = true,
    .date_family = UI_FONT_MEDIUM, .date = 0xFFFFFF,
    .secs_style = HUD_SECS_BAR, .secs = 0xFFFFFF, .secs_track = 0x000000,
    .secs_track_opa = LV_OPA_40,
};

static lv_obj_t *photo_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);

    lv_obj_t *img = lv_image_create(root);
    lv_image_set_src(img, &s_photo);
    lv_obj_set_pos(img, 0, 0);

    ui_hud_create(root, &THEME);
    return root;
}

const clock_face_t face_photo = {
    .name = "Photo",
    .light_chrome = true,
    .create = photo_create,
    .tick = ui_hud_tick,
    .tap = ui_hud_tap,
};
