/*
 * Stardew - a wooden sign over a green field, with the sign shrunk to the
 * face's own band so the crops never crowd the data.
 */
#include <stdio.h>

#include "ui/ui.h"
#include "ui/ui_bands.h"
#include "ui/ui_fonts.h"
#include "ui/ui_theme.h"

#define WOOD_DARK  0x5A3A22
#define WOOD       0x8B5A2B
#define WOOD_LIGHT 0xB07D4A
#define CREAM      0xF7E7C4
#define LEAF       0x4F8A3D

/* The game's 28-day seasons drift from the real calendar; mapping by month
 * keeps it recognisable rather than accurate. */
static const char *SEASONS[12] = {
    "Winter", "Winter", "Spring", "Spring", "Spring", "Summer",
    "Summer", "Summer", "Fall",   "Fall",   "Fall",   "Winter",
};

static ui_bands_t s_bands;
static lv_obj_t *s_time, *s_season;
static int s_last_minute = -1, s_last_yday = -1;

static lv_obj_t *stardew_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0x8FCB6E), lv_color_hex(0x40733A));
    ui_add_glow(root, lv_color_hex(0xFFD98A), 480, 280, -240, LV_OPA_30);

    /* Bevelled sign, nested rectangles, inside the face band. */
    const int y = UI_BAND_FACE_TOP;
    const int h = UI_BAND_FACE_BOTTOM - UI_BAND_FACE_TOP;
    ui_add_rect(root, 236, y,      552, h,      WOOD_DARK,  LV_OPA_COVER, 14);
    ui_add_rect(root, 246, y + 10, 532, h - 20, WOOD,       LV_OPA_COVER, 12);
    ui_add_rect(root, 256, y + 20, 512, h - 40, WOOD_LIGHT, LV_OPA_COVER, 10);
    ui_add_rect(root, 266, y + 30, 492, h - 60, WOOD,       LV_OPA_COVER, 8);

    /* A couple of crops either side of the posts. */
    for (int i = 0; i < 3; i++) {
        int x = 150 + i * 26;
        ui_add_rect(root, x, y + 96, 5, 34, LEAF, LV_OPA_COVER, 2);
        ui_add_rect(root, x - 7, y + 88, 19, 11, 0xE86A6A, LV_OPA_COVER, 4);
        int rx = UI_SCREEN_W - 170 - i * 26;
        ui_add_rect(root, rx, y + 96, 5, 34, LEAF, LV_OPA_COVER, 2);
        ui_add_rect(root, rx - 7, y + 88, 19, 11, 0xF2C14E, LV_OPA_COVER, 4);
    }

    s_time = ui_face_time_label(root, UI_FONT_MONO, FACE_TIME_MONO, CREAM);
    s_season = ui_face_date_label(root, 0x3B2716);

    const ui_bands_palette_t palette = { .ink = 0x24331C, .dim = 0x4A5C3A, .accent = 0x2F4A22 };
    ui_bands_create(&s_bands, root, &palette);

    s_last_minute = -1; s_last_yday = -1;
    return root;
}

static void stardew_tick(const clock_time_t *now)
{
    /* Its own date line: the in-game calendar, not a weekday. */
    ui_face_update_time(s_time, NULL, now, &s_last_minute, &s_last_yday);

    if (now->tm.tm_yday != s_last_yday) {
        s_last_yday = now->tm.tm_yday;
        lv_label_set_text_fmt(s_season, "%s %d",
                              SEASONS[now->tm.tm_mon % 12], now->tm.tm_mday);
    }

    ui_bands_tick(&s_bands);
}

const clock_face_t face_stardew = {
    .name = "Stardew",
    .light_chrome = false,
    .create = stardew_create,
    .tick = stardew_tick,
};
