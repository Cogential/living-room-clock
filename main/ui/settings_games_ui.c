#include "bsp/esp-bsp.h"
#include "doom/app_doom.h"
#include "input/app_pad.h"
#include "ui/settings_ui.h"
#include "ui/ui.h"
#include "ui/ui_fonts.h"
#include "lvgl.h"
#include "ui/ui_theme.h"
#include "ui/ui_widgets.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "games";

/*
 * Doom takes over the panel and blocks until the player quits, so it cannot run
 * on the LVGL task - that task is what would otherwise be servicing the very
 * timers Doom is starving. It gets its own task, and LVGL is left frozen behind
 * it: nothing invalidates while the clock screens are not being touched, so
 * LVGL does not fight Doom for the frame buffer.
 */
/* ---------------------------------------------------------------- loading -- */

static lv_obj_t *s_load_screen;
static lv_obj_t *s_load_bar;
static lv_obj_t *s_load_stage;
static lv_obj_t *s_load_percent;

/*
 * Built on its own screen rather than over the settings page: Doom is about to
 * take the whole panel, and this is the transition into that.
 */
void app_doom_loading_begin(void)
{
    if (!bsp_display_lock(2000)) {
        return;
    }

    s_load_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_load_screen);
    lv_obj_set_style_bg_opa(s_load_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_load_screen, lv_color_hex(0x0A0A0C), 0);
    lv_obj_remove_flag(s_load_screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(s_load_screen);
    lv_label_set_text(title, "DOOM");
    lv_obj_set_style_text_font(title, ui_font(UI_FONT_MONO, FACE_MONO_XL), 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xB01508), 0);
    lv_obj_set_style_text_letter_space(title, 12, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -110);

    /* Track and fill, rather than lv_bar, so the fill is a plain rectangle and
     * cannot animate behind our back while Doom is hogging the CPU. */
    lv_obj_t *track = lv_obj_create(s_load_screen);
    lv_obj_remove_style_all(track);
    lv_obj_set_size(track, 620, 14);
    lv_obj_align(track, LV_ALIGN_CENTER, 0, 70);
    lv_obj_set_style_radius(track, 7, 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(track, lv_color_hex(0x241A18), 0);

    s_load_bar = lv_obj_create(track);
    lv_obj_remove_style_all(s_load_bar);
    lv_obj_set_size(s_load_bar, 0, 14);
    lv_obj_align(s_load_bar, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_radius(s_load_bar, 7, 0);
    lv_obj_set_style_bg_opa(s_load_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_load_bar, lv_color_hex(0xD9401B), 0);

    s_load_stage = lv_label_create(s_load_screen);
    lv_label_set_text(s_load_stage, "Starting");
    lv_obj_set_style_text_font(s_load_stage, ui_font(UI_FONT_MEDIUM, UI_TEXT_MD), 0);
    lv_obj_set_style_text_color(s_load_stage, lv_color_hex(0x8A8078), 0);
    lv_obj_align(s_load_stage, LV_ALIGN_CENTER, 0, 116);

    s_load_percent = lv_label_create(s_load_screen);
    lv_label_set_text(s_load_percent, "0%");
    lv_obj_set_style_text_font(s_load_percent, ui_font(UI_FONT_MEDIUM, FACE_LABEL_L), 0);
    lv_obj_set_style_text_color(s_load_percent, lv_color_hex(0xF0E6DC), 0);
    lv_obj_align(s_load_percent, LV_ALIGN_CENTER, 0, 20);

    lv_screen_load(s_load_screen);
    bsp_display_unlock();
}

void app_doom_loading_progress(int percent, const char *stage)
{
    if (s_load_bar == NULL) {
        return;
    }
    /* Short timeout: a stalled repaint must never hold up Doom's start-up. */
    if (!bsp_display_lock(200)) {
        return;
    }

    lv_obj_set_width(s_load_bar, (620 * percent) / 100);
    lv_label_set_text_fmt(s_load_percent, "%d%%", percent);
    lv_label_set_text(s_load_stage, stage);
    lv_obj_align(s_load_stage, LV_ALIGN_CENTER, 0, 116);
    lv_obj_align(s_load_percent, LV_ALIGN_CENTER, 0, 20);

    bsp_display_unlock();
}

void app_doom_loading_end(void)
{
    s_load_bar = NULL;
    s_load_stage = NULL;
    s_load_percent = NULL;
    /* The screen itself is left for ui_close_settings() to replace: deleting it
     * here would leave LVGL with no active screen while Doom draws. */
    s_load_screen = NULL;
}

static void doom_task(void *arg)
{
    (void)arg;

    esp_err_t err = app_doom_run();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "doom exited: %s", esp_err_to_name(err));
    }

    /*
     * Go back to the clock.
     *
     * The loading screen is still the active LVGL screen at this point - Doom
     * drew over the frame buffer beneath it without ever replacing it - so
     * merely invalidating left the loading bar on screen, frozen on its last
     * stage. ui_close_app() loads a face over it and restores the chrome.
     */
    if (bsp_display_lock(2000)) {
        ui_close_app();
        bsp_display_unlock();
    }

    vTaskDelete(NULL);
}

void games_ui_launch_doom(void)
{
    if (!app_doom_available()) {
        ESP_LOGE(TAG, "doom1.wad missing");
        return;
    }

    /*
     * 16 KB, not the 32 KB this started with.
     *
     * Task stacks come out of internal RAM, which is the scarce pool on this
     * board, and 32 KB simply could not be allocated once the UI, Wi-Fi, BLE
     * and USB host were up - xTaskCreate returned pdFAIL and the button
     * silently did nothing. The reference port runs Doom on the main task with
     * an 8 KB stack, so this is still double what it needs.
     */
    if (xTaskCreate(doom_task, "doom", 16384, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "could not start doom: no internal RAM for the stack");
    }
}

static void launch_cb(lv_event_t *e)
{
    (void)e;
    games_ui_launch_doom();
}

static void back_cb(lv_event_t *e)
{
    (void)e;
    settings_ui_open();
}

void settings_ui_open_games(void)
{
    lv_obj_t *page = w_page();
    w_header(page, "DOOM", back_cb, NULL);
    lv_obj_t *body = w_body(page);

    w_section(body, "STATUS");
    w_info_row(body, "WAD", app_doom_available() ? "doom1.wad" : "Not found", NULL);

    w_info_row(body, "Controller",
               app_pad_state() == PAD_STATE_READY ? app_pad_name() : "Not connected",
               NULL);

    w_section(body, "CONTROLS");
    w_info_row(body, "Move", "Left stick / D-pad", NULL);
    w_info_row(body, "Turn", "Right stick", NULL);
    w_info_row(body, "Fire", "Cross or R2", NULL);
    w_info_row(body, "Open / use", "Square or L2", NULL);
    w_info_row(body, "Strafe", "L1 / R1", NULL);
    w_info_row(body, "Menu", "Options", NULL);
    w_info_row(body, "Quit to clock", "PS + Create", NULL);
    w_info_row(body, "No controller?", "Tap screen, then the X", NULL);

    w_section(body, "");
    w_action_row(body, "Play DOOM", W_ACCENT, launch_cb, NULL);

    lv_obj_t *note = lv_label_create(body);
    lv_label_set_text(note,
                      "Doom takes over the whole screen and the clock stops\n"
                      "updating until you quit. Plug the controller in first -\n"
                      "there is no touch control.");
    lv_obj_set_style_text_color(note, lv_color_hex(W_DIM), 0);
    lv_obj_set_style_text_font(note, ui_font(UI_FONT_MEDIUM, UI_TEXT_SM), 0);
    lv_obj_set_style_pad_all(note, 10, 0);

    settings_ui_push(page, true);
}
