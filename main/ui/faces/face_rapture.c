/*
 * Rapture - art deco under the sea. Brass on oxidised teal, stepped ziggurats
 * bracketing the clock, fluted columns down the sides.
 */
#include "ui/ui.h"
#include "ui/ui_bands.h"
#include "ui/ui_fonts.h"
#include "ui/ui_theme.h"

#define BRASS 0xC9A227
#define CREAM 0xF2E4C2
#define DIM   0x7E9A93

static ui_bands_t s_bands;
static lv_obj_t *s_time, *s_date;
static int s_last_minute = -1, s_last_yday = -1;

/* Stepped deco ornament: three bars narrowing away from the centre. */
static void add_ziggurat(lv_obj_t *root, int y, int dir)
{
    static const int widths[3] = { 150, 108, 66 };
    for (int i = 0; i < 3; i++) {
        ui_add_rect(root, UI_SCREEN_W / 2 - widths[i] / 2, y + dir * (i * 9),
                    widths[i], 4, BRASS, LV_OPA_80, 0);
    }
}

static lv_obj_t *rapture_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0x0A2A2E), lv_color_hex(0x06181C));
    ui_add_glow(root, lv_color_hex(0x2FA98F), 560, 230, -250, LV_OPA_10);

    for (int i = 0; i < 5; i++) {
        ui_add_rect(root, 40 + i * 12, UI_BAND_FACE_TOP, 4,
                    UI_BAND_FACE_BOTTOM - UI_BAND_FACE_TOP, BRASS, LV_OPA_30, 0);
        ui_add_rect(root, UI_SCREEN_W - 44 - i * 12, UI_BAND_FACE_TOP, 4,
                    UI_BAND_FACE_BOTTOM - UI_BAND_FACE_TOP, BRASS, LV_OPA_30, 0);
    }

    add_ziggurat(root, UI_BAND_FACE_TOP + 2, 1);
    add_ziggurat(root, UI_BAND_FACE_BOTTOM - 6, -1);

    s_time = ui_face_time_label(root, UI_FONT_THIN, FACE_TIME_THIN, CREAM);
    s_date = ui_face_date_label(root, BRASS);

    const ui_bands_palette_t palette = { .ink = CREAM, .dim = DIM, .accent = BRASS };
    ui_bands_create(&s_bands, root, &palette);

    s_last_minute = -1; s_last_yday = -1;
    return root;
}

static void rapture_tick(const clock_time_t *now)
{
    ui_face_update_time(s_time, s_date, now, &s_last_minute, &s_last_yday);
    ui_bands_tick(&s_bands);
}

const clock_face_t face_rapture = {
    .name = "Rapture",
    .light_chrome = true,
    .create = rapture_create,
    .tick = rapture_tick,
};
