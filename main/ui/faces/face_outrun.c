/*
 * Outrun - banded sun and a horizon, compressed into the face's own band so the
 * grid never runs under the data.
 */
#include "ui/ui.h"
#include "ui/ui_bands.h"
#include "ui/ui_fonts.h"
#include "ui/ui_theme.h"

#define MAGENTA 0xFF2E88
#define CYAN    0x2DE2E6
#define INK     0xFFF3F8
#define DIM     0x9A7FB0
#define HORIZON (UI_BAND_FACE_BOTTOM - 6)

static ui_bands_t s_bands;
static lv_obj_t *s_time, *s_date;
static int s_last_minute = -1, s_last_yday = -1;

static lv_obj_t *outrun_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0x2B0A3D), lv_color_hex(0x0B0418));

    /* Sun: a disc with bands of background punched across its lower half. It
     * sits behind the clock, entirely within the face band. */
    ui_add_rect(root, UI_SCREEN_W / 2 - 110, UI_BAND_FACE_TOP + 4, 220, 220,
                0xFF8A3D, LV_OPA_COVER, 110);
    ui_add_rect(root, UI_SCREEN_W / 2 - 110, UI_BAND_FACE_TOP + 4, 220, 110,
                MAGENTA, LV_OPA_30, 0);
    for (int i = 0; i < 5; i++) {
        int y = UI_BAND_FACE_TOP + 130 + i * 18;
        ui_add_rect(root, UI_SCREEN_W / 2 - 115, y, 230, 6 + i, 0x2B0A3D, LV_OPA_COVER, 0);
    }

    ui_add_rect(root, 0, HORIZON, UI_SCREEN_W, 3, CYAN, LV_OPA_90, 0);
    ui_add_glow(root, lv_color_hex(MAGENTA), 520, 250, HORIZON - 300, LV_OPA_20);

    s_time = ui_face_time_label(root, UI_FONT_MONO, FACE_TIME_MONO, INK);
    s_date = ui_face_date_label(root, CYAN);

    const ui_bands_palette_t palette = { .ink = INK, .dim = DIM, .accent = CYAN };
    ui_bands_create(&s_bands, root, &palette);

    s_last_minute = -1; s_last_yday = -1;
    return root;
}

static void outrun_tick(const clock_time_t *now)
{
    ui_face_update_time(s_time, s_date, now, &s_last_minute, &s_last_yday);
    ui_bands_tick(&s_bands);
}

const clock_face_t face_outrun = {
    .name = "Outrun",
    .light_chrome = true,
    .create = outrun_create,
    .tick = outrun_tick,
};
