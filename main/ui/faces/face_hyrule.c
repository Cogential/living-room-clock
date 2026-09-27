/*
 * Hyrule - gold on forest green, a triangle motif above the clock and a gold
 * seconds ring sweeping behind it.
 */
#include "ui/ui.h"
#include "ui/ui_bands.h"
#include "ui/ui_fonts.h"
#include "ui/ui_theme.h"

#define GOLD     0xD4AF37
#define GOLD_DIM 0x8A7326
#define PARCH    0xF0E3BC

static ui_bands_t s_bands;
static lv_obj_t *s_time, *s_date, *s_arc;
static int s_last_minute = -1, s_last_yday = -1;

/* An outlined triangle from three lines; the points must outlive the call. */
static void add_triangle(lv_obj_t *root, int cx, int cy, int size, uint32_t colour)
{
    static lv_point_precise_t pts[3][4];
    static int slot = 0;
    lv_point_precise_t *p = pts[slot % 3];
    slot++;

    int h = (int)(size * 0.866f);
    p[0].x = cx;            p[0].y = cy - h / 2;
    p[1].x = cx - size / 2; p[1].y = cy + h / 2;
    p[2].x = cx + size / 2; p[2].y = cy + h / 2;
    p[3] = p[0];

    lv_obj_t *line = lv_line_create(root);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, 0, 0);
    lv_obj_set_size(line, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_line_width(line, 3, 0);
    lv_obj_set_style_line_color(line, lv_color_hex(colour), 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_line_set_points(line, p, 4);
}

static lv_obj_t *hyrule_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0x123524), lv_color_hex(0x061410));
    ui_add_glow(root, lv_color_hex(GOLD), 480, 270, -230, LV_OPA_10);

    /* Three triangles stacked into a larger one, at the top of the band. */
    add_triangle(root, UI_SCREEN_W / 2, UI_BAND_FACE_TOP + 18, 40, GOLD);
    add_triangle(root, UI_SCREEN_W / 2 - 22, UI_BAND_FACE_TOP + 52, 40, GOLD_DIM);
    add_triangle(root, UI_SCREEN_W / 2 + 22, UI_BAND_FACE_TOP + 52, 40, GOLD_DIM);

    /* Seconds ring, sized to sit inside the face's band. */
    s_arc = lv_arc_create(root);
    lv_obj_set_size(s_arc, 228, 228);
    lv_obj_set_pos(s_arc, UI_SCREEN_W / 2 - 114,
                   (UI_BAND_FACE_TOP + UI_BAND_FACE_BOTTOM) / 2 - 114);
    lv_obj_remove_style(s_arc, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_rotation(s_arc, 270);
    lv_arc_set_bg_angles(s_arc, 0, 360);
    lv_arc_set_range(s_arc, 0, 60000);
    lv_obj_set_style_arc_width(s_arc, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(0x24422F), LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(GOLD), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(s_arc, true, LV_PART_INDICATOR);

    s_time = ui_face_time_label(root, UI_FONT_THIN, FACE_TIME_THIN, PARCH);
    s_date = ui_face_date_label(root, GOLD);

    const ui_bands_palette_t palette = { .ink = PARCH, .dim = 0x8FA98C, .accent = GOLD };
    ui_bands_create(&s_bands, root, &palette);

    s_last_minute = -1; s_last_yday = -1;
    return root;
}

static void hyrule_tick(const clock_time_t *now)
{
    lv_arc_set_value(s_arc, now->tm.tm_sec * 1000 + now->millis);
    ui_face_update_time(s_time, s_date, now, &s_last_minute, &s_last_yday);
    ui_bands_tick(&s_bands);
}

const clock_face_t face_hyrule = {
    .name = "Hyrule",
    .light_chrome = true,
    .create = hyrule_create,
    .tick = hyrule_tick,
};
