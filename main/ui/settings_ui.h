#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Settings is a stack of full screens. Each builder loads its own screen and
 * lets LVGL delete the outgoing one, so only the visible page exists. */

/** Root settings page. Also used as the "back" target from every sub-page. */
void settings_ui_open(void);

void settings_ui_open_faces(void);
void settings_ui_open_timezone(void);
void settings_ui_open_wifi(void);
void settings_ui_open_ble(void);
void settings_ui_open_pad(void);
void settings_ui_open_games(void);

/**
 * Start DOOM.
 *
 * Shared by the settings page's Play button and the toolbar icon, so both
 * paths take the same route in and the same route back out.
 */
void games_ui_launch_doom(void);

/** Slide `page` in from the right (forward) or left (back) and load it. */
void settings_ui_push(lv_obj_t *page, bool forward);

#ifdef __cplusplus
}
#endif
