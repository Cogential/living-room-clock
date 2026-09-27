/*
 * Aperture - clean test-chamber signage, with the two portal colours filling
 * the minute from opposite ends.
 */
#include "ui/ui.h"
#include "ui/ui_bands.h"
#include "ui/ui_fonts.h"
#include "ui/ui_theme.h"

#define PANEL  0xEDEFF1
#define SEAM   0xD3D8DD
#define CHAR   0x24282C
#define ORANGE 0xF28E1C
#define BLUE   0x2FA8E0

static ui_bands_t s_bands;
static lv_obj_t *s_time, *s_date, *s_blue, *s_orange;
static int s_last_minute = -1, s_last_yday = -1;

static lv_obj_t *aperture_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0xF5F7F8), lv_color_hex(0xE2E6EA));

    /* Wall panel seams on a coarse grid - the whole look. */
    for (int x = 0; x <= UI_SCREEN_W; x += 128) {
        ui_add_rect(root, x, 0, 2, UI_SCREEN_H, SEAM, LV_OPA_COVER, 0);
    }
    for (int y = 0; y <= UI_SCREEN_H; y += 128) {
        ui_add_rect(root, 0, y, UI_SCREEN_W, 2, SEAM, LV_OPA_COVER, 0);
    }

    /* Signage plate the clock sits on, filling the face's band. */
    ui_add_rect(root, 152, UI_BAND_FACE_TOP, 720,
                UI_BAND_FACE_BOTTOM - UI_BAND_FACE_TOP, PANEL, LV_OPA_COVER, 6);
    ui_add_rect(root, 152, UI_BAND_FACE_TOP, 720, 6, CHAR, LV_OPA_COVER, 0);

    s_time = ui_face_time_label(root, UI_FONT_THIN, FACE_TIME_THIN, CHAR);
    s_date = ui_face_date_label(root, 0x6C7378);

    /* Two bars closing on the middle as the minute runs out. Pinned to the
     * bottom edge of the band: the date now sits just above here, and at
     * FACE_BOTTOM - 22 the bars ran through the middle of it. */
    ui_add_rect(root, 232, UI_BAND_FACE_BOTTOM - 6, 560, 6, SEAM, LV_OPA_COVER, 3);
    s_blue = ui_add_rect(root, 232, UI_BAND_FACE_BOTTOM - 6, 0, 6, BLUE, LV_OPA_COVER, 3);
    s_orange = ui_add_rect(root, 792, UI_BAND_FACE_BOTTOM - 6, 0, 6, ORANGE, LV_OPA_COVER, 3);

    const ui_bands_palette_t palette = { .ink = CHAR, .dim = 0x6C7378, .accent = 0x1B6E9B };
    ui_bands_create(&s_bands, root, &palette);

    s_last_minute = -1; s_last_yday = -1;
    return root;
}

static void aperture_tick(const clock_time_t *now)
{
    ui_face_update_time(s_time, s_date, now, &s_last_minute, &s_last_yday);

    int elapsed = now->tm.tm_sec * 1000 + now->millis;
    int half = (280 * elapsed) / 60000;
    lv_obj_set_width(s_blue, half);
    lv_obj_set_width(s_orange, half);
    lv_obj_set_x(s_orange, 792 - half);

    ui_bands_tick(&s_bands);
}

const clock_face_t face_aperture = {
    .name = "Aperture",
    .light_chrome = false,
    .create = aperture_create,
    .tick = aperture_tick,
};
