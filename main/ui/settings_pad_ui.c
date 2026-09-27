#include <stdio.h>
#include <string.h>

#include "input/app_pad.h"
#include "ui/settings_ui.h"
#include "ui/ui_fonts.h"
#include "ui/ui_theme.h"
#include "ui/ui_widgets.h"

/* Fast enough to feel live under your thumb without redrawing constantly. */
#define PAD_POLL_MS 100

#define STICK_BOX 118

static lv_obj_t *s_status;
static lv_obj_t *s_left_dot;
static lv_obj_t *s_right_dot;
static lv_obj_t *s_left_val;
static lv_obj_t *s_right_val;
static lv_obj_t *s_l2_bar;
static lv_obj_t *s_r2_bar;
static lv_obj_t *s_buttons;
static lv_obj_t *s_dpad;
static lv_obj_t *s_raw;

static const struct {
    uint32_t mask;
    const char *name;
} BUTTONS[] = {
    { PAD_BTN_CROSS,    "Cross" },
    { PAD_BTN_CIRCLE,   "Circle" },
    { PAD_BTN_SQUARE,   "Square" },
    { PAD_BTN_TRIANGLE, "Triangle" },
    { PAD_BTN_L1,       "L1" },
    { PAD_BTN_R1,       "R1" },
    { PAD_BTN_L2,       "L2" },
    { PAD_BTN_R2,       "R2" },
    { PAD_BTN_L3,       "L3" },
    { PAD_BTN_R3,       "R3" },
    { PAD_BTN_CREATE,   "Create" },
    { PAD_BTN_OPTIONS,  "Options" },
    { PAD_BTN_PS,       "PS" },
    { PAD_BTN_TOUCHPAD, "Touchpad" },
    { PAD_BTN_MUTE,     "Mute" },
};
#define BUTTON_COUNT ((int)(sizeof(BUTTONS) / sizeof(BUTTONS[0])))

static const char *DPAD_NAMES[8] = {
    "Up", "Up-Right", "Right", "Down-Right", "Down", "Down-Left", "Left", "Up-Left",
};

/* A square with a dot in it reads as a stick position instantly; numbers alone
 * make you do the mapping in your head. */
static lv_obj_t *build_stick(lv_obj_t *parent, int x, lv_obj_t **out_dot)
{
    lv_obj_t *box = ui_add_rect(parent, x, 14, STICK_BOX, STICK_BOX, 0x2A323F,
                                LV_OPA_COVER, 10);
    ui_add_rect(box, STICK_BOX / 2 - 1, 0, 2, STICK_BOX, 0x3C4655, LV_OPA_COVER, 0);
    ui_add_rect(box, 0, STICK_BOX / 2 - 1, STICK_BOX, 2, 0x3C4655, LV_OPA_COVER, 0);

    *out_dot = ui_add_rect(box, STICK_BOX / 2 - 9, STICK_BOX / 2 - 9, 18, 18,
                           W_ACCENT, LV_OPA_COVER, 9);
    return box;
}

static void set_stick(lv_obj_t *dot, float x, float y)
{
    int range = STICK_BOX / 2 - 12;
    lv_obj_set_pos(dot,
                   STICK_BOX / 2 - 9 + (int)(x * range),
                   STICK_BOX / 2 - 9 + (int)(y * range));
}

static void poll_cb(lv_timer_t *timer)
{
    (void)timer;

    pad_state_t state = app_pad_state();
    const char *name = app_pad_name();

    switch (state) {
    case PAD_STATE_READY:
        lv_label_set_text(s_status, name[0] ? name : "Controller");
        break;
    case PAD_STATE_UNSUPPORTED:
        lv_label_set_text_fmt(s_status, "%s (not decoded)", name[0] ? name : "HID device");
        break;
    default:
        lv_label_set_text(s_status, "Nothing connected");
        break;
    }

    pad_input_t in;
    if (app_pad_get(&in)) {
        set_stick(s_left_dot, in.lx, in.ly);
        set_stick(s_right_dot, in.rx, in.ry);
        lv_label_set_text_fmt(s_left_val, "%+.2f  %+.2f", in.lx, in.ly);
        lv_label_set_text_fmt(s_right_val, "%+.2f  %+.2f", in.rx, in.ry);
        lv_obj_set_width(s_l2_bar, (280 * in.l2) / 255);
        lv_obj_set_width(s_r2_bar, (280 * in.r2) / 255);

        char pressed[160] = "";
        for (int i = 0; i < BUTTON_COUNT; i++) {
            if (in.buttons & BUTTONS[i].mask) {
                if (pressed[0]) {
                    strlcat(pressed, "   ", sizeof(pressed));
                }
                strlcat(pressed, BUTTONS[i].name, sizeof(pressed));
            }
        }
        lv_label_set_text(s_buttons, pressed[0] ? pressed : "-");
        lv_label_set_text(s_dpad, in.dpad >= 0 ? DPAD_NAMES[in.dpad] : "-");
    } else {
        lv_label_set_text(s_buttons, "-");
        lv_label_set_text(s_dpad, "-");
    }

    uint8_t raw[16];
    int n = app_pad_raw(raw, sizeof(raw));
    if (n > 0) {
        char hex[64] = "";
        for (int i = 0; i < n; i++) {
            char byte[4];
            snprintf(byte, sizeof(byte), "%02X ", raw[i]);
            strlcat(hex, byte, sizeof(hex));
        }
        lv_label_set_text(s_raw, hex);
    } else {
        lv_label_set_text(s_raw, "-");
    }
}

static void page_deleted_cb(lv_event_t *e)
{
    lv_timer_t *timer = (lv_timer_t *)lv_event_get_user_data(e);
    if (timer) {
        lv_timer_delete(timer);
    }
    s_status = NULL;
    s_left_dot = s_right_dot = NULL;
    s_left_val = s_right_val = NULL;
    s_l2_bar = s_r2_bar = NULL;
    s_buttons = s_dpad = s_raw = NULL;
}

static void back_cb(lv_event_t *e)
{
    (void)e;
    settings_ui_open();
}

static lv_obj_t *labelled(lv_obj_t *parent, const char *text, int x, int y, uint32_t colour)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_color(l, lv_color_hex(colour), 0);
    lv_obj_set_style_text_font(l, ui_font(UI_FONT_MEDIUM, UI_TEXT_SM), 0);
    lv_obj_set_pos(l, x, y);
    return l;
}

void settings_ui_open_pad(void)
{
    lv_obj_t *page = w_page();
    w_header(page, "Controller", back_cb, NULL);
    lv_obj_t *body = w_body(page);

    w_section(body, "STATUS");
    w_info_row(body, "Device", "", &s_status);

    lv_obj_t *note = lv_label_create(body);
    lv_label_set_text(note,
                      "Connect by USB cable to the Type-A port. The DualSense uses\n"
                      "Bluetooth Classic, which this board's radio does not support,\n"
                      "so wireless pairing is not possible.");
    lv_obj_set_style_text_color(note, lv_color_hex(W_DIM), 0);
    lv_obj_set_style_text_font(note, ui_font(UI_FONT_MEDIUM, UI_TEXT_SM), 0);
    lv_obj_set_style_pad_all(note, 10, 0);

    /* ---- sticks ---- */
    w_section(body, "STICKS");
    lv_obj_t *sticks = lv_obj_create(body);
    lv_obj_remove_style_all(sticks);
    lv_obj_set_size(sticks, LV_PCT(100), STICK_BOX + 60);
    lv_obj_set_style_radius(sticks, 16, 0);
    lv_obj_set_style_bg_opa(sticks, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(sticks, lv_color_hex(W_CARD), 0);
    lv_obj_remove_flag(sticks, LV_OBJ_FLAG_SCROLLABLE);

    build_stick(sticks, 90, &s_left_dot);
    build_stick(sticks, 470, &s_right_dot);
    labelled(sticks, "Left", 90, STICK_BOX + 22, W_DIM);
    labelled(sticks, "Right", 470, STICK_BOX + 22, W_DIM);
    s_left_val = labelled(sticks, "+0.00  +0.00", 226, 60, W_TEXT);
    s_right_val = labelled(sticks, "+0.00  +0.00", 606, 60, W_TEXT);

    /* ---- triggers ---- */
    w_section(body, "TRIGGERS");
    lv_obj_t *trig = lv_obj_create(body);
    lv_obj_remove_style_all(trig);
    lv_obj_set_size(trig, LV_PCT(100), 96);
    lv_obj_set_style_radius(trig, 16, 0);
    lv_obj_set_style_bg_opa(trig, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(trig, lv_color_hex(W_CARD), 0);
    lv_obj_remove_flag(trig, LV_OBJ_FLAG_SCROLLABLE);

    labelled(trig, "L2", 24, 22, W_DIM);
    ui_add_rect(trig, 70, 22, 280, 12, 0x2A323F, LV_OPA_COVER, 6);
    s_l2_bar = ui_add_rect(trig, 70, 22, 0, 12, W_ACCENT, LV_OPA_COVER, 6);

    labelled(trig, "R2", 24, 58, W_DIM);
    ui_add_rect(trig, 70, 58, 280, 12, 0x2A323F, LV_OPA_COVER, 6);
    s_r2_bar = ui_add_rect(trig, 70, 58, 0, 12, W_ACCENT, LV_OPA_COVER, 6);

    /* ---- digital ---- */
    w_section(body, "BUTTONS");
    w_info_row(body, "D-pad", "-", &s_dpad);
    w_info_row(body, "Pressed", "-", &s_buttons);

    /* ---- raw ---- */
    w_section(body, "RAW REPORT");
    w_info_row(body, "First 16 bytes", "-", &s_raw);
    lv_obj_set_style_text_font(s_raw, ui_font(UI_FONT_MONO, UI_TEXT_SM), 0);

    lv_obj_t *raw_note = lv_label_create(body);
    lv_label_set_text(raw_note,
                      "Button and axis offsets come from documentation, not from\n"
                      "this device. If something reads wrong, these bytes show what\n"
                      "the controller actually sent.");
    lv_obj_set_style_text_color(raw_note, lv_color_hex(W_DIM), 0);
    lv_obj_set_style_text_font(raw_note, ui_font(UI_FONT_MEDIUM, UI_TEXT_SM), 0);
    lv_obj_set_style_pad_all(raw_note, 10, 0);

    lv_timer_t *timer = lv_timer_create(poll_cb, PAD_POLL_MS, NULL);
    lv_obj_add_event_cb(page, page_deleted_cb, LV_EVENT_DELETE, timer);
    poll_cb(NULL);

    settings_ui_push(page, true);
}
