#pragma once

#include <stddef.h>
#include <time.h>

#include "lvgl.h"
#include "ui/ui.h"
#include "ui/ui_fonts.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The HUD every face is built on: clock top left, the live feeds in panels
 * around it, each section in its own accent colour so it can be picked out from
 * across the room.
 *
 *   clock + date + seconds   |  WEATHER
 *   NEWS                     |  HACKER NEWS
 *   five market cells, change coloured up / down
 *
 * A face supplies a theme and draws its own artwork: the background before
 * ui_hud_create(), anything that must sit on top of the panels after it. Free
 * space for artwork is the clock column around the digits, the gutters between
 * panels, and the whole background behind the translucent panels.
 *
 * Tapping a headline card shows its article as a QR code (see qr_sheet.h);
 * faces get that by setting .tap = ui_hud_tap.
 *
 * All state is module-static: only one face is alive at a time.
 */

/* Geometry, in panel pixels. Faces use these to place artwork. */
#define HUD_MARGIN     20
#define HUD_GAP        16

#define HUD_CLOCK_X    HUD_MARGIN
#define HUD_CLOCK_W    580
#define HUD_CLOCK_H    262          /* the column runs 0..HUD_CLOCK_H */
#define HUD_DATE_Y     192
#define HUD_SECS_X     40
#define HUD_SECS_Y     236
#define HUD_SECS_W     540

#define HUD_WX_X       (HUD_CLOCK_X + HUD_CLOCK_W + HUD_GAP)
#define HUD_WX_Y       16
#define HUD_WX_W       (UI_SCREEN_W - HUD_MARGIN - HUD_WX_X)
#define HUD_WX_H       246

#define HUD_CARD_Y     (HUD_WX_Y + HUD_WX_H + HUD_GAP)
#define HUD_CARD_W     ((UI_SCREEN_W - 2 * HUD_MARGIN - HUD_GAP) / 2)
#define HUD_CARD_H     162

#define HUD_MKT_Y      (HUD_CARD_Y + HUD_CARD_H + HUD_GAP)
#define HUD_MKT_H      (UI_SCREEN_H - 16 - HUD_MKT_Y)
#define HUD_MKT_CELLS  5
#define HUD_MKT_GAP    12
#define HUD_MKT_W      ((UI_SCREEN_W - 2 * HUD_MARGIN - (HUD_MKT_CELLS - 1) * HUD_MKT_GAP) \
                        / HUD_MKT_CELLS)

typedef enum {
    HUD_SECS_BAR = 0,  /* one bar filling left to right */
    HUD_SECS_DUAL,     /* two bars closing on the middle, secs / secs2 colours */
    HUD_SECS_NONE,     /* the face draws its own */
} hud_secs_style_t;

typedef struct {
    /* Text. */
    uint32_t ink;              /* titles and values */
    uint32_t dim;              /* keys, sources, secondary detail */

    /* Panels. A hard offset shadow (shadow_dx/dy non-zero) is a flat rect, not a
     * blur - see the performance notes in the README. */
    uint32_t panel;
    lv_opa_t panel_opa;
    int      border_w;         /* in the section's accent colour */
    lv_opa_t border_opa;
    int      radius;
    uint32_t shadow;
    int      shadow_dx, shadow_dy;

    /* One accent per section, used for its notch, tag and border. */
    uint32_t weather, news, hn, markets;
    uint32_t up, down;         /* market change, and fear / greed */

    /* Section headings: "// " for a terminal feel, "" for a plain one. */
    const char *tag_prefix;
    ui_font_family_t tag_family;

    /* Clock. */
    ui_font_family_t clock_family;   /* UI_FONT_THIN or UI_FONT_MONO */
    uint32_t clock;
    lv_text_align_t clock_align;     /* CENTER, or LEFT to free the right edge */
    /* Offset duplicate behind the numerals: a chromatic split or a hard shadow.
     * ghost_opa 0 for none. */
    uint32_t ghost;
    lv_opa_t ghost_opa;
    int      ghost_dx, ghost_dy;
    bool     text_shadow;            /* per-glyph shadow, for a photo background */

    ui_font_family_t date_family;
    uint32_t date;
    /* Optional: write the date line (upper case reads best). NULL for the
     * default "SAT 27 SEP 2026". AM/PM is appended either way in 12 h mode. */
    void (*date_fmt)(const struct tm *tm, char *out, size_t len);

    hud_secs_style_t secs_style;
    uint32_t secs, secs2;
    uint32_t secs_track;
    lv_opa_t secs_track_opa;
} ui_hud_theme_t;

/** Build the clock column and all panels into `root`. */
void ui_hud_create(lv_obj_t *root, const ui_hud_theme_t *theme);

/** Time, seconds, feed refresh and headline rotation. Call from tick(). */
void ui_hud_tick(const clock_time_t *now);

/** Headline cards open their article as a QR code. Use as the face's .tap. */
void ui_hud_tap(int x, int y);

/** The time label, for faces that animate around the digits. */
lv_obj_t *ui_hud_time_label(void);

/** Give a label a per-glyph drop shadow, for text over a photograph. */
void ui_text_shadow(lv_obj_t *label, int radius);

#ifdef __cplusplus
}
#endif
