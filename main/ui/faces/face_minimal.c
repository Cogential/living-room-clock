/*
 * Minimal - warm paper and hairline ink. The one that reads in daylight, and
 * the quietest setting for the shared data bands.
 */
#include "ui/ui.h"
#include "ui/ui_bands.h"
#include "ui/ui_fonts.h"
#include "ui/ui_theme.h"

#define INK    0x1A1917
#define DIM    0x9A958C
#define ACCENT 0x7A736A

static ui_bands_t s_bands;
static lv_obj_t *s_time, *s_date;
static int s_last_minute = -1, s_last_yday = -1;

static lv_obj_t *minimal_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0xFBFAF7), lv_color_hex(0xEFEDE7));

    /* Two hairlines bracket the clock and mark where the face's own space
     * begins and ends. */
    ui_add_rect(root, 300, UI_BAND_FACE_TOP - 4, 424, 1, 0xD8D3CA, LV_OPA_COVER, 0);
    ui_add_rect(root, 300, UI_BAND_FACE_BOTTOM, 424, 1, 0xD8D3CA, LV_OPA_COVER, 0);

    s_time = ui_face_time_label(root, UI_FONT_THIN, FACE_TIME_THIN, INK);
    lv_obj_set_style_text_letter_space(s_time, -3, 0);
    s_date = ui_face_date_label(root, DIM);

    const ui_bands_palette_t palette = { .ink = INK, .dim = DIM, .accent = ACCENT };
    ui_bands_create(&s_bands, root, &palette);

    s_last_minute = -1; s_last_yday = -1;
    return root;
}

static void minimal_tick(const clock_time_t *now)
{
    ui_face_update_time(s_time, s_date, now, &s_last_minute, &s_last_yday);
    ui_bands_tick(&s_bands);
}

const clock_face_t face_minimal = {
    .name = "Minimal",
    .light_chrome = false,
    .create = minimal_create,
    .tick = minimal_tick,
};
