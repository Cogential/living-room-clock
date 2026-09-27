/*
 * Analog - a proper dial with a smoothly sweeping second hand, sized to sit
 * entirely within the face's own band.
 *
 * Ticks and hands are positioned with trig rather than rotated objects: LVGL's
 * transforms would resample the bitmap, and hairline hands show that badly.
 */
#include <math.h>

#include "ui/ui.h"
#include "ui/ui_bands.h"
#include "ui/ui_fonts.h"
#include "ui/ui_theme.h"

#define CX        512
#define CY        ((UI_BAND_FACE_TOP + UI_BAND_FACE_BOTTOM) / 2)
#define R_OUTER   112
#define R_DOTS    98
#define R_HOUR_IN 84

#define LEN_HOUR    54
#define LEN_MINUTE  82
#define LEN_SECOND  92
#define TAIL_SECOND 20

#define W_HOUR   7
#define W_MINUTE 5
#define W_SECOND 2

#define DEG2RAD 0.017453292519943295f

static ui_bands_t s_bands;
static lv_obj_t *s_hour, *s_minute, *s_second, *s_date;
static int s_last_minute = -1, s_last_yday = -1;

static lv_point_precise_t s_hour_pts[2];
static lv_point_precise_t s_minute_pts[2];
static lv_point_precise_t s_second_pts[2];
static lv_point_precise_t s_marker_pts[12][2];

static lv_obj_t *make_hand(lv_obj_t *parent, lv_point_precise_t *pts, int width,
                           uint32_t colour)
{
    lv_obj_t *line = lv_line_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_style_line_width(line, width, 0);
    lv_obj_set_style_line_color(line, lv_color_hex(colour), 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_line_set_points(line, pts, 2);
    return line;
}

/*
 * Points are kept relative to a box that just contains the hand, and the object
 * is moved to match. A full-screen line object would be simpler, but LVGL
 * invalidates by object bounds: a screen-sized hand repaints all 1024x600 every
 * frame, which measured at half the frame rate of the other faces.
 */
static void set_hand(lv_obj_t *hand, lv_point_precise_t *pts, float deg,
                     int length, int tail, int width)
{
    float rad = (deg - 90.0f) * DEG2RAD;
    float sx = cosf(rad), sy = sinf(rad);

    int x0 = (int)(CX - sx * tail), y0 = (int)(CY - sy * tail);
    int x1 = (int)(CX + sx * length), y1 = (int)(CY + sy * length);

    int pad = width / 2 + 1;
    int min_x = LV_MIN(x0, x1) - pad, min_y = LV_MIN(y0, y1) - pad;
    int max_x = LV_MAX(x0, x1) + pad, max_y = LV_MAX(y0, y1) + pad;

    pts[0].x = x0 - min_x; pts[0].y = y0 - min_y;
    pts[1].x = x1 - min_x; pts[1].y = y1 - min_y;

    lv_obj_set_pos(hand, min_x, min_y);
    lv_obj_set_size(hand, max_x - min_x, max_y - min_y);
    lv_line_set_points(hand, pts, 2);
}

static void add_dial(lv_obj_t *root)
{
    ui_add_rect(root, CX - R_OUTER - 8, CY - R_OUTER - 8, (R_OUTER + 8) * 2,
                (R_OUTER + 8) * 2, 0x05070A, LV_OPA_40, R_OUTER + 8);

    lv_obj_t *bezel = ui_add_rect(root, CX - R_OUTER, CY - R_OUTER, R_OUTER * 2,
                                  R_OUTER * 2, 0x14161C, LV_OPA_COVER, R_OUTER);
    lv_obj_set_style_border_width(bezel, 2, 0);
    lv_obj_set_style_border_color(bezel, lv_color_hex(0x2A2E38), 0);

    for (int i = 0; i < 60; i++) {
        if (i % 5 == 0) continue;   /* hour positions get a line instead */
        float rad = (i * 6.0f - 90.0f) * DEG2RAD;
        int x = (int)(CX + cosf(rad) * R_DOTS), y = (int)(CY + sinf(rad) * R_DOTS);
        ui_add_rect(root, x - 1, y - 1, 3, 3, 0x4A5060, LV_OPA_COVER, 2);
    }

    for (int i = 0; i < 12; i++) {
        float rad = (i * 30.0f - 90.0f) * DEG2RAD;
        s_marker_pts[i][0].x = (int32_t)(CX + cosf(rad) * R_HOUR_IN);
        s_marker_pts[i][0].y = (int32_t)(CY + sinf(rad) * R_HOUR_IN);
        s_marker_pts[i][1].x = (int32_t)(CX + cosf(rad) * (R_DOTS + 5));
        s_marker_pts[i][1].y = (int32_t)(CY + sinf(rad) * (R_DOTS + 5));

        lv_obj_t *marker = lv_line_create(root);
        lv_obj_remove_style_all(marker);
        lv_obj_set_pos(marker, 0, 0);
        lv_obj_set_size(marker, UI_SCREEN_W, UI_SCREEN_H);
        lv_obj_set_style_line_width(marker, i % 3 == 0 ? 4 : 2, 0);
        lv_obj_set_style_line_color(marker, lv_color_hex(0xE2E6F0), 0);
        lv_obj_set_style_line_rounded(marker, true, 0);
        lv_line_set_points(marker, s_marker_pts[i], 2);
    }
}

static lv_obj_t *analog_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0x0D0F14), lv_color_hex(0x171A22));

    add_dial(root);

    s_hour = make_hand(root, s_hour_pts, W_HOUR, 0xF2F4FA);
    s_minute = make_hand(root, s_minute_pts, W_MINUTE, 0xF2F4FA);
    s_second = make_hand(root, s_second_pts, W_SECOND, 0xFF6B4A);

    lv_obj_t *cap = ui_add_rect(root, CX - 7, CY - 7, 14, 14, 0xFF6B4A, LV_OPA_COVER, 7);
    lv_obj_set_style_border_width(cap, 2, 0);
    lv_obj_set_style_border_color(cap, lv_color_hex(0x14161C), 0);

    s_date = ui_face_date_label(root, 0x8A91A6);

    const ui_bands_palette_t palette = { .ink = 0xE2E6F0, .dim = 0x8A91A6, .accent = 0xFF6B4A };
    ui_bands_create(&s_bands, root, &palette);

    s_last_minute = -1; s_last_yday = -1;
    return root;
}

static void analog_tick(const clock_time_t *now)
{
    float seconds = now->tm.tm_sec + now->millis / 1000.0f;
    float minutes = now->tm.tm_min + seconds / 60.0f;
    float hours = (now->tm.tm_hour % 12) + minutes / 60.0f;

    set_hand(s_hour, s_hour_pts, hours * 30.0f, LEN_HOUR, 0, W_HOUR);
    set_hand(s_minute, s_minute_pts, minutes * 6.0f, LEN_MINUTE, 0, W_MINUTE);
    set_hand(s_second, s_second_pts, seconds * 6.0f, LEN_SECOND, TAIL_SECOND, W_SECOND);

    /* No digital time on this face; the dial is the clock. */
    if (now->tm.tm_yday != s_last_yday) {
        s_last_yday = now->tm.tm_yday;
        char buf[64];
        strftime(buf, sizeof(buf), "%A  %e %B", &now->tm);
        for (char *p = buf; *p; p++) {
            if (*p >= 'a' && *p <= 'z') *p -= 32;
        }
        lv_label_set_text(s_date, buf);
    }
    (void)s_last_minute;

    ui_bands_tick(&s_bands);
}

const clock_face_t face_analog = {
    .name = "Analog",
    .light_chrome = true,
    .create = analog_create,
    .tick = analog_tick,
};
