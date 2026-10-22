#include "ui/ui.h"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "app_settings.h"
#include "ble/app_ble.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "net/app_net.h"
#include "sdkconfig.h"
#include "ui/settings_ui.h"
#include "ui/ui_fonts.h"
#include "ui/ui_theme.h"

static const char *TAG = "ui";

#define TICK_PERIOD_MS   33     /* ~30 fps: enough for a smooth second hand */
#define TRANSITION_MS    320
#define CHROME_IDLE_MS   8000   /* hide the dock after this much no-touch */
/* The face a fresh install starts on. Resolved by name so the list can be
 * reordered without silently changing the default. */
#define DEFAULT_FACE_NAME "Phosphor"

static const clock_face_t *const s_faces[] = {
    &face_dashboard,
    &face_photo,
    &face_minimal,
    &face_cyberpunk,
    &face_synthwave,
    &face_phosphor,
    &face_rapture,
    &face_harvest,
    &face_tavern,
    &face_voxel,
    &face_lab,
    &face_grove,
    &face_cuckoo,
};
#define FACE_COUNT ((int)(sizeof(s_faces) / sizeof(s_faces[0])))

static const clock_face_t *s_face;
static int s_face_index = -1;

static lv_obj_t *s_status_bar;
static lv_obj_t *s_status_net_label;
static lv_obj_t *s_status_gear_label;
static lv_obj_t *s_status_doom_icon;
static lv_obj_t *s_nav;
static lv_obj_t *s_nav_prev_label;
static lv_obj_t *s_nav_next_label;
static lv_obj_t *s_setup_banner;
#ifdef CLOCK_TOUCH_DEBUG
static lv_obj_t *s_touch_debug;
#endif

static uint32_t s_last_touch_ms;
#ifdef CLOCK_FACE_SOAK_TEST
static uint32_t s_tick_count;
static uint32_t s_worst_gap_ms;
#endif
static bool s_chrome_visible = true;
static app_net_state_t s_last_net_state = (app_net_state_t)-1;

static bool s_settings_open = false;
/* Set when returning from settings so the face slides back in from the left
 * rather than appearing with no transition. */
static bool s_force_back_anim = false;
static int s_last_applied_hour = -1;

/* ------------------------------------------------------------------ time -- */

int ui_display_hour(const struct tm *t)
{
    if (settings()->hour_24) {
        return t->tm_hour;
    }
    int h = t->tm_hour % 12;
    return h == 0 ? 12 : h;
}

const char *ui_meridiem(const struct tm *t)
{
    if (settings()->hour_24) {
        return "";
    }
    return t->tm_hour < 12 ? "AM" : "PM";
}

static void sample_time(clock_time_t *out)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &out->tm);
    out->millis = (int)(tv.tv_usec / 1000);
    /* Anything before 2024 means the RTC never had a real time set. */
    out->time_valid = (out->tm.tm_year + 1900) >= 2024;
}

/* ---------------------------------------------------------------- chrome -- */

static void set_opa_exec(void *obj, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

/* Only runs on the hide animation: taking the object out of the tree any
 * earlier would let an invisible dock swallow taps. */
static void hide_when_faded(lv_anim_t *a)
{
    lv_obj_add_flag((lv_obj_t *)a->var, LV_OBJ_FLAG_HIDDEN);
}

static void chrome_set_visible(bool visible)
{
    if (visible == s_chrome_visible) {
        return;
    }
    s_chrome_visible = visible;

    lv_obj_t *targets[] = { s_status_bar, s_nav };
    for (int i = 0; i < 2; i++) {
        if (visible) {
            lv_obj_remove_flag(targets[i], LV_OBJ_FLAG_HIDDEN);
        }

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, targets[i]);
        lv_anim_set_exec_cb(&a, set_opa_exec);
        lv_anim_set_values(&a, lv_obj_get_style_opa(targets[i], 0),
                           visible ? LV_OPA_COVER : LV_OPA_TRANSP);
        lv_anim_set_duration(&a, TRANSITION_MS);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        if (!visible) {
            lv_anim_set_completed_cb(&a, hide_when_faded);
        }
        lv_anim_start(&a);
    }
}

static void chrome_wake(void)
{
    s_last_touch_ms = lv_tick_get();
    chrome_set_visible(true);
}

static void apply_chrome_palette(bool light)
{
    lv_color_t text     = light ? lv_color_hex(0xF4F6FB) : lv_color_hex(0x2A2822);
    lv_color_t dim      = light ? lv_color_hex(0x9AA3BC) : lv_color_hex(0x8A857B);
    lv_color_t surface  = light ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x1A1917);
    lv_opa_t   surf_opa = light ? LV_OPA_10 : LV_OPA_10;

    (void)surface;
    (void)surf_opa;

    lv_obj_set_style_text_color(s_status_net_label, dim, 0);
    lv_obj_set_style_text_color(s_status_gear_label, text, 0);
    lv_obj_set_style_text_color(s_nav_prev_label, text, 0);
    lv_obj_set_style_text_color(s_nav_next_label, text, 0);
    lv_obj_set_style_bg_color(s_nav, surface, 0);
    lv_obj_set_style_bg_opa(s_nav, light ? LV_OPA_30 : LV_OPA_20, 0);
}

static void update_net_status(void)
{
    app_net_state_t state = app_net_state();
    if (state == s_last_net_state) {
        return;
    }
    s_last_net_state = state;

    const char *text;
    switch (state) {
    /* Words rather than icons: these labels are drawn in Ubuntu, which has
     * no glyphs at the LV_SYMBOL_* codepoints. */
    case APP_NET_DISABLED:   text = "Offline";     break;
    case APP_NET_CONNECTING: text = "Connecting";  break;
    case APP_NET_CONNECTED:  text = "Syncing";     break;
    case APP_NET_SYNCED:     text = "Synced";      break;
    default:                 text = "No network";  break;
    }
    lv_label_set_text(s_status_net_label, text);
}

/* ----------------------------------------------------------------- faces -- */

/*
 * Each face is its own LVGL screen so switching can use lv_screen_load_anim.
 *
 * The obvious alternative, crossfading two faces inside one screen, needs a
 * full-screen alpha layer every frame: measured here it cost roughly a fifth of
 * the frame budget and stalled for ~600 ms mid-animation. Sliding screens only
 * repositions what already exists, and the direction doubles as feedback for
 * which way the user swiped.
 */
void ui_show_face(int index)
{
    if (index < 0 || index >= FACE_COUNT) {
        return;
    }
    if (index == s_face_index) {
        return;
    }

    const clock_face_t *face = s_faces[index];

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    face->create(scr);

    int previous = s_face_index;
    s_face = face;
    s_face_index = index;

    /* Render the correct time before the screen is ever shown. */
    clock_time_t now;
    sample_time(&now);
    face->tick(&now);

    if (s_force_back_anim) {
        s_force_back_anim = false;
        lv_screen_load_anim(scr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, TRANSITION_MS, 0, true);
    } else if (previous < 0) {
        lv_screen_load(scr);
    } else {
        /* Moving forward through the list sends the old face out to the left. */
        bool forward = ((index - previous + FACE_COUNT) % FACE_COUNT) <= FACE_COUNT / 2;
        lv_screen_load_anim(scr,
                            forward ? LV_SCR_LOAD_ANIM_MOVE_LEFT
                                    : LV_SCR_LOAD_ANIM_MOVE_RIGHT,
                            TRANSITION_MS, 0, true /* delete the outgoing screen */);
    }


    apply_chrome_palette(face->light_chrome);

    ESP_LOGI(TAG, "face -> %s", face->name);
}

#ifdef CLOCK_TOUCH_DEBUG
/*
 * On-screen touch readout.
 *
 * Reading this off the panel avoids needing a serial capture running at the
 * exact moment someone taps, which is awkward to coordinate remotely. It shows
 * where LVGL thinks the finger landed and which handler, if any, claimed it -
 * enough to tell a coordinate-mapping fault from an event-wiring one.
 */
static void touch_debug_note(const char *what)
{
    if (s_touch_debug == NULL) {
        return;
    }
    lv_point_t p = { 0, 0 };
    lv_indev_t *indev = lv_indev_active();
    if (indev) {
        lv_indev_get_point(indev, &p);
    }
    lv_label_set_text_fmt(s_touch_debug, "touch %d,%d  %s", (int)p.x, (int)p.y, what);
    lv_obj_move_foreground(s_touch_debug);
    ESP_LOGI(TAG, "TOUCHDEBUG: %d,%d %s", (int)p.x, (int)p.y, what);
}

static void build_touch_debug(lv_obj_t *parent)
{
    s_touch_debug = lv_label_create(parent);
    lv_obj_set_style_text_font(s_touch_debug, ui_font(UI_FONT_MEDIUM, UI_TEXT_MD), 0);
    lv_obj_set_style_text_color(s_touch_debug, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_touch_debug, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_touch_debug, lv_color_hex(0xFFE066), 0);
    lv_obj_set_style_pad_all(s_touch_debug, 10, 0);
    lv_obj_set_style_radius(s_touch_debug, 8, 0);
    lv_label_set_text(s_touch_debug, "touch -,-  (waiting)");
    lv_obj_align(s_touch_debug, LV_ALIGN_LEFT_MID, 20, 0);
}
#endif

/* Returns -1 when the name is not in the list, which happens whenever a face is
 * removed after someone selected it. */
static int face_index_by_name(const char *name)
{
    for (int i = 0; i < FACE_COUNT; i++) {
        if (strcmp(s_faces[i]->name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int ui_face_count(void)
{
    return FACE_COUNT;
}

const char *ui_face_name(int index)
{
    if (index < 0 || index >= FACE_COUNT) {
        return "";
    }
    return s_faces[index]->name;
}

int ui_face_current(void)
{
    return s_face_index;
}

int ui_face_selected(void)
{
    int index = face_index_by_name(settings()->face_name);
    if (index < 0) {
        index = face_index_by_name(DEFAULT_FACE_NAME);
    }
    return index < 0 ? 0 : index;
}

void ui_face_select(int index)
{
    if (index < 0 || index >= FACE_COUNT) {
        return;
    }
    strlcpy(settings()->face_name, s_faces[index]->name, sizeof(settings()->face_name));
    settings_save();
}

/* -------------------------------------------------------------- settings -- */

bool ui_app_open(void)
{
    return s_settings_open;
}

bool ui_settings_open(void)
{
    return s_settings_open;
}

void ui_open_settings(void)
{
    ui_open_app(settings_ui_open);
}

void ui_close_settings(void)
{
    ui_close_app();
}

void ui_open_app(void (*open_fn)(void))
{
    if (s_settings_open || open_fn == NULL) {
        return;
    }
    s_settings_open = true;

    /* The chrome lives on the top layer and would otherwise float above the
     * settings pages and swallow every tap meant for them. */
    lv_obj_add_flag(s_status_bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_nav, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_setup_banner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(lv_layer_top(), LV_OBJ_FLAG_CLICKABLE);

    open_fn();
}

void ui_close_app(void)
{
    if (!s_settings_open) {
        return;
    }
    s_settings_open = false;

    lv_obj_remove_flag(s_status_bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_nav, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lv_layer_top(), LV_OBJ_FLAG_CLICKABLE);

    settings_apply_timezone();
    s_last_applied_hour = -1;  /* re-evaluate night dimming immediately */

    /* The face screen was deleted when settings loaded, so rebuild it - and use
     * whatever the picker last selected rather than what was showing before. */
    int face = ui_face_selected();
    s_face_index = -1;
    s_face = NULL;
    s_force_back_anim = true;
    ui_show_face(face < 0 ? 0 : face);

    chrome_wake();
}

/* ---------------------------------------------------------------- events -- */

/* Step through the face list and remember where we landed, so the arrows and
 * the settings picker agree about which face is current. */
static void nav_cb(lv_event_t *e)
{
    int step = (int)(intptr_t)lv_event_get_user_data(e);
    int next = (s_face_index + step + FACE_COUNT) % FACE_COUNT;

    chrome_wake();
    ui_face_select(next);
    ui_show_face(next);
}

/* ui_open_app() has already hidden the chrome and stopped the tick by the time
 * the launch can fail, and nothing else would ever close the app again. */
static void launch_doom_from_dock(void)
{
    if (!games_ui_launch_doom()) {
        ui_close_app();
    }
}

static void doom_btn_cb(lv_event_t *e)
{
    (void)e;
    ui_open_app(launch_doom_from_dock);
}

static void settings_btn_cb(lv_event_t *e)
{
    (void)e;
#ifdef CLOCK_TOUCH_DEBUG
    touch_debug_note("-> GEAR");
#endif
    ui_open_settings();
}

/* Largest finger travel, in pixels, that still counts as a tap on the face. */
#define FACE_TAP_SLOP 24

static void chrome_input_cb(lv_event_t *e)
{
    /* LVGL still sends SHORT_CLICKED when a swipe is released (the top layer is
     * not scrollable, so nothing cancels it), with the release point as the
     * position. Remember where the press started and whether it became a
     * gesture, so a swipe to wake the chrome never opens a QR sheet. */
    static lv_point_t press_at;
    static bool press_swiped;

    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
#ifdef CLOCK_TOUCH_DEBUG
        /* Only fires for taps that miss every clickable child. */
        touch_debug_note("(background)");
#endif
        lv_indev_get_point(lv_indev_active(), &press_at);
        press_swiped = false;
        chrome_wake();
        return;
    }

    if (code == LV_EVENT_GESTURE) {
        /* Faces are chosen in Settings only, so a swipe just wakes the chrome
         * rather than cycling the list under the user. */
        press_swiped = true;
        chrome_wake();
        return;
    }

    /* SHORT_CLICKED, not CLICKED: a long press or a brush past the screen
     * should not act on whatever the face has under the finger. */
    if (code == LV_EVENT_SHORT_CLICKED && s_face && s_face->tap && !ui_app_open()) {
        lv_point_t p;
        lv_indev_get_point(lv_indev_active(), &p);
        if (press_swiped || LV_ABS(p.x - press_at.x) > FACE_TAP_SLOP ||
            LV_ABS(p.y - press_at.y) > FACE_TAP_SLOP) {
            return;
        }
        s_face->tap(p.x, p.y);
    }
}

static void tick_cb(lv_timer_t *t)
{
    (void)t;

#ifdef CLOCK_FACE_SOAK_TEST
    /* The gap between ticks is the render budget: if LVGL cannot keep up, this
     * timer is what slips, so it measures the load directly. */
    static uint32_t last_tick_ms;
    if (last_tick_ms != 0) {
        uint32_t gap = lv_tick_elaps(last_tick_ms);
        if (gap > s_worst_gap_ms) s_worst_gap_ms = gap;
    }
    last_tick_ms = lv_tick_get();
    s_tick_count++;
#endif

    clock_time_t now;
    sample_time(&now);

    /* While settings is up the face screen has been deleted, and the face
     * modules still hold pointers into it - ticking would write through them. */
    if (s_settings_open) {
        return;
    }

    if (s_face != NULL) {
        s_face->tick(&now);
    }

    /* Night dimming only needs re-evaluating when the hour rolls over. */
    if (now.tm.tm_hour != s_last_applied_hour) {
        s_last_applied_hour = now.tm.tm_hour;
        settings_apply_brightness(now.tm.tm_hour);
    }

    update_net_status();

    /* The banner is the one thing that stays up while the chrome hides. */
    bool banner_hidden = lv_obj_has_flag(s_setup_banner, LV_OBJ_FLAG_HIDDEN);
    if (now.time_valid && !banner_hidden) {
        lv_obj_add_flag(s_setup_banner, LV_OBJ_FLAG_HIDDEN);
    } else if (!now.time_valid && banner_hidden) {
        lv_obj_remove_flag(s_setup_banner, LV_OBJ_FLAG_HIDDEN);
    }

    if (s_chrome_visible && lv_tick_elaps(s_last_touch_ms) > CHROME_IDLE_MS) {
        chrome_set_visible(false);
    }
}

#ifdef CLOCK_SETTINGS_SOAK
/*
 * Walk the whole settings tree unattended.
 *
 * Every page here is built entirely in code and torn down by LVGL's screen
 * loader, so a missing null-check or a timer outliving its page only shows up
 * when the page is actually constructed. This drives that without a finger.
 */
static void settings_soak_cb(lv_timer_t *t)
{
    (void)t;
    static int step = 0;

    switch (step) {
    case 0: ui_open_settings();          break;
    case 1: settings_ui_open_wifi();     break;
    case 2: settings_ui_open();          break;
    case 3:
        settings_ui_open_ble();
        /* The page only scans on a tap, so drive it here too. */
        app_ble_scan_start();
        break;
    case 4: settings_ui_open();          break;
    case 5: settings_ui_open_timezone(); break;
    case 6: settings_ui_open();          break;
    case 7: settings_ui_open_faces();    break;
    case 8: settings_ui_open();          break;
    case 9: settings_ui_open_pad();      break;
    case 10: settings_ui_open();         break;
    case 11: ui_close_settings();        break;
    default: break;
    }

    ESP_LOGW(TAG, "soak: step=%d internal=%u ble=%s devices=%d", step,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             app_ble_state_text(), app_ble_device_count());

    step = (step + 1) % 12;
}
#endif

#ifdef CLOCK_FACE_SOAK_TEST
/* Build with -DCLOCK_FACE_SOAK_TEST to cycle every face unattended and watch
 * the heap, which is the only way to exercise all four without a finger. */
static void soak_cb(lv_timer_t *t)
{
    (void)t;
    ESP_LOGW(TAG, "soak: face=%s free=%u ticks=%u worst_gap=%ums",
             s_face->name,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (unsigned)s_tick_count, (unsigned)s_worst_gap_ms);
    s_tick_count = 0;
    s_worst_gap_ms = 0;
    ui_show_face((s_face_index + 1) % FACE_COUNT);
}
#endif

/* ----------------------------------------------------------------- build -- */

static void build_status_bar(lv_obj_t *parent)
{
    s_status_bar = lv_obj_create(parent);
    lv_obj_remove_style_all(s_status_bar);
    lv_obj_set_size(s_status_bar, UI_SCREEN_W, UI_STATUS_H);
    lv_obj_align(s_status_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_remove_flag(s_status_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(s_status_bar, LV_OBJ_FLAG_CLICKABLE);

    /* The gear is the only control on the clock screen. The face name used to
     * sit on the left, but it is chrome about the app rather than about the
     * time, and it has no business appearing over a wall clock. */
    lv_obj_t *gear = lv_button_create(s_status_bar);
    lv_obj_remove_style_all(gear);
    lv_obj_set_size(gear, 64, UI_STATUS_H);
    lv_obj_align(gear, LV_ALIGN_RIGHT_MID, -16, 0);
    lv_obj_set_style_radius(gear, 12, 0);
    lv_obj_set_style_bg_opa(gear, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(gear, LV_OPA_10, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(gear, lv_color_hex(0x808080), LV_STATE_PRESSED);
    lv_obj_add_event_cb(gear, settings_btn_cb, LV_EVENT_CLICKED, NULL);

    s_status_gear_label = lv_label_create(gear);
    lv_label_set_text(s_status_gear_label, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(s_status_gear_label, ui_symbol_font(24), 0);
    lv_obj_center(s_status_gear_label);

    /*
     * DOOM, left of the gear. The marine's face from the status bar, drawn
     * from rectangles at 28 px: hair, face, two eyes, a mouth. Crude, but it
     * reads at a glance and needs no image decoder or extra font.
     */
    lv_obj_t *doom = lv_button_create(s_status_bar);
    lv_obj_remove_style_all(doom);
    lv_obj_set_size(doom, 64, UI_STATUS_H);
    lv_obj_align(doom, LV_ALIGN_RIGHT_MID, -84, 0);
    lv_obj_set_style_radius(doom, 12, 0);
    lv_obj_set_style_bg_opa(doom, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(doom, LV_OPA_10, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(doom, lv_color_hex(0x808080), LV_STATE_PRESSED);
    lv_obj_add_event_cb(doom, doom_btn_cb, LV_EVENT_CLICKED, NULL);

    s_status_doom_icon = lv_obj_create(doom);
    lv_obj_remove_style_all(s_status_doom_icon);
    lv_obj_set_size(s_status_doom_icon, 28, 28);
    lv_obj_center(s_status_doom_icon);
    lv_obj_remove_flag(s_status_doom_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_status_doom_icon, LV_OBJ_FLAG_SCROLLABLE);
    ui_add_rect(s_status_doom_icon, 4, 2, 20, 7, 0x7A3B12, LV_OPA_COVER, 2);  /* hair */
    ui_add_rect(s_status_doom_icon, 5, 8, 18, 15, 0xE0A070, LV_OPA_COVER, 3);  /* face */
    ui_add_rect(s_status_doom_icon, 8, 12, 4, 3, 0x241309, LV_OPA_COVER, 0);   /* eyes */
    ui_add_rect(s_status_doom_icon, 16, 12, 4, 3, 0x241309, LV_OPA_COVER, 0);
    ui_add_rect(s_status_doom_icon, 10, 19, 8, 2, 0x8C2F1F, LV_OPA_COVER, 0);  /* mouth */

    s_status_net_label = lv_label_create(s_status_bar);
    lv_obj_set_style_text_font(s_status_net_label, ui_font(UI_FONT_MEDIUM, UI_TEXT_MD), 0);
    lv_label_set_text(s_status_net_label, "");
    lv_obj_align(s_status_net_label, LV_ALIGN_RIGHT_MID, -160, 0);
}

/* Until SNTP lands, the faces are drawing an epoch date. Say so plainly rather
 * than letting a confidently wrong time sit on the wall. */
static void build_setup_banner(lv_obj_t *parent)
{
    s_setup_banner = lv_obj_create(parent);
    lv_obj_remove_style_all(s_setup_banner);
    lv_obj_set_size(s_setup_banner, LV_SIZE_CONTENT, 44);
    lv_obj_align(s_setup_banner, LV_ALIGN_TOP_MID, 0, UI_STATUS_H + 8);
    lv_obj_set_style_radius(s_setup_banner, 22, 0);
    lv_obj_set_style_bg_opa(s_setup_banner, LV_OPA_80, 0);
    lv_obj_set_style_bg_color(s_setup_banner, lv_color_hex(0xB45309), 0);
    lv_obj_set_style_pad_hor(s_setup_banner, 22, 0);
    lv_obj_remove_flag(s_setup_banner, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(s_setup_banner, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *label = lv_label_create(s_setup_banner);
    lv_obj_set_style_text_font(label, ui_font(UI_FONT_MEDIUM, UI_TEXT_MD), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFF7ED), 0);
    lv_label_set_text(label, "Waiting for time sync");
    lv_obj_center(label);
}

/*
 * Previous / next face, bottom left.
 *
 * Chrome, so it fades out with the status bar rather than sitting over the
 * clock. It does overlap the first market cell of the HUD while visible - eight
 * seconds over one price is a better trade than shrinking every face's layout.
 */
static void build_nav(lv_obj_t *parent)
{
    s_nav = lv_obj_create(parent);
    lv_obj_remove_style_all(s_nav);
    lv_obj_set_size(s_nav, 136, 64);
    lv_obj_align(s_nav, LV_ALIGN_BOTTOM_LEFT, 16, -14);
    lv_obj_set_style_radius(s_nav, 32, 0);
    lv_obj_remove_flag(s_nav, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(s_nav, LV_OBJ_FLAG_CLICKABLE);

    static const struct { const char *symbol; int step; int x; } BUTTONS[2] = {
        { LV_SYMBOL_LEFT,  -1,  4 },
        { LV_SYMBOL_RIGHT, +1, 68 },
    };

    lv_obj_t *labels[2];
    for (int i = 0; i < 2; i++) {
        lv_obj_t *b = lv_button_create(s_nav);
        lv_obj_remove_style_all(b);
        lv_obj_set_size(b, 64, 56);
        lv_obj_set_pos(b, BUTTONS[i].x, 4);
        lv_obj_set_style_radius(b, 28, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_20, LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x808080), LV_STATE_PRESSED);
        lv_obj_add_event_cb(b, nav_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)BUTTONS[i].step);

        labels[i] = lv_label_create(b);
        lv_label_set_text(labels[i], BUTTONS[i].symbol);
        /* Built-in font: the arrows are LV_SYMBOL glyphs, which the Ubuntu
         * faces have nothing at. */
        lv_obj_set_style_text_font(labels[i], ui_symbol_font(24), 0);
        lv_obj_center(labels[i]);
    }
    s_nav_prev_label = labels[0];
    s_nav_next_label = labels[1];
}

void ui_start(void)
{
    /* Chrome lives on the top layer so it survives every screen load, and
     * because it sits above the faces it also receives all the touches -
     * the faces themselves are purely decorative. */
    lv_obj_t *top = lv_layer_top();
    lv_obj_remove_flag(top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(top, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(top, chrome_input_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(top, chrome_input_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(top, chrome_input_cb, LV_EVENT_SHORT_CLICKED, NULL);

    build_status_bar(top);
    build_nav(top);
    build_setup_banner(top);
#ifdef CLOCK_TOUCH_DEBUG
    build_touch_debug(top);
#endif

    int face = ui_face_selected();
    if (settings()->face_name[0] == '\0') {
        /* Pin the resolved default so the choice is explicit from here on. */
        ui_face_select(face);
    }
    ui_show_face(face);

    s_last_touch_ms = lv_tick_get();
    lv_timer_create(tick_cb, TICK_PERIOD_MS, NULL);

#ifdef CLOCK_FACE_SOAK_TEST
    lv_timer_create(soak_cb, 4000, NULL);
#endif
#ifdef CLOCK_SETTINGS_SOAK
    lv_timer_create(settings_soak_cb, 2500, NULL);
#endif
}
