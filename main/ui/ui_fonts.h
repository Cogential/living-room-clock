#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_FONT_THIN = 0,   /* Ubuntu Thin  - the big elegant numerals */
    UI_FONT_MEDIUM,     /* Ubuntu Medium - labels, dock, status bar */
    UI_FONT_MONO,       /* Ubuntu Mono Bold - the neon face */
    UI_FONT_FAMILY_COUNT,
} ui_font_family_t;

/*
 * Interface type scale.
 *
 * Every font instance carries its own glyph and bitmap caches, so each distinct
 * size costs real memory. Chrome and settings stick to these three sizes; only
 * the clock faces, where the type *is* the design, ask for arbitrary sizes.
 */
#define UI_TEXT_SM 18
#define UI_TEXT_MD 22
#define UI_TEXT_LG 28

/*
 * Face type scale.
 *
 * Faces are where the type is the design, but every distinct pixel size is a
 * separate font instance with its own caches, and there are a dozen faces. Pick
 * from these rather than inventing sizes: the whole app then needs eight font
 * instances no matter how many faces get added.
 */
/*
 * The clock itself. Both families are the same nominal size so a mono face and
 * a thin face read as the same size on the wall - keep them equal.
 *
 * 210 is as large as the stacked layout allows: at this size "12:34" spans
 * x 250..774 and the tallest family (thin, 1.149x line height) fills
 * UI_BAND_TIME_Y..399, leaving the date and the news band clear by a few
 * pixels. Going bigger means moving the bands in ui_bands.h first.
 */
#define FACE_TIME_THIN 210   /* UI_FONT_THIN */
#define FACE_TIME_MONO 210   /* UI_FONT_MONO */

#define FACE_MONO_XL  190   /* UI_FONT_MONO  */
#define FACE_LABEL_L   34   /* UI_FONT_MEDIUM */

/**
 * Fetch a font at an arbitrary pixel size, rendered from the embedded TTFs.
 *
 * Instances are cached and shared, so asking for the same family/size twice is
 * free. Returns LVGL's default font if the cache is exhausted or the face
 * fails to load, so callers never have to null-check.
 */
const lv_font_t *ui_font(ui_font_family_t family, int size);

/**
 * Font for LV_SYMBOL_* glyphs.
 *
 * The symbols live at private-use codepoints (LV_SYMBOL_SETTINGS is 0xF013)
 * that only LVGL's own built-in fonts carry - the Ubuntu faces have nothing
 * there, so a symbol drawn in ui_font() renders as blank. Any label whose text
 * contains a symbol must use this instead. Snaps to the nearest built-in size.
 */
const lv_font_t *ui_symbol_font(int size);

#ifdef __cplusplus
}
#endif
