#include "ui/ui_hud.h"

#include <stdio.h>
#include <string.h>

#include "esp_attr.h"
#include "net/app_api.h"
#include "ui/qr_sheet.h"
#include "ui/ui_theme.h"

/* Headline titles: three lines of Medium 28, whose line height is 1.149x. */
#define TITLE_SIZE  UI_TEXT_LG
#define TITLE_H     (3 * 33)

/* Each headline holds for twice this; the two cards alternate. */
#define ROTATE_MS   9000

/*
 * Headline history. Each card remembers the stories it has shown, so the
 * arrows can step back through them; stepping forward past the newest brings
 * up the next story from the feed. Browsing holds the card still, and when the
 * hold runs out it returns to the newest story and rotation resumes.
 */
#define HIST_MAX        30
#define BROWSE_HOLD_MS  60000

/* Touch targets for the arrows, at the right end of a card's header. Larger
 * than the buttons drawn in them: a finger is not a stylus. */
#define NAV_W  60
#define NAV_H  44

/*
 * Where the digits land. The two clock families put their numerals at
 * different heights inside the same line box - measured at 210 px, Ubuntu Thin
 * inks y 48..199 and Ubuntu Mono 42..178 - so Thin is raised to clear the date
 * at HUD_DATE_Y. Both then span roughly y 26..178.
 */
#define TIME_Y_THIN  (-22)
#define TIME_Y_MONO  0

typedef struct {
    lv_obj_t *symbol, *price, *change;
} market_cell_t;

static ui_hud_theme_t s_theme;

static lv_obj_t *s_time, *s_ghost, *s_date, *s_secs_a, *s_secs_b;
static int s_last_minute = -1, s_last_sec = -1;

static lv_obj_t *s_wx_temp, *s_wx_desc, *s_wx_feels, *s_wx_humid, *s_wx_wind,
                *s_wx_aqi, *s_wx_rise, *s_wx_set;
static lv_obj_t *s_wx_fcst_day[3], *s_wx_fcst_val[3];

static market_cell_t s_cells[HUD_MKT_CELLS];

/*
 * The last snapshot of each feed. In PSRAM: with URLs and twenty items these
 * are ~8 KB apiece, too much for the LVGL task's stack and not worth internal
 * RAM.
 */
EXT_RAM_BSS_ATTR static api_news_t s_news;
EXT_RAM_BSS_ATTR static api_hn_t s_hn;

typedef enum { CARD_NEWS = 0, CARD_HN, CARD_COUNT } card_id_t;

/* A story as a card shows it - a copy, so it outlives the feed it came from. */
typedef struct {
    char title[120];
    char meta[64];               /* source, or points and comments */
    char url[API_URL_MAX];
} story_t;

typedef struct {
    story_t  hist[HIST_MAX];     /* ring buffer, newest at `newest` */
    int      count, newest;
    int      offset;             /* 0 = the newest story, n = n stories back */
    int      feed_index;         /* which feed item the newest came from */
    bool     holding;            /* browsing: rotation paused */
    uint32_t browsed;            /* lv_tick of the last arrow tap */
} history_t;

/* Outlives the face: switching faces rebuilds the widgets, not what was read.
 * ~11.5 KB apiece, in PSRAM. */
EXT_RAM_BSS_ATTR static history_t s_hist[CARD_COUNT];

typedef struct {
    lv_obj_t *title, *meta, *prev, *next;
} card_ui_t;

static card_ui_t s_card[CARD_COUNT];

static uint32_t s_seen_generation;
static uint32_t s_rotated;
static uint8_t s_rotate_slot;

/* ---------------------------------------------------------------- build -- */

void ui_text_shadow(lv_obj_t *label, int radius)
{
    /*
     * A real per-glyph shadow, not a box shadow: lv_obj_set_style_shadow_* on a
     * label shadows its rectangle, which over a photo reads as a dark slab.
     * drop_shadow_* renders the glyphs into a layer and blurs that instead. It
     * is the most expensive thing a label can ask for, so keep radii small.
     */
    lv_obj_set_style_drop_shadow_color(label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_drop_shadow_opa(label, LV_OPA_80, 0);
    lv_obj_set_style_drop_shadow_radius(label, radius, 0);
    lv_obj_set_style_drop_shadow_offset_x(label, 0, 0);
    lv_obj_set_style_drop_shadow_offset_y(label, 1, 0);
    lv_obj_set_style_drop_shadow_quality(label, LV_BLUR_QUALITY_SPEED, 0);
}

static lv_obj_t *text(lv_obj_t *parent, int x, int y, int w,
                      ui_font_family_t family, int size, uint32_t colour)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, ui_font(family, size), 0);
    lv_obj_set_style_text_color(l, lv_color_hex(colour), 0);
    if (w > 0) {
        lv_obj_set_width(l, w);
    }
    lv_obj_set_pos(l, x, y);
    lv_label_set_text(l, "");
    return l;
}

/* Section heading: the thing the eye looks for first. */
static lv_obj_t *tag(lv_obj_t *parent, int x, int y, uint32_t colour, const char *name)
{
    lv_obj_t *l = text(parent, x, y, 0, s_theme.tag_family, UI_TEXT_SM, colour);
    lv_obj_set_style_text_letter_space(l, 3, 0);
    if (name) {
        lv_label_set_text_fmt(l, "%s%s", s_theme.tag_prefix, name);
    }
    return l;
}

/* A panel with a border and a solid notch in its accent. Children are
 * positioned relative to it. */
static lv_obj_t *panel(lv_obj_t *root, int x, int y, int w, int h, uint32_t accent)
{
    if (s_theme.shadow_dx || s_theme.shadow_dy) {
        ui_add_rect(root, x + s_theme.shadow_dx, y + s_theme.shadow_dy, w, h,
                    s_theme.shadow, LV_OPA_COVER, s_theme.radius);
    }
    lv_obj_t *p = ui_add_rect(root, x, y, w, h, s_theme.panel, s_theme.panel_opa,
                              s_theme.radius);
    if (s_theme.border_w > 0) {
        lv_obj_set_style_border_color(p, lv_color_hex(accent), 0);
        lv_obj_set_style_border_opa(p, s_theme.border_opa, 0);
        lv_obj_set_style_border_width(p, s_theme.border_w, 0);
    }
    /* Clip the notch to rounded corners rather than letting it poke out. */
    lv_obj_set_style_clip_corner(p, s_theme.radius > 0, 0);
    ui_add_rect(p, 0, 0, 64, 4, accent, LV_OPA_COVER, 0);
    return p;
}

/* "KEY value" pair, the key dim and fixed-width so values line up. */
static lv_obj_t *key_value(lv_obj_t *parent, int x, int y, const char *key)
{
    lv_obj_t *k = text(parent, x, y + 3, 0, UI_FONT_MONO, UI_TEXT_SM, s_theme.dim);
    lv_label_set_text(k, key);
    return text(parent, x + 64, y, 110, UI_FONT_MEDIUM, UI_TEXT_MD, s_theme.ink);
}

static void build_clock(lv_obj_t *root)
{
    const ui_hud_theme_t *t = &s_theme;
    int y = t->clock_family == UI_FONT_MONO ? TIME_Y_MONO : TIME_Y_THIN;
    int size = t->clock_family == UI_FONT_MONO ? FACE_TIME_MONO : FACE_TIME_THIN;

    s_ghost = NULL;
    if (t->ghost_opa > 0) {
        s_ghost = text(root, HUD_CLOCK_X + t->ghost_dx, y + t->ghost_dy, HUD_CLOCK_W,
                       t->clock_family, size, t->ghost);
        lv_obj_set_style_text_opa(s_ghost, t->ghost_opa, 0);
        lv_obj_set_style_text_align(s_ghost, t->clock_align, 0);
        lv_label_set_text(s_ghost, "--:--");
    }

    s_time = text(root, HUD_CLOCK_X, y, HUD_CLOCK_W, t->clock_family, size, t->clock);
    lv_obj_set_style_text_align(s_time, t->clock_align, 0);
    lv_label_set_text(s_time, "--:--");

    s_date = text(root, HUD_CLOCK_X, HUD_DATE_Y, HUD_CLOCK_W, t->date_family, UI_TEXT_LG,
                  t->date);
    lv_obj_set_style_text_align(s_date, t->clock_align, 0);
    lv_obj_set_style_text_letter_space(s_date, t->date_family == UI_FONT_MONO ? 4 : 3, 0);

    if (t->text_shadow) {
        ui_text_shadow(s_time, 8);
        ui_text_shadow(s_date, 5);
    }

    /* Seconds. Only the fill moves, once a second, over a 6 px strip. */
    s_secs_a = s_secs_b = NULL;
    if (t->secs_style == HUD_SECS_NONE) {
        return;
    }
    ui_add_rect(root, HUD_SECS_X, HUD_SECS_Y, HUD_SECS_W, 6, t->secs_track,
                t->secs_track_opa, 3);
    s_secs_a = ui_add_rect(root, HUD_SECS_X, HUD_SECS_Y, 0, 6, t->secs, LV_OPA_COVER, 3);
    if (t->secs_style == HUD_SECS_DUAL) {
        s_secs_b = ui_add_rect(root, HUD_SECS_X + HUD_SECS_W, HUD_SECS_Y, 0, 6,
                               t->secs2, LV_OPA_COVER, 3);
    } else {
        for (int i = 0; i <= 4; i++) {
            ui_add_rect(root, HUD_SECS_X + i * (HUD_SECS_W - 2) / 4, HUD_SECS_Y - 5, 2, 16,
                        t->secs, LV_OPA_60, 0);
        }
    }
}

static void build_weather(lv_obj_t *root)
{
    const uint32_t accent = s_theme.weather;
    lv_obj_t *p = panel(root, HUD_WX_X, HUD_WX_Y, HUD_WX_W, HUD_WX_H, accent);
    tag(p, 14, 12, accent, "WEATHER");

    s_wx_temp = text(p, 14, 40, 0, UI_FONT_MONO, FACE_MONO_M, accent);
    s_wx_desc = text(p, 132, 48, HUD_WX_W - 150, UI_FONT_MEDIUM, UI_TEXT_LG, s_theme.ink);
    lv_label_set_long_mode(s_wx_desc, LV_LABEL_LONG_DOT);

    s_wx_feels = key_value(p, 14, 100, "FEELS");
    s_wx_humid = key_value(p, 196, 100, "HUMID");
    s_wx_wind  = key_value(p, 14, 132, "WIND");
    s_wx_aqi   = key_value(p, 196, 132, "AQI");
    s_wx_rise  = key_value(p, 14, 164, "RISE");
    s_wx_set   = key_value(p, 196, 164, "SET");

    ui_add_rect(p, 14, 198, HUD_WX_W - 30, 1, accent, LV_OPA_30, 0);

    int col_w = (HUD_WX_W - 28) / 3;
    for (int i = 0; i < 3; i++) {
        s_wx_fcst_day[i] = text(p, 14 + i * col_w, 211, 0, UI_FONT_MONO, UI_TEXT_SM,
                                s_theme.dim);
        s_wx_fcst_val[i] = text(p, 14 + i * col_w + 38, 208, 0, UI_FONT_MEDIUM, UI_TEXT_MD,
                                s_theme.ink);
    }
}

/* Drawn only: the chrome layer takes every touch and ui_hud_tap() hit-tests. */
static lv_obj_t *nav_button(lv_obj_t *card, int x, uint32_t accent, const char *symbol)
{
    lv_obj_t *b = ui_add_rect(card, x, 6, NAV_W - 12, 32, accent, LV_OPA_10, 6);
    lv_obj_set_style_border_color(b, lv_color_hex(accent), 0);
    lv_obj_set_style_border_opa(b, LV_OPA_60, 0);
    lv_obj_set_style_border_width(b, 1, 0);

    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, ui_symbol_font(20), 0);
    lv_obj_set_style_text_color(l, lv_color_hex(accent), 0);
    lv_label_set_text(l, symbol);
    lv_obj_center(l);
    return b;
}

/* A headline card: tag, detail and arrows on top, title below. */
static void build_card(lv_obj_t *root, card_id_t c, int x, uint32_t accent, const char *name)
{
    card_ui_t *u = &s_card[c];
    lv_obj_t *p = panel(root, x, HUD_CARD_Y, HUD_CARD_W, HUD_CARD_H, accent);
    tag(p, 14, 12, accent, name);

    u->prev = nav_button(p, HUD_CARD_W - 2 * NAV_W + 2, accent, LV_SYMBOL_LEFT);
    u->next = nav_button(p, HUD_CARD_W - NAV_W + 2, accent, LV_SYMBOL_RIGHT);

    /* Right-aligned against the arrows; long sources end in dots. */
    u->meta = text(p, HUD_CARD_W - 2 * NAV_W - 8 - 160, 12, 160, UI_FONT_MONO, UI_TEXT_SM,
                   s_theme.dim);
    lv_obj_set_style_text_align(u->meta, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(u->meta, LV_LABEL_LONG_DOT);
    lv_obj_set_height(u->meta, UI_TEXT_SM);

    /* Clipped at three lines rather than growing into the markets row. */
    u->title = text(p, 14, 42, HUD_CARD_W - 30, UI_FONT_MEDIUM, TITLE_SIZE, s_theme.ink);
    lv_label_set_long_mode(u->title, LV_LABEL_LONG_DOT);
    lv_obj_set_height(u->title, TITLE_H);
}

static void build_markets(lv_obj_t *root)
{
    const uint32_t accent = s_theme.markets;
    for (int i = 0; i < HUD_MKT_CELLS; i++) {
        lv_obj_t *p = panel(root, HUD_MARGIN + i * (HUD_MKT_W + HUD_MKT_GAP), HUD_MKT_Y,
                            HUD_MKT_W, HUD_MKT_H, accent);
        s_cells[i].symbol = tag(p, 14, 12, accent, NULL);
        s_cells[i].price  = text(p, 14, 38, HUD_MKT_W - 24, UI_FONT_MONO, FACE_MONO_M,
                                 s_theme.ink);
        lv_label_set_long_mode(s_cells[i].price, LV_LABEL_LONG_CLIP);
        s_cells[i].change = text(p, 14, 90, HUD_MKT_W - 24, UI_FONT_MEDIUM, UI_TEXT_MD,
                                 s_theme.dim);
        lv_label_set_long_mode(s_cells[i].change, LV_LABEL_LONG_CLIP);
    }
}

void ui_hud_create(lv_obj_t *root, const ui_hud_theme_t *theme)
{
    s_theme = *theme;
    if (s_theme.tag_prefix == NULL) {
        s_theme.tag_prefix = "";
    }

    build_clock(root);
    build_weather(root);
    build_card(root, CARD_NEWS, HUD_MARGIN, s_theme.news, "NEWS");
    build_card(root, CARD_HN, HUD_MARGIN + HUD_CARD_W + HUD_GAP, s_theme.hn, "HACKER NEWS");
    build_markets(root);

    s_last_minute = -1;
    s_last_sec = -1;
    s_seen_generation = UINT32_MAX;
    for (int c = 0; c < CARD_COUNT; c++) {
        s_hist[c].offset = 0;
        s_hist[c].holding = false;
    }
}

lv_obj_t *ui_hud_time_label(void)
{
    return s_time;
}

/* -------------------------------------------------------------- refresh -- */

static void upper(char *s)
{
    for (; *s; s++) {
        if (*s >= 'a' && *s <= 'z') *s -= 32;
    }
}

static void refresh_weather(void)
{
    api_weather_t w;
    if (!app_api_weather(&w)) {
        lv_label_set_text(s_wx_temp, "--°");
        lv_label_set_text(s_wx_desc, app_api_configured() ? "Waiting..." : "No data gateway");
        return;
    }

    lv_label_set_text_fmt(s_wx_temp, "%d°", w.temp);
    lv_label_set_text(s_wx_desc, w.desc);
    lv_label_set_text_fmt(s_wx_feels, "%d°", w.feels);
    lv_label_set_text_fmt(s_wx_humid, "%d%%", w.humidity);
    lv_label_set_text_fmt(s_wx_wind, "%d", w.wind);
    if (w.aqi >= 0) {
        lv_label_set_text_fmt(s_wx_aqi, "%d", w.aqi);
    } else {
        lv_label_set_text(s_wx_aqi, "--");
    }
    lv_label_set_text(s_wx_rise, w.sunrise[0] ? w.sunrise : "--");
    lv_label_set_text(s_wx_set, w.sunset[0] ? w.sunset : "--");

    for (int i = 0; i < 3; i++) {
        if (w.forecast[i].day[0]) {
            char day[8];
            strlcpy(day, w.forecast[i].day, sizeof(day));
            upper(day);
            lv_label_set_text(s_wx_fcst_day[i], day);
            /* No degree signs: at "-3°/-12°" the column runs into the next. */
            lv_label_set_text_fmt(s_wx_fcst_val[i], "%d/%d",
                                  w.forecast[i].hi, w.forecast[i].lo);
        } else {
            lv_label_set_text(s_wx_fcst_day[i], "");
            lv_label_set_text(s_wx_fcst_val[i], "");
        }
    }
}

/* Item `i` of a card's feed, wrapping; false while the feed is empty. */
static bool feed_story(card_id_t c, int i, story_t *out)
{
    if (c == CARD_NEWS) {
        if (s_news.count == 0) {
            return false;
        }
        const typeof(s_news.items[0]) *it = &s_news.items[i % s_news.count];
        strlcpy(out->title, it->title, sizeof(out->title));
        strlcpy(out->meta, it->source, sizeof(out->meta));
        strlcpy(out->url, it->url, sizeof(out->url));
        return true;
    }

    if (s_hn.count == 0) {
        return false;
    }
    const typeof(s_hn.items[0]) *it = &s_hn.items[i % s_hn.count];
    strlcpy(out->title, it->title, sizeof(out->title));
    strlcpy(out->url, it->url, sizeof(out->url));
    /* Both are -1 on "job" posts, which carry neither. */
    if (it->score >= 0 && it->comments >= 0) {
        snprintf(out->meta, sizeof(out->meta), "%d PTS  %d CMT", it->score, it->comments);
    } else if (it->score >= 0) {
        snprintf(out->meta, sizeof(out->meta), "%d PTS", it->score);
    } else {
        strlcpy(out->meta, "JOB", sizeof(out->meta));
    }
    return true;
}

static story_t *hist_at(history_t *h, int back)
{
    return &h->hist[(h->newest - back + HIST_MAX) % HIST_MAX];
}

/* Record the feed's current item as the newest story, unless it already is -
 * a refresh often returns the same headline, and it should not stack up. */
static void hist_push_current(card_id_t c)
{
    history_t *h = &s_hist[c];
    story_t s;
    if (!feed_story(c, h->feed_index, &s)) {
        return;
    }
    if (h->count > 0 && strcmp(hist_at(h, 0)->title, s.title) == 0) {
        *hist_at(h, 0) = s;  /* same story: pick up new points and comments */
        return;
    }
    h->newest = (h->newest + 1) % HIST_MAX;
    h->hist[h->newest] = s;
    if (h->count < HIST_MAX) {
        h->count++;
    }
}

static void card_show(card_id_t c)
{
    history_t *h = &s_hist[c];
    card_ui_t *u = &s_card[c];

    if (h->count == 0) {
        const char *waiting = c == CARD_NEWS ? "Waiting for headlines" : "Waiting for Hacker News";
        lv_label_set_text(u->title, app_api_configured() ? waiting : "");
        lv_label_set_text(u->meta, "");
    } else {
        const story_t *s = hist_at(h, h->offset);
        lv_label_set_text(u->title, s->title);
        if (h->offset > 0) {
            lv_label_set_text_fmt(u->meta, "%d BACK · %s", h->offset, s->meta);
        } else {
            lv_label_set_text(u->meta, s->meta);
        }
    }

    /* Faded when there is nothing that way. Small objects, so the layer that
     * a whole-object opacity costs is cheap. */
    bool older = h->offset < h->count - 1;
    bool newer = h->offset > 0 || (c == CARD_NEWS ? s_news.count : s_hn.count) > 0;
    lv_obj_set_style_opa(u->prev, older ? LV_OPA_COVER : LV_OPA_30, 0);
    lv_obj_set_style_opa(u->next, newer ? LV_OPA_COVER : LV_OPA_30, 0);
}

/* Move on to the next story in the feed and show it. */
static void card_advance(card_id_t c)
{
    history_t *h = &s_hist[c];
    h->feed_index++;
    hist_push_current(c);
    h->offset = 0;
    card_show(c);
}

static void set_cell(int i, const char *symbol, const char *price,
                     const char *change, uint32_t change_colour)
{
    if (i >= HUD_MKT_CELLS) {
        return;
    }
    lv_label_set_text(s_cells[i].symbol, symbol);
    lv_label_set_text(s_cells[i].price, price);
    lv_label_set_text(s_cells[i].change, change);
    lv_obj_set_style_text_color(s_cells[i].change, lv_color_hex(change_colour), 0);
}

/* Prices sized to the cell: seven mono characters at 48 px is its width. */
static void format_price(char *out, size_t len, float price)
{
    snprintf(out, len, price >= 1000.0f ? "%.0f" : "%.2f", price);
}

static void refresh_markets(void)
{
    api_markets_t m;
    int n = 0;
    char price[16], change[24];

    if (app_api_markets(&m)) {
        for (int i = 0; i < m.count && n < HUD_MKT_CELLS - 2; i++) {
            format_price(price, sizeof(price), m.quotes[i].price);
            snprintf(change, sizeof(change), "%+.2f%%", m.quotes[i].change_pct);
            set_cell(n++, m.quotes[i].symbol, price, change,
                     m.quotes[i].change_pct >= 0.0f ? s_theme.up : s_theme.down);
        }
        if (m.btc_usd > 0.0f) {
            format_price(price, sizeof(price), m.btc_usd);
            snprintf(change, sizeof(change), "%+.2f%%", m.btc_change_pct);
            set_cell(n++, "BTC", price, change,
                     m.btc_change_pct >= 0.0f ? s_theme.up : s_theme.down);
        }
        if (m.fear_greed >= 0.0f) {
            char rating[16];
            strlcpy(rating, m.fear_greed_rating, sizeof(rating));
            upper(rating);
            /* Fear reads as down, greed as up, the middle neutral. */
            uint32_t colour = m.fear_greed < 45.0f ? s_theme.down
                            : m.fear_greed > 55.0f ? s_theme.up : s_theme.ink;
            snprintf(price, sizeof(price), "%.0f", m.fear_greed);
            set_cell(n++, "FEAR/GREED", price, rating, colour);
        }
    }

    const char *waiting = app_api_configured() ? "--" : "";
    while (n < HUD_MKT_CELLS) {
        set_cell(n++, "", waiting, "", s_theme.dim);
    }
}

/* ----------------------------------------------------------------- tick -- */

static void default_date(const struct tm *tm, char *out, size_t len)
{
    strftime(out, len, "%a %e %b %Y", tm);
    upper(out);
}

static void update_clock(const clock_time_t *now)
{
    if (now->tm.tm_min != s_last_minute) {
        s_last_minute = now->tm.tm_min;

        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d", ui_display_hour(&now->tm), now->tm.tm_min);
        lv_label_set_text(s_time, buf);
        if (s_ghost) {
            lv_label_set_text(s_ghost, buf);
        }

        /* Rebuilt per minute rather than per day, because AM/PM rides along. */
        char date[64];
        (s_theme.date_fmt ? s_theme.date_fmt : default_date)(&now->tm, date, sizeof(date));
        const char *meridiem = ui_meridiem(&now->tm);
        if (meridiem[0]) {
            strlcat(date, "  ", sizeof(date));
            strlcat(date, meridiem, sizeof(date));
        }
        lv_label_set_text(s_date, date);
    }

    if (s_secs_a && now->tm.tm_sec != s_last_sec) {
        s_last_sec = now->tm.tm_sec;
        int sec = now->tm.tm_sec > 59 ? 59 : now->tm.tm_sec;  /* leap second */
        if (s_secs_b) {
            int half = (HUD_SECS_W / 2) * sec / 59;
            lv_obj_set_width(s_secs_a, half);
            lv_obj_set_width(s_secs_b, half);
            lv_obj_set_x(s_secs_b, HUD_SECS_X + HUD_SECS_W - half);
        } else {
            lv_obj_set_width(s_secs_a, HUD_SECS_W * sec / 59);
        }
    }
}

void ui_hud_tick(const clock_time_t *now)
{
    update_clock(now);

    uint32_t gen = app_api_generation();
    if (gen != s_seen_generation) {
        s_seen_generation = gen;
        refresh_weather();
        if (!app_api_news(&s_news)) {
            s_news.count = 0;
        }
        if (!app_api_hn(&s_hn)) {
            s_hn.count = 0;
        }
        /* A card being browsed keeps its place; the rest pick up the refresh.
         * An empty card has no place to keep, so it never holds out. */
        for (int c = 0; c < CARD_COUNT; c++) {
            if (!s_hist[c].holding || s_hist[c].count == 0) {
                s_hist[c].holding = false;
                hist_push_current(c);
                card_show(c);
            }
        }
        refresh_markets();
        s_rotated = lv_tick_get();
        return;
    }

    /* A browsed card that has been left alone goes back to the newest story. */
    for (int c = 0; c < CARD_COUNT; c++) {
        history_t *h = &s_hist[c];
        if (h->holding && lv_tick_elaps(h->browsed) >= BROWSE_HOLD_MS) {
            h->holding = false;
            h->offset = 0;
            card_show(c);
        }
    }

    /* One card changes per period, alternating, so the two never swap at
     * the same moment. Only one card is relabelled. */
    if (lv_tick_elaps(s_rotated) < ROTATE_MS) {
        return;
    }
    s_rotated = lv_tick_get();
    card_id_t c = (s_rotate_slot++ & 1) ? CARD_HN : CARD_NEWS;
    if (!s_hist[c].holding) {
        card_advance(c);
    }
}

void ui_hud_tap(int x, int y)
{
    if (y < HUD_CARD_Y || y >= HUD_CARD_Y + HUD_CARD_H) {
        return;
    }

    card_id_t c;
    int card_x;
    if (x >= HUD_MARGIN && x < HUD_MARGIN + HUD_CARD_W) {
        c = CARD_NEWS;
        card_x = HUD_MARGIN;
    } else if (x >= HUD_MARGIN + HUD_CARD_W + HUD_GAP &&
               x < HUD_MARGIN + 2 * HUD_CARD_W + HUD_GAP) {
        c = CARD_HN;
        card_x = HUD_MARGIN + HUD_CARD_W + HUD_GAP;
    } else {
        return;
    }

    history_t *h = &s_hist[c];
    int cx = x - card_x, cy = y - HUD_CARD_Y;

    /* The arrows: back through what was shown, forward toward the newest and
     * then on to stories not seen yet. */
    if (cy < NAV_H && cx >= HUD_CARD_W - 2 * NAV_W) {
        if (cx < HUD_CARD_W - NAV_W) {
            if (h->offset < h->count - 1) {
                h->offset++;
            }
        } else if (h->offset > 0) {
            h->offset--;
        } else {
            card_advance(c);
        }
        /* Nothing to browse yet: holding would only keep the card on
         * "Waiting" after the first headlines land. */
        h->holding = h->count > 0;
        h->browsed = lv_tick_get();
        card_show(c);
        return;
    }

    /* Anywhere else on the card: the story on screen, as a QR code. */
    if (h->count == 0) {
        return;
    }
    const story_t *s = hist_at(h, h->offset);
    char heading[32];
    snprintf(heading, sizeof(heading), "%s%s", s_theme.tag_prefix,
             c == CARD_NEWS ? "NEWS" : "HACKER NEWS");
    ui_qr_sheet_show(c == CARD_NEWS ? s_theme.news : s_theme.hn, heading,
                     s->title, s->meta, s->url);
}
