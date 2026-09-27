#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * DOOM, running full screen.
 *
 * Doom owns the panel while it runs: it renders 320x200 and the PPA scales that
 * straight into the MIPI-DSI frame buffer, so LVGL is frozen for the duration
 * rather than composited with. Press the PS button to quit back to the clock.
 *
 * The engine is doomgeneric (GPLv2) with the freely redistributable shareware
 * doom1.wad, mounted from the storage partition.
 */

/** Hand Doom the panel it should draw into. Call once at start-up. */
void app_doom_set_panel(esp_lcd_panel_handle_t panel);

/**
 * Hand Doom the touch controller.
 *
 * LVGL is frozen while Doom owns the panel, so its input device never runs and
 * the panel has to be read directly. Without a controller plugged in this is
 * the only way out of the game.
 */
void app_doom_set_touch(esp_lcd_touch_handle_t touch);

/** True when the WAD is present and Doom can be launched. */
bool app_doom_available(void);

/**
 * Launch Doom. Blocks the calling task until the player quits, so call it from
 * a task that can afford to be gone - not from the LVGL timer.
 */
esp_err_t app_doom_run(void);

/*
 * Loading screen, implemented by the UI layer.
 *
 * Called from the Doom task while LVGL is still running, before Doom takes the
 * display. begin/end bracket the start-up; progress reports a percentage and
 * the stage name.
 */
void app_doom_loading_begin(void);
void app_doom_loading_progress(int percent, const char *stage);
void app_doom_loading_end(void);

/** Report a start-up stage; drives the loading bar. */
void app_doom_stage(const char *stage);

/** True while Doom owns the display. */
bool app_doom_running(void);

#ifdef __cplusplus
}
#endif
