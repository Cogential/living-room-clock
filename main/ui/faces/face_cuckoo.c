/*
 * Cuckoo - a carved clock case beside the numerals. The pendulum swings
 * continuously; on the hour and the half hour the doors open and the bird comes
 * out, and once when the face is opened so picking it shows what it does.
 *
 * The clock is left-aligned to free a narrow case at the right edge of the
 * clock column (digits end at x ~541, the weather panel starts at 616).
 *
 * The animation is driven from the frame tick rather than lv_anim, so the whole
 * sequence is a pure function of elapsed time: it cannot stick half-open if a
 * frame is dropped, and switching face mid-call leaves nothing behind.
 */
#include <math.h>

#include "ui/ui.h"
#include "ui/ui_hud.h"
#include "ui/ui_fonts.h"
#include "ui/ui_theme.h"

#define WOOD_DARK 0x4A2F1B
#define WOOD      0x6E4526
#define TRIM      0xC79A4B
#define CREAM     0xF4E3C1
#define BIRD      0xD9853B

#define CX 576                     /* centre line of the case */
#define CASE_X  (CX - 32)
#define CASE_Y  30
#define CASE_W  64
#define CASE_H  164

#define DOOR_W      20
#define DOOR_H      34
#define DOOR_Y      44
#define DOOR_TRAVEL 16

#define BIRD_HOME_Y (DOOR_Y + 8)
#define BIRD_OUT_Y  (DOOR_Y + 26)

#define PIVOT_Y   100
#define ROD_LEN   62
#define BOB_R     10
#define SWING_DEG 12.0f
#define SWING_MS  2000.0f

#define DEG2RAD 0.017453292519943295f

#define T_DOORS_OPEN   420
#define T_BIRD_OUT     900
#define T_HOLD_END    2900
#define T_BIRD_IN     3320
#define T_DOORS_SHUT  3740

static const ui_hud_theme_t THEME = {
    .ink = CREAM, .dim = 0xB39A7C,
    .panel = 0x2E1D12, .panel_opa = LV_OPA_90,
    .border_w = 1, .border_opa = LV_OPA_70, .radius = 6,
    .weather = 0x9CC5A1, .news = TRIM, .hn = BIRD, .markets = 0xE8D5A8,
    .up = 0x9CC5A1, .down = 0xE06C5C,
    .tag_prefix = "", .tag_family = UI_FONT_MEDIUM,
    .clock_family = UI_FONT_THIN, .clock = CREAM, .clock_align = LV_TEXT_ALIGN_LEFT,
    .date_family = UI_FONT_MEDIUM, .date = TRIM,
    .secs_style = HUD_SECS_BAR, .secs = TRIM, .secs_track = WOOD_DARK,
    .secs_track_opa = LV_OPA_COVER,
};

static lv_obj_t *s_door_l, *s_door_r, *s_bird, *s_call;
static lv_obj_t *s_rod, *s_bob;
static lv_point_precise_t s_rod_pts[2];

static uint32_t s_call_start;
static bool s_calling;
static int s_last_minute = -1;

static void add_case(lv_obj_t *root)
{
    /* Stepped eaves read as a pitched roof without a rotated shape. */
    for (int i = 0; i < 4; i++) {
        int w = 40 + i * 12;
        ui_add_rect(root, CX - w / 2, 8 + i * 6, w, 7, i % 2 ? WOOD_DARK : WOOD,
                    LV_OPA_COVER, 2);
    }
    lv_obj_t *c = ui_add_rect(root, CASE_X, CASE_Y, CASE_W, CASE_H, WOOD, LV_OPA_COVER, 4);
    lv_obj_set_style_border_width(c, 2, 0);
    lv_obj_set_style_border_color(c, lv_color_hex(TRIM), 0);

    /* Dark recess so an open door shows somewhere to be. */
    ui_add_rect(root, CX - DOOR_W, DOOR_Y, DOOR_W * 2, DOOR_H, 0x140C07, LV_OPA_COVER, 3);
}

static void add_bird(lv_obj_t *root)
{
    s_bird = lv_obj_create(root);
    lv_obj_remove_style_all(s_bird);
    lv_obj_set_size(s_bird, 28, 20);
    lv_obj_set_pos(s_bird, CX - 14, BIRD_HOME_Y);
    lv_obj_remove_flag(s_bird, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(s_bird, LV_OBJ_FLAG_CLICKABLE);
    ui_add_rect(s_bird, 2, 5, 18, 13, BIRD, LV_OPA_COVER, 6);        /* body */
    ui_add_rect(s_bird, 13, 1, 10, 10, BIRD, LV_OPA_COVER, 5);       /* head */
    ui_add_rect(s_bird, 22, 5, 6, 4, 0xE8B23A, LV_OPA_COVER, 2);     /* beak */
    ui_add_rect(s_bird, 17, 4, 3, 3, 0x241309, LV_OPA_COVER, 1);     /* eye */

    /* Doors after the bird so they hide it when shut. */
    s_door_l = ui_add_rect(root, CX - DOOR_W, DOOR_Y, DOOR_W, DOOR_H, WOOD, LV_OPA_COVER, 3);
    lv_obj_set_style_border_width(s_door_l, 2, 0);
    lv_obj_set_style_border_color(s_door_l, lv_color_hex(TRIM), 0);
    s_door_r = ui_add_rect(root, CX, DOOR_Y, DOOR_W, DOOR_H, WOOD, LV_OPA_COVER, 3);
    lv_obj_set_style_border_width(s_door_r, 2, 0);
    lv_obj_set_style_border_color(s_door_r, lv_color_hex(TRIM), 0);

    /* Right-aligned under the case, clear of the left-aligned date. */
    s_call = lv_label_create(root);
    lv_obj_set_style_text_font(s_call, ui_font(UI_FONT_MEDIUM, UI_TEXT_MD), 0);
    lv_obj_set_style_text_color(s_call, lv_color_hex(CREAM), 0);
    lv_obj_set_width(s_call, 160);
    lv_obj_set_style_text_align(s_call, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s_call, HUD_CLOCK_X + HUD_CLOCK_W - 160, CASE_Y + CASE_H + 6);
    lv_label_set_text(s_call, "Cuckoo!");
    lv_obj_add_flag(s_call, LV_OBJ_FLAG_HIDDEN);
}

static void add_pendulum(lv_obj_t *root)
{
    ui_add_rect(root, CX - 5, PIVOT_Y - 5, 10, 10, TRIM, LV_OPA_COVER, 5);
    s_rod = lv_line_create(root);
    lv_obj_remove_style_all(s_rod);
    lv_obj_set_style_line_width(s_rod, 3, 0);
    lv_obj_set_style_line_color(s_rod, lv_color_hex(TRIM), 0);
    lv_obj_set_style_line_rounded(s_rod, true, 0);
    s_bob = ui_add_rect(root, 0, 0, BOB_R * 2, BOB_R * 2, TRIM, LV_OPA_COVER, BOB_R);
    lv_obj_set_style_border_width(s_bob, 2, 0);
    lv_obj_set_style_border_color(s_bob, lv_color_hex(WOOD_DARK), 0);
}

static lv_obj_t *cuckoo_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    ui_make_layer(root);
    ui_set_gradient(root, lv_color_hex(0x2A1B12), lv_color_hex(0x140C07));

    add_case(root);
    add_bird(root);
    add_pendulum(root);

    ui_hud_create(root, &THEME);

    s_call_start = lv_tick_get();
    s_calling = true;
    s_last_minute = -1;
    return root;
}

static int lerp(int from, int to, int elapsed, int span)
{
    if (elapsed <= 0) return from;
    if (elapsed >= span) return to;
    return from + ((to - from) * elapsed) / span;
}

static void set_doors(int offset)
{
    lv_obj_set_x(s_door_l, CX - DOOR_W - offset);
    lv_obj_set_x(s_door_r, CX + offset);
}

static void update_call(void)
{
    if (!s_calling) {
        return;
    }
    uint32_t t = lv_tick_elaps(s_call_start);

    if (t < T_DOORS_OPEN) {
        set_doors(lerp(0, DOOR_TRAVEL, t, T_DOORS_OPEN));
        lv_obj_set_y(s_bird, BIRD_HOME_Y);
    } else if (t < T_BIRD_OUT) {
        set_doors(DOOR_TRAVEL);
        lv_obj_set_y(s_bird, lerp(BIRD_HOME_Y, BIRD_OUT_Y, t - T_DOORS_OPEN,
                                  T_BIRD_OUT - T_DOORS_OPEN));
    } else if (t < T_HOLD_END) {
        set_doors(DOOR_TRAVEL);
        /* Bob forward on each call rather than sitting still. */
        lv_obj_set_y(s_bird, BIRD_OUT_Y + (((t - T_BIRD_OUT) / 500) % 2 ? 4 : 0));
        lv_obj_remove_flag(s_call, LV_OBJ_FLAG_HIDDEN);
    } else if (t < T_BIRD_IN) {
        lv_obj_add_flag(s_call, LV_OBJ_FLAG_HIDDEN);
        set_doors(DOOR_TRAVEL);
        lv_obj_set_y(s_bird, lerp(BIRD_OUT_Y, BIRD_HOME_Y, t - T_HOLD_END,
                                  T_BIRD_IN - T_HOLD_END));
    } else if (t < T_DOORS_SHUT) {
        lv_obj_set_y(s_bird, BIRD_HOME_Y);
        set_doors(lerp(DOOR_TRAVEL, 0, t - T_BIRD_IN, T_DOORS_SHUT - T_BIRD_IN));
    } else {
        set_doors(0);
        lv_obj_set_y(s_bird, BIRD_HOME_Y);
        lv_obj_add_flag(s_call, LV_OBJ_FLAG_HIDDEN);
        s_calling = false;
    }
}

static void update_pendulum(const clock_time_t *now)
{
    float phase = ((now->tm.tm_sec % 2) * 1000.0f + now->millis) / SWING_MS;
    float angle = SWING_DEG * sinf(phase * 2.0f * (float)M_PI);
    float rad = (angle + 90.0f) * DEG2RAD;

    int x = (int)(CX + cosf(rad) * ROD_LEN);
    int y = (int)(PIVOT_Y + sinf(rad) * ROD_LEN);

    /* Tight bounding box: a full-screen line object would repaint the whole
     * panel every frame. */
    int min_x = LV_MIN(CX, x) - 3, min_y = PIVOT_Y - 3;
    int max_x = LV_MAX(CX, x) + 3, max_y = y + 3;

    s_rod_pts[0].x = CX - min_x;  s_rod_pts[0].y = PIVOT_Y - min_y;
    s_rod_pts[1].x = x - min_x;   s_rod_pts[1].y = y - min_y;

    lv_obj_set_pos(s_rod, min_x, min_y);
    lv_obj_set_size(s_rod, max_x - min_x, max_y - min_y);
    lv_line_set_points(s_rod, s_rod_pts, 2);

    lv_obj_set_pos(s_bob, x - BOB_R, y - BOB_R);
}

static void cuckoo_tick(const clock_time_t *now)
{
    update_pendulum(now);
    update_call();

    /* Change the test below to make it hourly only. */
    if (s_last_minute != -1 && now->tm.tm_min != s_last_minute &&
        (now->tm.tm_min == 0 || now->tm.tm_min == 30)) {
        s_call_start = lv_tick_get();
        s_calling = true;
    }
    s_last_minute = now->tm.tm_min;

    ui_hud_tick(now);
}

const clock_face_t face_cuckoo = {
    .name = "Cuckoo",
    .light_chrome = true,
    .create = cuckoo_create,
    .tick = cuckoo_tick,
    .tap = ui_hud_tap,
};
