#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Live data from a data gateway (CLOCK_API_BASE in secrets.h; see the README
 * for the endpoints it has to serve).
 *
 * A background task refreshes each feed on its own schedule and keeps the
 * parsed result in a small fixed-size struct. Nothing here allocates on behalf
 * of the UI and no call blocks: the dashboard reads whatever was last fetched,
 * so a slow or failed request never stalls a frame.
 */

/* Everything the feeds return (18 news, 20 HN today), so the headline cards
 * have stories to step forward through. In PSRAM, so size is cheap. */
#define API_NEWS_MAX   20
#define API_QUOTE_MAX  4

/*
 * Article links, for the QR code a headline card shows when tapped. Measured
 * against the live feeds: news links reach ~104 chars and HN ~130. A link that
 * does not fit is dropped rather than truncated - a cut-off URL would encode a
 * QR code that leads nowhere.
 */
#define API_URL_MAX    200

typedef struct {
    bool     ok;
    uint32_t fetched;          /* epoch of the last successful fetch */
    int      temp, feels, humidity, wind;
    int      aqi, hi, lo;
    char     desc[24];
    struct {
        char day[6];
        int  hi, lo;
    } forecast[3];
    char     sunrise[6];       /* "HH:MM" local */
    char     sunset[6];
} api_weather_t;

typedef struct {
    bool     ok;
    uint32_t fetched;
    int      count;
    struct {
        /* Measured against the live feed: titles reach ~94 chars and sources
         * ~54 (some are full publication names, not short codes). */
        char title[120];
        char source[64];
        char url[API_URL_MAX];     /* empty if missing or too long */
    } items[API_NEWS_MAX];
} api_news_t;

/*
 * Hacker News top stories.
 *
 * Kept apart from api_news_t rather than merged into it: these carry scores and
 * comment counts that the headline feed has no equivalent for, and the UI shows
 * them in their own slot.
 */
#define API_HN_MAX 20

typedef struct {
    bool     ok;
    uint32_t fetched;
    int      count;
    struct {
        /* Measured against /hn/top: titles run to ~79 chars, but HN allows 80
         * and the gateway does not truncate, so this has room to spare. */
        char title[120];
        /* -1 when the feed reports null, which happens on "job" posts. */
        int  score;
        int  comments;
        /* The linked article. Text posts (Ask HN) link nowhere else, so for
         * those this is the story's own page on HN. */
        char url[API_URL_MAX];
    } items[API_HN_MAX];
} api_hn_t;

typedef struct {
    bool     ok;
    uint32_t fetched;
    int      count;
    struct {
        char  symbol[10];
        float price;
        float change_pct;
    } quotes[API_QUOTE_MAX];
    float    btc_usd, btc_change_pct;
    float    fear_greed;
    char     fear_greed_rating[16];
} api_markets_t;

/** Start the refresh task. Safe to call before Wi-Fi is up. */
void app_api_start(void);

/** False when no CLOCK_API_BASE was compiled in; the dashboard says so rather than spin. */
bool app_api_configured(void);

/**
 * Bumped after every successful fetch.
 *
 * The dashboard redraws only when this changes, so a face ticking at 30 fps
 * does not rebuild labels it already drew.
 */
uint32_t app_api_generation(void);

/** Snapshots. Return false when nothing has been fetched yet. */
bool app_api_weather(api_weather_t *out);
bool app_api_news(api_news_t *out);
bool app_api_hn(api_hn_t *out);
bool app_api_markets(api_markets_t *out);

/** Short status for the settings page: "Idle", "Updating", "Offline"... */
const char *app_api_status(void);

#ifdef __cplusplus
}
#endif
