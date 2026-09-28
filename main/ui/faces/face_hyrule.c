/*
 * Hyrule - gold on forest green, a small triangle motif crowning the clock and
 * a gold seconds bar beneath it.
 */
#include "ui/ui.h"
#include "ui/ui_hud.h"
#include "ui/ui_theme.h"

#define GOLD     0xD4AF37
#define GOLD_DIM 0x8A7326
#define PARCH    0xF0E3BC

#define TRI      14   /* side, px */

static const ui_hud_theme_t THEME = {
    .ink = PARCH, .dim = 0x8FA98C,
    .panel = 0x0B2418, .panel_opa = LV_OPA_80,
    .border_w = 1, .border_opa = LV_OPA_60, .radius = 6,
    .weather = 0x8FD694, .news = GOLD, .hn = 0xE0A458, .markets = 0x9CC9E0,
    .up = 0x8FD694, .down = 0xE06C5C,
    .tag_prefix = "", .tag_family = UI_FONT_MEDIUM,
    .clock_family = UI_FONT_THIN, .clock = PARCH, .clock_align = LV_TEXT_ALIGN_CENTER,
    .date_family = UI_FONT_MEDIUM, .date = GOLD,
    .secs_style = HUD_SECS_BAR, .secs = GOLD, .secs_track = 0x24422F,
    .secs_track_opa = LV_OPA_COVER,
};

/*
 * An outlined triangle from a closed polyline, sized to its own bounding box
 * rather than the screen. The points must outlive the call.
 */
static void add_triangle(lv_obj_t *root, int cx, int top, uint32_t colour)
{
    static lv_point_precise_t pts[3][4];
    static int slot;
    lv_point_precise_t *p = pts[slot++ % 3];

    const int h = TRI * 866 / 1000;
    p[0].x = TRI / 2 + 1; p[0].y = 1;
    p[1].x = 1;           p[1].y = h + 1;
    p[2].x = TRI + 1;     p[2].y = h + 1;
    p[3] = p[0];

    lv_obj_t *line = lv_line_create(root);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, cx - TRI / 2 - 1, top);
    lv_obj_set_size(line, TRI + 3, h + 3);
    lv_obj_set_style_line_width(line, 2, 0);
    lv_obj_set_style_line_color(line, lv_color_hex(colour), 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_line_set_points(line, p, 4);
}

static lv_obj_t *hyrule_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0x123524), lv_color_hex(0x061410));
    ui_add_glow(root, lv_color_hex(GOLD), 480, 70, -230, LV_OPA_10);

    /* Three triangles stacked into a larger one, above the digits (y ~26). */
    const int cx = HUD_CLOCK_X + HUD_CLOCK_W / 2;
    const int h = TRI * 866 / 1000;
    add_triangle(root, cx, 0, GOLD);
    add_triangle(root, cx - TRI / 2, h, GOLD_DIM);
    add_triangle(root, cx + TRI / 2, h, GOLD_DIM);

    ui_hud_create(root, &THEME);
    return root;
}

const clock_face_t face_hyrule = {
    .name = "Hyrule",
    .light_chrome = true,
    .create = hyrule_create,
    .tick = ui_hud_tick,
    .tap = ui_hud_tap,
};
