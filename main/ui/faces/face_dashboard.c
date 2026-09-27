/*
 * Dashboard - the plainest presentation of the shared data bands: a quiet
 * gradient, the clock in the middle, and nothing else competing with it.
 */
#include <stdio.h>

#include "ui/ui.h"
#include "ui/ui_bands.h"
#include "ui/ui_fonts.h"
#include "ui/ui_theme.h"

#define INK    0xF2F5FA
#define DIM    0x8B95A7
#define ACCENT 0x7DD3FC

static ui_bands_t s_bands;
static lv_obj_t *s_time;
static lv_obj_t *s_date;
static int s_last_minute = -1;
static int s_last_yday = -1;

static lv_obj_t *dashboard_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0x0B1020), lv_color_hex(0x141A2E));
    ui_add_glow(root, lv_color_hex(0x3B82F6), 560, -200, -260, LV_OPA_10);

    s_time = ui_face_time_label(root, UI_FONT_THIN, FACE_TIME_THIN, INK);
    s_date = ui_face_date_label(root, DIM);

    const ui_bands_palette_t palette = { .ink = INK, .dim = DIM, .accent = ACCENT };
    ui_bands_create(&s_bands, root, &palette);

    s_last_minute = -1;
    s_last_yday = -1;
    return root;
}

static void dashboard_tick(const clock_time_t *now)
{
    ui_face_update_time(s_time, s_date, now, &s_last_minute, &s_last_yday);
    ui_bands_tick(&s_bands);
}

const clock_face_t face_dashboard = {
    .name = "Dashboard",
    .light_chrome = true,
    .create = dashboard_create,
    .tick = dashboard_tick,
};
