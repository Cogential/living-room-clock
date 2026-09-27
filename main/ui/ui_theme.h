#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The panel is 1024x600; faces lay out against these rather than magic numbers. */
#define UI_SCREEN_W 1024
#define UI_SCREEN_H 600

/* Chrome geometry, shared so faces can keep clear of the status bar and dock. */
#define UI_STATUS_H  56
#define UI_DOCK_H    76
#define UI_DOCK_W    600
#define UI_DOCK_GAP  22

/**
 * Make `obj` a plain full-bleed layer: no padding, no scrolling, no border.
 * Every face root starts from this so they compose predictably.
 */
void ui_make_layer(lv_obj_t *obj);

/**
 * Fill `obj` with a soft vertical two-stop gradient.
 *
 * LVGL's multi-stop gradients moved around between 9.x releases, so faces
 * build depth from a plain gradient plus translucent overlay shapes instead.
 */
void ui_set_gradient(lv_obj_t *obj, lv_color_t top, lv_color_t bottom);

/**
 * Add a large, very soft translucent circle - the cheap way to fake the
 * coloured bloom that makes a flat background look lit rather than painted.
 */
lv_obj_t *ui_add_glow(lv_obj_t *parent, lv_color_t color, int diameter,
                      int x, int y, lv_opa_t opa);

/** Horizontal CRT scanlines across the whole face. Keep spacing >= 6. */
void ui_add_scanlines(lv_obj_t *parent, uint32_t color, int spacing, lv_opa_t opa);

/** A flat rectangle: the building block for panels, bars and pixel blocks. */
lv_obj_t *ui_add_rect(lv_obj_t *parent, int x, int y, int w, int h,
                      uint32_t color, lv_opa_t opa, int radius);

/** Four L-shaped corner brackets inset from the screen edge. */
void ui_add_corner_brackets(lv_obj_t *parent, int inset, int len, int thick,
                            uint32_t color, lv_opa_t opa);

/** A horizontal rule with a small diamond at its centre. */
void ui_add_ornate_rule(lv_obj_t *parent, int y, int width, uint32_t color, lv_opa_t opa);

#ifdef __cplusplus
}
#endif
