/*
 * Living room clock - ESP32-P4-WIFI6-Touch-LCD-7B.
 *
 * The display comes up first and unconditionally: networking is best-effort so
 * a bad password or a dead router degrades to "clock running on the RTC"
 * rather than a blank screen.
 */
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "bsp/touch.h"
#include "esp_log.h"
#include "esp_system.h"
#include "lvgl.h"

#include "app_settings.h"
#include "ble/app_ble.h"
#include "doom/app_doom.h"
#include "input/app_pad.h"
#include "net/app_api.h"
#include "net/app_net.h"
#include "ui/ui.h"

static const char *TAG = "clock";

/*
 * Bring the display up the way bsp_display_start_with_config() does, but keep
 * the panel handle.
 *
 * The BSP creates the panel in a local and hands only the lv_display_t back,
 * and the adapter exposes no getter for it - so there is no way to recover it
 * afterwards. Doom needs it to reach the MIPI-DSI frame buffer directly, so the
 * few calls the BSP makes are repeated here instead.
 *
 * If the BSP is updated, check this against its bsp_display_start_with_config.
 */
static lv_display_t *display_start(bsp_display_cfg_t *cfg,
                                   esp_lcd_panel_handle_t *out_panel,
                                   esp_lcd_touch_handle_t *out_touch)
{
    ESP_ERROR_CHECK(esp_lv_adapter_init(&cfg->lv_adapter_cfg));
    ESP_ERROR_CHECK(bsp_display_brightness_init());

    bsp_lcd_handles_t handles;
    ESP_ERROR_CHECK(bsp_display_new_with_handles(NULL, &handles));
    *out_panel = handles.panel;

    const esp_lv_adapter_display_config_t disp_cfg = {
        .panel = handles.panel,
        .panel_io = handles.io,
        .profile = {
            .interface = ESP_LV_ADAPTER_PANEL_IF_MIPI_DSI,
            .rotation = cfg->rotation,
            .hor_res = BSP_LCD_H_RES,
            .ver_res = BSP_LCD_V_RES,
            .buffer_height = 50,
            .use_psram = false,
            .enable_ppa_accel = false,
            .require_double_buffer = false,
        },
        .tear_avoid_mode = cfg->tear_avoid_mode,
    };
    lv_display_t *disp = esp_lv_adapter_register_display(&disp_cfg);
    if (disp == NULL) {
        return NULL;
    }

    esp_lcd_touch_handle_t tp = NULL;
    const bsp_touch_config_t touch_config = {
        .flags = {
            .swap_xy = cfg->touch_flags.swap_xy,
            .mirror_x = cfg->touch_flags.mirror_x,
            .mirror_y = cfg->touch_flags.mirror_y,
        },
    };
    ESP_ERROR_CHECK(bsp_touch_new(&touch_config, &tp));
    *out_touch = tp;

    const esp_lv_adapter_touch_config_t touch_cfg =
        ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, tp);
    if (esp_lv_adapter_register_touch(&touch_cfg) == NULL) {
        return NULL;
    }

    ESP_ERROR_CHECK(esp_lv_adapter_start());
    return disp;
}

void app_main(void)
{
    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_180,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL,
        /*
         * No touch mirroring, despite what Waveshare's own example sets.
         *
         * This adapter version already maps touch through `rotation`, so adding
         * mirror_x + mirror_y on top rotates the input a second time: taps
         * landed in the diagonally opposite corner, which reads as "the button
         * does nothing" rather than as an obviously broken axis. If you ever
         * change `rotation`, re-check this together with it.
         */
        .touch_flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_touch_handle_t touch = NULL;
    lv_display_t *display = display_start(&cfg, &panel, &touch);
    ESP_ERROR_CHECK(display != NULL ? ESP_OK : ESP_FAIL);
    app_doom_set_panel(panel);
    app_doom_set_touch(touch);

    /* Settings first: networking reads the stored credentials, and the UI reads
     * the stored face and time format. */
    settings_init();
    settings_apply_timezone();

    /* Mounts the storage partition, so it also reports whether the WAD made
     * it into flash - worth knowing at boot rather than from the menu. */
    /* A panic inside Doom reboots the board, and the reason is the only trace
     * left once the console has scrolled. */
    ESP_LOGI(TAG, "reset reason: %d", (int)esp_reset_reason());
    ESP_LOGI(TAG, "doom: %s", app_doom_available() ? "doom1.wad ready" : "no wad");

    app_net_start();
    app_ble_start();
    app_pad_start();
    app_api_start();

    /* Everything touching LVGL runs under the port lock. */
    ESP_ERROR_CHECK(bsp_display_lock(-1) ? ESP_OK : ESP_ERR_TIMEOUT);
    ui_start();
    bsp_display_unlock();

    /* backlight_on() drives the PWM to 100%, so the stored level has to be
     * applied after it or the setting is silently overridden at every boot.
     * The UI tick re-applies this with the real hour once the clock is read,
     * which is what brings night dimming in. */
    bsp_display_backlight_on();
    settings_apply_brightness(-1);
    ESP_LOGI(TAG, "clock up");
}
