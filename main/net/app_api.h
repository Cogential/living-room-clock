#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Live data from the example.com gateway.
 *
 * A background task refreshes each feed on its own schedule and keeps the
 * parsed result in a small fixed-size struct. Nothing here allocates on behalf
 * of the UI and no call blocks: the dashboard reads whatever was last fetched,
 * so a slow or failed request never stalls a frame.
 */

#define API_NEWS_MAX   6
#define API_QUOTE_MAX  4

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
    } items[API_NEWS_MAX];
} api_news_t;

/*
 * Hacker News top stories.
 *
 * Kept apart from api_news_t rather than merged into it: these carry scores and
 * comment counts that the headline feed has no equivalent for, and the UI shows
 * them in their own slot.
 */
#define API_HN_MAX 6

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

/*
 * Barcode lookup, on demand rather than on a schedule.
 *
 * app_food_request() returns immediately and does the fetch on a short-lived
 * task, so the UI can show a spinner instead of freezing for the round trip.
 */
typedef struct {
    bool  ok;        /* a lookup completed */
    bool  found;     /* ...and the product existed */
    char  code[16];
    char  name[64];
    char  brands[64];
    char  nutriscore[4];
    int   nova;
    float energy_kcal, fat, saturated_fat, carbohydrates, sugars, proteins, salt;
} api_food_t;

void app_food_request(const char *code);
bool app_food_busy(void);
bool app_food_result(api_food_t *out);

/** Start the refresh task. Safe to call before Wi-Fi is up. */
void app_api_start(void);

/** False when no token was compiled in; the dashboard says so rather than spin. */
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
