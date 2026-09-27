#include "ui/ui_bands.h"

#include <stdio.h>
#include <string.h>

#include "net/app_api.h"
#include "ui/ui_fonts.h"
#include "ui/ui_theme.h"

#include <time.h>

#define SIDE_PAD   60
#define CONTENT_W  (UI_SCREEN_W - SIDE_PAD * 2)
#define NEWS_SLOT_H 50

/* Each headline line changes every other period, so a given line holds for
 * twice this - long enough to finish reading one. */
#define ROTATE_MS   9000

void ui_text_shadow(lv_obj_t *label, int radius)
{
    /*
     * A real per-glyph shadow, not a box shadow.
     *
     * lv_obj_set_style_shadow_width() on a label shadows the label's *rectangle*
     * - here a 904 px wide box - which over a photo reads as a dark slab, not as
     * contrast behind the text. drop_shadow_* renders the glyphs into a layer and
     * blurs that instead, which is the effect actually wanted.
     *
     * QUALITY_SPEED because this sits behind moving text on a 1024x600 panel and
     * the difference is not visible at these sizes.
     */
    lv_obj_set_style_drop_shadow_color(label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_drop_shadow_opa(label, LV_OPA_80, 0);
    lv_obj_set_style_drop_shadow_radius(label, radius, 0);
    lv_obj_set_style_drop_shadow_offset_x(label, 0, 0);
    lv_obj_set_style_drop_shadow_offset_y(label, 1, 0);
    lv_obj_set_style_drop_shadow_quality(label, LV_BLUR_QUALITY_SPEED, 0);
}

static lv_obj_t *band(lv_obj_t *root, int y, int size, uint32_t colour)
{
    lv_obj_t *l = lv_label_create(root);
    lv_obj_set_style_text_font(l, ui_font(UI_FONT_MEDIUM, size), 0);
    lv_obj_set_style_text_color(l, lv_color_hex(colour), 0);
    lv_obj_set_width(l, CONTENT_W);
    lv_obj_set_pos(l, SIDE_PAD, y);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(l, "");
    return l;
}

lv_obj_t *ui_face_time_label(lv_obj_t *root, ui_font_family_t family, int size,
                             uint32_t colour)
{
    lv_obj_t *l = lv_label_create(root);
    lv_obj_set_style_text_font(l, ui_font(family, size), 0);
    lv_obj_set_style_text_color(l, lv_color_hex(colour), 0);
    lv_obj_set_width(l, CONTENT_W);
    lv_obj_set_pos(l, SIDE_PAD, UI_BAND_TIME_Y);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(l, "--:--");
    return l;
}

lv_obj_t *ui_face_date_label(lv_obj_t *root, uint32_t colour)
{
    lv_obj_t *l = lv_label_create(root);
    lv_obj_set_style_text_font(l, ui_font(UI_FONT_MEDIUM, UI_TEXT_MD), 0);
    lv_obj_set_style_text_color(l, lv_color_hex(colour), 0);
    lv_obj_set_style_text_letter_space(l, 5, 0);
    lv_obj_set_width(l, CONTENT_W);
    lv_obj_set_pos(l, SIDE_PAD, UI_BAND_DATE_Y);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(l, "");
    return l;
}

void ui_face_update_time(lv_obj_t *time_label, lv_obj_t *date_label,
                         const clock_time_t *now, int *last_minute, int *last_yday)
{
    if (now->tm.tm_min != *last_minute) {
        *last_minute = now->tm.tm_min;
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d",
                 ui_display_hour(&now->tm), now->tm.tm_min);
        lv_label_set_text(time_label, buf);
    }

    if (date_label && now->tm.tm_yday != *last_yday) {
        *last_yday = now->tm.tm_yday;
        char buf[64];
        strftime(buf, sizeof(buf), "%A  %e %B", &now->tm);
        for (char *p = buf; *p; p++) {
            if (*p >= 'a' && *p <= 'z') *p -= 32;
        }
        lv_label_set_text(date_label, buf);
    }
}

void ui_bands_create(ui_bands_t *b, lv_obj_t *root, const ui_bands_palette_t *p)
{
    memset(b, 0, sizeof(*b));

    b->weather_main = band(root, UI_BAND_WEATHER_MAIN, FACE_LABEL_L, p->ink);
    b->weather_meta = band(root, UI_BAND_WEATHER_META, UI_TEXT_SM, p->dim);
    b->weather_fcst = band(root, UI_BAND_WEATHER_FCST, UI_TEXT_SM, p->accent);

    /* Clipped, not wrapped: a long headline must not push into the band below
     * it, and titles run to ~94 characters. */
    b->news = band(root, UI_BAND_NEWS_1, UI_TEXT_SM, p->ink);
    lv_label_set_long_mode(b->news, LV_LABEL_LONG_DOT);
    lv_obj_set_height(b->news, NEWS_SLOT_H);

    /* Hacker News gets the accent colour so the two headline lines are
     * distinguishable at a glance without a second font size. */
    b->hn = band(root, UI_BAND_NEWS_2, UI_TEXT_SM, p->accent);
    lv_label_set_long_mode(b->hn, LV_LABEL_LONG_DOT);
    lv_obj_set_height(b->hn, NEWS_SLOT_H);

    b->ticker = lv_label_create(root);
    lv_obj_set_style_text_font(b->ticker, ui_font(UI_FONT_MEDIUM, UI_TEXT_MD), 0);
    lv_obj_set_style_text_color(b->ticker, lv_color_hex(p->dim), 0);
    lv_obj_set_width(b->ticker, CONTENT_W);
    lv_obj_set_pos(b->ticker, SIDE_PAD, UI_BAND_TICKER);
    /* Circular scroll is a ticker for free, and only ever invalidates its own
     * strip - a few percent of the panel. */
    lv_label_set_long_mode(b->ticker, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(b->ticker, "");

    if (p->shadow) {
        /* Small radii. Blur cost scales with area, and the ticker repaints every
         * frame, so it gets the smallest one that still separates it from grass. */
        ui_text_shadow(b->weather_main, 6);
        ui_text_shadow(b->weather_meta, 4);
        ui_text_shadow(b->weather_fcst, 4);
        ui_text_shadow(b->news, 4);
        ui_text_shadow(b->hn, 4);
        ui_text_shadow(b->ticker, 3);
    }

    b->seen_generation = UINT32_MAX;
}

/* ---------------------------------------------------------------- refresh -- */

static void refresh_weather(ui_bands_t *b)
{
    api_weather_t w;
    if (!app_api_weather(&w)) {
        lv_label_set_text(b->weather_main,
                          app_api_configured() ? "Waiting for weather" : "");
        lv_label_set_text(b->weather_meta, "");
        lv_label_set_text(b->weather_fcst, "");
        return;
    }

    lv_label_set_text_fmt(b->weather_main, "%d°   %s", w.temp, w.desc);

    char meta[128];
    int n = snprintf(meta, sizeof(meta), "Feels %d°     Humidity %d%%     Wind %d",
                     w.feels, w.humidity, w.wind);
    if (w.aqi >= 0) {
        n += snprintf(meta + n, sizeof(meta) - n, "     AQI %d", w.aqi);
    }
    if (w.sunrise[0] && w.sunset[0]) {
        snprintf(meta + n, sizeof(meta) - n, "     %s - %s", w.sunrise, w.sunset);
    }
    lv_label_set_text(b->weather_meta, meta);

    char fcst[96] = "";
    for (int i = 0; i < 3 && w.forecast[i].day[0]; i++) {
        char part[32];
        snprintf(part, sizeof(part), "%s %d/%d     ",
                 w.forecast[i].day, w.forecast[i].hi, w.forecast[i].lo);
        strlcat(fcst, part, sizeof(fcst));
    }
    lv_label_set_text(b->weather_fcst, fcst);
}

static void refresh_news(ui_bands_t *b)
{
    api_news_t news;
    if (!app_api_news(&news) || news.count == 0) {
        lv_label_set_text(b->news, app_api_configured() ? "Waiting for headlines" : "");
        return;
    }

    int i = b->news_index % news.count;
    char line[200];
    snprintf(line, sizeof(line), "%s   ·   %s",
             news.items[i].title, news.items[i].source);
    lv_label_set_text(b->news, line);
}

static void refresh_hn(ui_bands_t *b)
{
    api_hn_t hn;
    if (!app_api_hn(&hn) || hn.count == 0) {
        lv_label_set_text(b->hn, app_api_configured() ? "Waiting for Hacker News" : "");
        return;
    }

    int i = b->hn_index % hn.count;
    char line[220];
    /* snprintf returns the length it *would* have written, so clamp before
     * using it as an offset or a long title walks off the end. */
    size_t n = (size_t)snprintf(line, sizeof(line), "HN   ·   %s", hn.items[i].title);
    if (n >= sizeof(line)) {
        n = sizeof(line) - 1;
    }

    /* Both are -1 on "job" posts, which carry neither. */
    if (hn.items[i].score >= 0) {
        size_t m = (size_t)snprintf(line + n, sizeof(line) - n,
                                    "   ·   %d points", hn.items[i].score);
        n = (m >= sizeof(line) - n) ? sizeof(line) - 1 : n + m;
    }
    if (hn.items[i].comments >= 0) {
        snprintf(line + n, sizeof(line) - n, "   ·   %d comments", hn.items[i].comments);
    }
    lv_label_set_text(b->hn, line);
}

static void refresh_ticker(ui_bands_t *b)
{
    api_markets_t m;
    if (!app_api_markets(&m)) {
        lv_label_set_text(b->ticker, app_api_configured() ? "Waiting for markets" : "");
        return;
    }

    /* One string: a single scrolling line cannot carry per-symbol colour, so
     * the sign does that job instead. */
    char line[320] = "";
    for (int i = 0; i < m.count; i++) {
        char part[64];
        snprintf(part, sizeof(part), "%s  %.2f  %+.2f%%     ",
                 m.quotes[i].symbol, m.quotes[i].price, m.quotes[i].change_pct);
        strlcat(line, part, sizeof(line));
    }
    if (m.btc_usd > 0.0f) {
        char part[64];
        snprintf(part, sizeof(part), "BTC  %.0f  %+.2f%%     ",
                 m.btc_usd, m.btc_change_pct);
        strlcat(line, part, sizeof(line));
    }
    if (m.fear_greed >= 0.0f) {
        char rating[16];
        strlcpy(rating, m.fear_greed_rating, sizeof(rating));
        for (char *p = rating; *p; p++) {
            if (*p >= 'a' && *p <= 'z') *p -= 32;
        }
        char part[64];
        snprintf(part, sizeof(part), "FEAR & GREED  %.0f  %s     ", m.fear_greed, rating);
        strlcat(line, part, sizeof(line));
    }

    lv_label_set_text(b->ticker, line);
}

void ui_bands_tick(ui_bands_t *b)
{
    uint32_t gen = app_api_generation();
    if (gen != b->seen_generation) {
        b->seen_generation = gen;
        refresh_weather(b);
        refresh_news(b);
        refresh_hn(b);
        refresh_ticker(b);
        b->rotated = lv_tick_get();
        return;
    }

    /*
     * Advance one headline line per period, alternating, so a line changes every
     * ROTATE_MS but the two never move together - two lines swapping at once
     * reads as a glitch rather than as a rotation.
     *
     * Only a single label is relabelled here, which invalidates its own strip
     * and nothing else.
     */
    if (lv_tick_elaps(b->rotated) < ROTATE_MS) {
        return;
    }
    b->rotated = lv_tick_get();

    if (b->rotate_slot++ & 1) {
        b->hn_index++;
        refresh_hn(b);
    } else {
        b->news_index++;
        refresh_news(b);
    }
}
