#pragma once

#include "lvgl.h"
#include "ui/ui.h"
#include "ui/ui_fonts.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The shared data bands: weather above the clock, headlines below it, a market
 * ticker along the bottom. Every face lays these out identically so the
 * information is always in the same place; only the colours change.
 *
 * Fixed geometry, and nothing is allowed to grow into its neighbour - headlines
 * are clipped rather than wrapped and the ticker scrolls within its strip. A
 * face owns only the middle region between them.
 */

/*
 * Vertical layout, in panel pixels on a 600 px screen.
 *
 * These are packed deliberately tight so the clock itself can be as large as
 * possible. Every value below is derived from a measured line height, not
 * guessed - lv_tiny_ttf sets line_height = size * (ascender - descender +
 * lineGap) / unitsPerEm, which for Ubuntu is 1.149x the nominal size and for
 * Ubuntu Mono is exactly 1.0x. Before moving anything here, redo that sum: the
 * bands sit within a few pixels of each other and nothing may overlap.
 *
 *    64  weather main   34 px -> ends  103
 *   106  weather meta   18 px -> ends  126
 *   130  weather fcst   18 px -> ends  150
 *   156  FACE_TOP
 *   158  time          210 px -> ends  399  (thin; mono ends 368)
 *   406  date           22 px -> ends  431
 *   438  FACE_BOTTOM
 *   446  news 1        50 px slot -> ends  496
 *   500  news 2        50 px slot -> ends  550
 *   558  ticker         22 px -> ends  583
 */
#define UI_BAND_WEATHER_MAIN  64
#define UI_BAND_WEATHER_META  106
#define UI_BAND_WEATHER_FCST  130
#define UI_BAND_NEWS_1        446
#define UI_BAND_NEWS_2        500
#define UI_BAND_TICKER        558

/* The slice a face may use for its clock and its own artwork. Backgrounds and
 * glows may span the whole panel; anything opaque must stay inside this. */
#define UI_BAND_FACE_TOP      156
#define UI_BAND_FACE_BOTTOM   438

/*
 * Where a face's own time and date go, so every face agrees.
 *
 * At FACE_TIME_* the numerals span x 250..774, so a face's own artwork needs to
 * live outside that column - roughly x < 240 or x > 784 - or sit behind the
 * clock as a backdrop. Anything small and meaningful placed centrally will be
 * covered by the digits.
 */
#define UI_BAND_TIME_Y        158
#define UI_BAND_DATE_Y        406

typedef struct {
    uint32_t ink;     /* headline text */
    uint32_t dim;     /* secondary detail and the ticker */
    uint32_t accent;  /* forecast row */
    /*
     * Per-glyph drop shadow behind the band text, for faces whose background is
     * a photograph rather than a flat colour they chose.
     *
     * Off by default, and deliberately opt-in: LVGL renders the text into an
     * offscreen layer and blurs it, so it is the most expensive thing a band can
     * ask for. Faces that control their own background do not need it.
     */
    bool shadow;
} ui_bands_palette_t;

/** Give a label the same per-glyph shadow the bands use. */
void ui_text_shadow(lv_obj_t *label, int radius);

typedef struct {
    lv_obj_t *weather_main;
    lv_obj_t *weather_meta;
    lv_obj_t *weather_fcst;
    lv_obj_t *news;              /* world headline, rotating */
    lv_obj_t *hn;                /* Hacker News story, rotating */
    lv_obj_t *ticker;
    uint32_t  seen_generation;
    /*
     * Both headline lines cycle through what was fetched rather than showing a
     * fixed item. The feeds return six of each but the layout has room for one
     * of each at a time, so without this five in six were never seen.
     */
    uint32_t  rotated;           /* lv_tick at the last rotation */
    uint8_t   rotate_slot;       /* alternates, so only one line moves at a time */
    int       news_index;
    int       hn_index;
} ui_bands_t;

/*
 * The clock itself, positioned to agree with the bands.
 *
 * Every face wants the same thing here - centred time at UI_BAND_TIME_Y, a
 * date under it - differing only in typeface and colour, so it lives here
 * rather than eleven times over.
 */
lv_obj_t *ui_face_time_label(lv_obj_t *root, ui_font_family_t family, int size,
                             uint32_t colour);

/** Date line under the clock. */
lv_obj_t *ui_face_date_label(lv_obj_t *root, uint32_t colour);

/**
 * Update both, only when the value shown actually changed.
 * `date_label` may be NULL for faces that do not show one.
 */
void ui_face_update_time(lv_obj_t *time_label, lv_obj_t *date_label,
                         const clock_time_t *now, int *last_minute, int *last_yday);

/** Build the bands into `root`. Call once from a face's create(). */
void ui_bands_create(ui_bands_t *bands, lv_obj_t *root, const ui_bands_palette_t *palette);

/**
 * Refresh if the API layer has new data.
 *
 * Cheap to call every frame: it compares a generation counter and returns
 * immediately when nothing has changed.
 */
void ui_bands_tick(ui_bands_t *bands);

#ifdef __cplusplus
}
#endif
