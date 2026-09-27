#include "ui/ui_widgets.h"

#include "ui/ui_fonts.h"
#include "ui/ui_theme.h"

/* ------------------------------------------------------------------ page -- */

lv_obj_t *w_page(void)
{
    lv_obj_t *page = lv_obj_create(NULL);
    lv_obj_remove_style_all(page);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(page, lv_color_hex(W_BG), 0);
    lv_obj_remove_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    return page;
}

lv_obj_t *w_header(lv_obj_t *page, const char *title, lv_event_cb_t back_cb, void *user_data)
{
    lv_obj_t *header = lv_obj_create(page);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, UI_SCREEN_W, W_HEADER_H);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    /* Generous tap target around a small chevron. */
    lv_obj_t *back = lv_button_create(header);
    lv_obj_remove_style_all(back);
    lv_obj_set_size(back, 92, W_HEADER_H);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_10, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(back, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);
    lv_obj_set_style_radius(back, 12, 0);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, user_data);

    lv_obj_t *chevron = lv_label_create(back);
    lv_label_set_text(chevron, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(chevron, lv_color_hex(W_ACCENT), 0);
    lv_obj_set_style_text_font(chevron, ui_symbol_font(UI_TEXT_LG), 0);
    lv_obj_center(chevron);

    lv_obj_t *label = lv_label_create(header);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_color(label, lv_color_hex(W_TEXT), 0);
    lv_obj_set_style_text_font(label, ui_font(UI_FONT_MEDIUM, UI_TEXT_LG), 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 92, 0);

    /* Hairline under the header keeps it distinct while the body scrolls. */
    lv_obj_t *rule = lv_obj_create(header);
    lv_obj_remove_style_all(rule);
    lv_obj_set_size(rule, UI_SCREEN_W, 1);
    lv_obj_align(rule, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_20, 0);
    lv_obj_set_style_bg_color(rule, lv_color_hex(0xFFFFFF), 0);

    return header;
}

lv_obj_t *w_body(lv_obj_t *page)
{
    lv_obj_t *body = lv_obj_create(page);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, UI_SCREEN_W, UI_SCREEN_H - W_HEADER_H);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, W_HEADER_H);
    lv_obj_set_style_pad_hor(body, (UI_SCREEN_W - W_BODY_W) / 2, 0);
    lv_obj_set_style_pad_top(body, 12, 0);
    lv_obj_set_style_pad_bottom(body, 40, 0);
    lv_obj_set_style_pad_row(body, 10, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_OFF);
    return body;
}

void w_section(lv_obj_t *body, const char *title)
{
    lv_obj_t *label = lv_label_create(body);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_color(label, lv_color_hex(W_DIM), 0);
    lv_obj_set_style_text_font(label, ui_font(UI_FONT_MEDIUM, UI_TEXT_SM), 0);
    lv_obj_set_style_text_letter_space(label, 2, 0);
    lv_obj_set_style_pad_left(label, 6, 0);
    lv_obj_set_style_pad_top(label, 18, 0);
}

/* ------------------------------------------------------------------- rows -- */

static lv_obj_t *base_row(lv_obj_t *body, int height, bool interactive)
{
    lv_obj_t *row = interactive ? lv_button_create(body) : lv_obj_create(body);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), height);
    lv_obj_set_style_radius(row, 16, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(W_CARD), 0);
    lv_obj_set_style_pad_hor(row, 22, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    if (interactive) {
        lv_obj_set_style_bg_color(row, lv_color_hex(W_CARD_PRESS), LV_STATE_PRESSED);
    }
    return row;
}

static lv_obj_t *row_title(lv_obj_t *row, const char *text)
{
    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(W_TEXT), 0);
    lv_obj_set_style_text_font(label, ui_font(UI_FONT_MEDIUM, UI_TEXT_MD), 0);
    return label;
}

lv_obj_t *w_nav_row(lv_obj_t *body, const char *label, const char *value,
                    lv_event_cb_t cb, void *user_data, lv_obj_t **out_value_label)
{
    lv_obj_t *row = base_row(body, W_ROW_H, true);
    lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, user_data);

    lv_obj_t *title = row_title(row, label);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *chevron = lv_label_create(row);
    lv_label_set_text(chevron, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(chevron, lv_color_hex(W_DIM), 0);
    lv_obj_set_style_text_font(chevron, ui_symbol_font(UI_TEXT_MD), 0);
    lv_obj_align(chevron, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_t *val = lv_label_create(row);
    lv_label_set_text(val, value ? value : "");
    lv_obj_set_style_text_color(val, lv_color_hex(W_DIM), 0);
    lv_obj_set_style_text_font(val, ui_font(UI_FONT_MEDIUM, UI_TEXT_MD), 0);
    lv_obj_align(val, LV_ALIGN_RIGHT_MID, -30, 0);

    if (out_value_label) {
        *out_value_label = val;
    }
    return row;
}

lv_obj_t *w_choice_row(lv_obj_t *body, const char *label, bool selected,
                       lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *row = base_row(body, W_ROW_H, true);
    lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, user_data);

    lv_obj_t *title = row_title(row, label);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);
    if (selected) {
        lv_obj_set_style_text_color(title, lv_color_hex(W_ACCENT), 0);
    }

    if (selected) {
        lv_obj_t *tick = lv_label_create(row);
        lv_label_set_text(tick, LV_SYMBOL_OK);
        lv_obj_set_style_text_color(tick, lv_color_hex(W_ACCENT), 0);
        lv_obj_set_style_text_font(tick, ui_symbol_font(UI_TEXT_MD), 0);
        lv_obj_align(tick, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    return row;
}

lv_obj_t *w_switch_row(lv_obj_t *body, const char *label, bool on,
                       lv_event_cb_t cb, void *user_data, lv_obj_t **out_switch)
{
    lv_obj_t *row = base_row(body, W_ROW_H, false);

    lv_obj_t *title = row_title(row, label);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *sw = lv_switch_create(row);
    lv_obj_set_size(sw, 66, 36);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x39404E), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, lv_color_hex(W_ACCENT), LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (on) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, user_data);

    if (out_switch) {
        *out_switch = sw;
    }
    return row;
}

lv_obj_t *w_slider_row(lv_obj_t *body, const char *label, int min, int max, int value,
                       lv_obj_t **out_slider, lv_obj_t **out_value_label)
{
    lv_obj_t *row = base_row(body, W_ROW_TALL_H, false);

    lv_obj_t *title = row_title(row, label);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 18);

    lv_obj_t *val = lv_label_create(row);
    lv_obj_set_style_text_color(val, lv_color_hex(W_DIM), 0);
    lv_obj_set_style_text_font(val, ui_font(UI_FONT_MEDIUM, UI_TEXT_MD), 0);
    lv_label_set_text_fmt(val, "%d%%", value);
    lv_obj_align(val, LV_ALIGN_TOP_RIGHT, 0, 20);

    lv_obj_t *slider = lv_slider_create(row);
    lv_obj_set_size(slider, W_BODY_W - 90, 8);
    lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, -22);
    lv_slider_set_range(slider, min, max);
    lv_slider_set_value(slider, value, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x39404E), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(W_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 10, LV_PART_KNOB);

    if (out_slider) {
        *out_slider = slider;
    }
    if (out_value_label) {
        *out_value_label = val;
    }
    return row;
}

lv_obj_t *w_info_row(lv_obj_t *body, const char *label, const char *value,
                     lv_obj_t **out_value_label)
{
    lv_obj_t *row = base_row(body, W_ROW_H, false);

    lv_obj_t *title = row_title(row, label);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *val = lv_label_create(row);
    lv_label_set_text(val, value ? value : "");
    lv_obj_set_style_text_color(val, lv_color_hex(W_DIM), 0);
    lv_obj_set_style_text_font(val, ui_font(UI_FONT_MEDIUM, UI_TEXT_MD), 0);
    lv_obj_align(val, LV_ALIGN_RIGHT_MID, 0, 0);

    if (out_value_label) {
        *out_value_label = val;
    }
    return row;
}

lv_obj_t *w_action_row(lv_obj_t *body, const char *label, uint32_t colour,
                       lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *row = base_row(body, W_ROW_H, true);
    lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, user_data);

    lv_obj_t *title = lv_label_create(row);
    lv_label_set_text(title, label);
    lv_obj_set_style_text_color(title, lv_color_hex(colour), 0);
    lv_obj_set_style_text_font(title, ui_font(UI_FONT_MEDIUM, UI_TEXT_MD), 0);
    lv_obj_center(title);

    return row;
}

lv_obj_t *w_stepper_row(lv_obj_t *body, const char *label, const char *value,
                        lv_event_cb_t minus_cb, lv_event_cb_t plus_cb, void *user_data,
                        lv_obj_t **out_value_label)
{
    lv_obj_t *row = base_row(body, W_ROW_H, false);

    lv_obj_t *title = row_title(row, label);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *plus = lv_button_create(row);
    lv_obj_remove_style_all(plus);
    lv_obj_set_size(plus, 54, 46);
    lv_obj_align(plus, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_radius(plus, 12, 0);
    lv_obj_set_style_bg_opa(plus, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(plus, lv_color_hex(0x2A323F), 0);
    lv_obj_set_style_bg_color(plus, lv_color_hex(W_ACCENT), LV_STATE_PRESSED);
    lv_obj_add_event_cb(plus, plus_cb, LV_EVENT_CLICKED, user_data);
    lv_obj_t *plus_lbl = lv_label_create(plus);
    lv_label_set_text(plus_lbl, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_font(plus_lbl, ui_symbol_font(UI_TEXT_MD), 0);
    lv_obj_set_style_text_color(plus_lbl, lv_color_hex(W_TEXT), 0);
    lv_obj_center(plus_lbl);

    lv_obj_t *val = lv_label_create(row);
    lv_label_set_text(val, value);
    lv_obj_set_style_text_color(val, lv_color_hex(W_TEXT), 0);
    lv_obj_set_style_text_font(val, ui_font(UI_FONT_MEDIUM, UI_TEXT_MD), 0);
    lv_obj_align(val, LV_ALIGN_RIGHT_MID, -70, 0);

    lv_obj_t *minus = lv_button_create(row);
    lv_obj_remove_style_all(minus);
    lv_obj_set_size(minus, 54, 46);
    lv_obj_align(minus, LV_ALIGN_RIGHT_MID, -150, 0);
    lv_obj_set_style_radius(minus, 12, 0);
    lv_obj_set_style_bg_opa(minus, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(minus, lv_color_hex(0x2A323F), 0);
    lv_obj_set_style_bg_color(minus, lv_color_hex(W_ACCENT), LV_STATE_PRESSED);
    lv_obj_add_event_cb(minus, minus_cb, LV_EVENT_CLICKED, user_data);
    lv_obj_t *minus_lbl = lv_label_create(minus);
    lv_label_set_text(minus_lbl, LV_SYMBOL_MINUS);
    lv_obj_set_style_text_font(minus_lbl, ui_symbol_font(UI_TEXT_MD), 0);
    lv_obj_set_style_text_color(minus_lbl, lv_color_hex(W_TEXT), 0);
    lv_obj_center(minus_lbl);

    if (out_value_label) {
        *out_value_label = val;
    }
    return row;
}

lv_obj_t *w_placeholder(lv_obj_t *body, const char *text)
{
    lv_obj_t *label = lv_label_create(body);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(W_DIM), 0);
    lv_obj_set_style_text_font(label, ui_font(UI_FONT_MEDIUM, UI_TEXT_MD), 0);
    lv_obj_set_style_pad_all(label, 24, 0);
    return label;
}
