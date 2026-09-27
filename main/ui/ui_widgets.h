#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Building blocks for the settings sheets.
 *
 * The clock faces each own their palette, but settings is one consistent dark
 * surface regardless of which face is behind it, so these carry fixed colours.
 */

#define W_BG        0x0E1116
#define W_CARD      0x171B22
#define W_CARD_PRESS 0x212936
#define W_TEXT      0xF2F5FA
#define W_DIM       0x8B95A7
#define W_ACCENT    0x4DA3FF
#define W_DANGER    0xFF6B5A

#define W_ROW_H      74
#define W_ROW_TALL_H 104
#define W_HEADER_H   78
#define W_BODY_W     780

/** A full-screen settings page (an LVGL screen, not yet loaded). */
lv_obj_t *w_page(void);

/** Header bar with a back chevron; `back_cb` fires on tap. Returns the header. */
lv_obj_t *w_header(lv_obj_t *page, const char *title, lv_event_cb_t back_cb, void *user_data);

/** Scrollable content column below the header. */
lv_obj_t *w_body(lv_obj_t *page);

/** Small uppercase group heading. */
void w_section(lv_obj_t *body, const char *title);

/** Row that navigates somewhere; shows `value` dimmed on the right. */
lv_obj_t *w_nav_row(lv_obj_t *body, const char *label, const char *value,
                    lv_event_cb_t cb, void *user_data, lv_obj_t **out_value_label);

/** Selection row: a tick when `selected`, and no chevron. */
lv_obj_t *w_choice_row(lv_obj_t *body, const char *label, bool selected,
                       lv_event_cb_t cb, void *user_data);

/** Row with a toggle. The switch object is returned via `out_switch`. */
lv_obj_t *w_switch_row(lv_obj_t *body, const char *label, bool on,
                       lv_event_cb_t cb, void *user_data, lv_obj_t **out_switch);

/**
 * Row with a slider and a live value readout.
 *
 * No callback is attached here: sliders need both a VALUE_CHANGED handler (to
 * apply and update the readout) and a RELEASED one (to persist once rather than
 * on every drag step), so wiring is left to the caller.
 */
lv_obj_t *w_slider_row(lv_obj_t *body, const char *label, int min, int max, int value,
                       lv_obj_t **out_slider, lv_obj_t **out_value_label);

/** Read-only label/value row. */
lv_obj_t *w_info_row(lv_obj_t *body, const char *label, const char *value,
                     lv_obj_t **out_value_label);

/** Row that acts as a button; the label is drawn in the accent colour. */
lv_obj_t *w_action_row(lv_obj_t *body, const char *label, uint32_t colour,
                       lv_event_cb_t cb, void *user_data);

/** Row with two -/+ steppers around a value, for hour pickers. */
lv_obj_t *w_stepper_row(lv_obj_t *body, const char *label, const char *value,
                        lv_event_cb_t minus_cb, lv_event_cb_t plus_cb, void *user_data,
                        lv_obj_t **out_value_label);

/** Centred dim message, e.g. "Scanning..." or an empty-list placeholder. */
lv_obj_t *w_placeholder(lv_obj_t *body, const char *text);

#ifdef __cplusplus
}
#endif
