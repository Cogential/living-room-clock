#include "doom/app_doom.h"

#include <setjmp.h>
#include <string.h>
#include <sys/stat.h>

#include "doomgeneric.h"
#include "doomkeys.h"
#include "driver/ppa.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "bsp/esp-bsp.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "ble/app_ble.h"
#include "input/app_pad.h"
#include "net/app_net.h"

static const char *TAG = "doom";

/*
 * Symbols this doomgeneric fork expects the platform to define.
 *
 * The save directory is patched into m_config.c and m_misc.c rather than being
 * configurable, and the resampler settings are bound as config variables even
 * when the sound modules are compiled out.
 */
char doomEsp_savedir[32] = "/doom";
int use_libsamplerate = 0;
float libsamplerate_scale = 0.65f;

#define DOOM_W 320
#define DOOM_H 200
#define LCD_W  1024
#define LCD_H  600

#define WAD_DIR  "/doom"
#define WAD_PATH "/doom/doom1.wad"

static esp_lcd_panel_handle_t s_panel;
static esp_lcd_touch_handle_t s_touch;
static uint16_t *s_frame_buffer;      /* the panel's own buffer */
static uint16_t *s_doom_rgb565;       /* 320x200 staging buffer, DMA-capable */
static ppa_client_handle_t s_ppa;
static volatile bool s_running;
static volatile bool s_quit;
static QueueHandle_t s_keys;
static bool s_drew_first_frame;

/*
 * Doom reports fatal problems through I_Error, which with ORIGCODE disabled in
 * this fork prints and then *returns* - leaving the caller to carry on in a
 * broken state and fall over somewhere unrelated. Unwinding here instead.
 */
static jmp_buf s_fatal_jmp;
static bool s_fatal_ready;

void doom_platform_fatal(const char *msg)
{
    ESP_LOGE(TAG, "fatal: %s", msg);

    if (s_fatal_ready) {
        s_fatal_ready = false;
        longjmp(s_fatal_jmp, 1);
    }
    /* Nothing to unwind to: better to stop here than continue corrupted. */
    vTaskDelete(NULL);
}

/*
 * Start-up takes about ten seconds, most of it in R_Init composing textures.
 * The stage breadcrumbs already mark the way through, so they double as
 * progress: no extra hooks in Doom, and the weighting reflects where the time
 * actually goes rather than spacing the stages evenly.
 */
static const struct {
    const char *stage;
    int percent;
} STAGE_PROGRESS[] = {
    { "run entered",              2 },
    { "framebuffer ok",           4 },
    { "ppa ok",                   6 },
    { "calling doomgeneric_Create", 8 },
    { "Z_Init",                  12 },
    { "V_Init",                  16 },
    { "M_LoadDefaults",          20 },
    { "W_Init (wad)",            28 },
    { "I_Init",                  36 },
    { "M_Init",                  40 },
    { "R_Init (textures)",       45 },   /* the long one */
    { "S_Init",                  88 },
    { "D_CheckNetGame",          92 },
    { "HU_Init",                 95 },
    { "ST_Init",                 98 },
    { "wad loaded, looping",    100 },
};
#define STAGE_COUNT ((int)(sizeof(STAGE_PROGRESS) / sizeof(STAGE_PROGRESS[0])))

static int progress_for(const char *stage)
{
    for (int i = 0; i < STAGE_COUNT; i++) {
        if (strcmp(STAGE_PROGRESS[i].stage, stage) == 0) {
            return STAGE_PROGRESS[i].percent;
        }
    }
    return -1;
}

void app_doom_stage(const char *stage)
{
    int percent = progress_for(stage);
    if (percent >= 0) {
        app_doom_loading_progress(percent, stage);
    }
    ESP_LOGD(TAG, "stage: %s", stage);
}


typedef struct {
    uint8_t key;
    uint8_t pressed;
} key_event_t;

void app_doom_set_panel(esp_lcd_panel_handle_t panel)
{
    s_panel = panel;
}

void app_doom_set_touch(esp_lcd_touch_handle_t touch)
{
    s_touch = touch;
}

/* ------------------------------------------------------------ exit button -- */

/*
 * A way out without a controller.
 *
 * Touch and frame buffer coordinates do *not* share a frame, which took a test
 * to establish. The button is drawn in frame buffer space, where the user's
 * top-right corner is the bottom-left because the panel is mounted upside down
 * and Doom rotates its output in the PPA to compensate. The touch controller
 * reports in the upright frame the user actually sees, so a tap has to be
 * flipped before it can be tested against the button.
 *
 * The giveaway was that tapping made the button appear - so reads worked - but
 * tapping the button itself did nothing.
 */
#define EXIT_BTN_SIZE 76
#define EXIT_BTN_X    28
#define EXIT_BTN_Y    (LCD_H - EXIT_BTN_SIZE - 28)
#define EXIT_VISIBLE_MS 6000

static volatile bool s_exit_visible;
static uint32_t s_exit_shown_at;

static inline void fb_px(int x, int y, uint16_t colour)
{
    if (x < 0 || y < 0 || x >= LCD_W || y >= LCD_H) {
        return;
    }
    s_frame_buffer[y * LCD_W + x] = colour;
}

static void fb_fill(int x, int y, int w, int h, uint16_t colour)
{
    for (int row = 0; row < h; row++) {
        int yy = y + row;
        if (yy < 0 || yy >= LCD_H) {
            continue;
        }
        for (int col = 0; col < w; col++) {
            fb_px(x + col, yy, colour);
        }
    }
}

/* Drawn straight into the frame buffer after the game frame, so Doom redrawing
 * the whole screen every tick does not erase it. */
static void draw_exit_button(void)
{
    const uint16_t bg = 0x18C3;   /* dark grey, RGB565 */
    const uint16_t fg = 0xFFFF;

    fb_fill(EXIT_BTN_X, EXIT_BTN_Y, EXIT_BTN_SIZE, EXIT_BTN_SIZE, bg);

    /* Two diagonals, three pixels thick, inset from the edges. */
    const int inset = 22;
    const int span = EXIT_BTN_SIZE - inset * 2;
    for (int i = 0; i < span; i++) {
        for (int t = 0; t < 3; t++) {
            fb_px(EXIT_BTN_X + inset + i + t, EXIT_BTN_Y + inset + i, fg);
            fb_px(EXIT_BTN_X + inset + i + t, EXIT_BTN_Y + inset + span - 1 - i, fg);
        }
    }
}

static bool point_in_exit_button(int x, int y)
{
    return x >= EXIT_BTN_X && x < EXIT_BTN_X + EXIT_BTN_SIZE &&
           y >= EXIT_BTN_Y && y < EXIT_BTN_Y + EXIT_BTN_SIZE;
}

/* One tap reveals the button, a second on it quits; it fades out on its own so
 * it is not sitting over the game permanently. */
static void poll_touch(void)
{
    if (s_touch == NULL) {
        return;
    }
    if (esp_lcd_touch_read_data(s_touch) != ESP_OK) {
        return;
    }

    esp_lcd_touch_point_data_t point = { 0 };
    uint8_t count = 0;
    static bool was_pressed;

    bool pressed = esp_lcd_touch_get_data(s_touch, &point, &count, 1) == ESP_OK &&
                   count > 0;

    /* Upright touch space -> frame buffer space. */
    int fb_x = LCD_W - 1 - (int)point.x;
    int fb_y = LCD_H - 1 - (int)point.y;

    if (pressed && !was_pressed) {
        if (s_exit_visible && point_in_exit_button(fb_x, fb_y)) {
            ESP_LOGI(TAG, "exit button tapped");
            s_quit = true;
        } else {
            s_exit_visible = true;
            s_exit_shown_at = xTaskGetTickCount() * portTICK_PERIOD_MS;
        }
    }
    was_pressed = pressed;

    if (s_exit_visible) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - s_exit_shown_at > EXIT_VISIBLE_MS) {
            s_exit_visible = false;
        }
    }
}

bool app_doom_running(void)
{
    return s_running;
}

/* ------------------------------------------------------------------- wad -- */

static bool s_spiffs_mounted;

static bool mount_wad(void)
{
    if (s_spiffs_mounted) {
        return true;
    }

    esp_vfs_spiffs_conf_t conf = {
        .base_path = WAD_DIR,
        .partition_label = "storage",
        .max_files = 4,
        .format_if_mount_failed = false,
    };

    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spiffs mount failed: %s", esp_err_to_name(err));
        return false;
    }
    s_spiffs_mounted = true;
    return true;
}

bool app_doom_available(void)
{
    if (!mount_wad()) {
        return false;
    }
    struct stat st;
    return stat(WAD_PATH, &st) == 0 && st.st_size > 0;
}

/* ---------------------------------------------------------------- display -- */

/*
 * Called by doomgeneric once per rendered frame.
 *
 * This fork emits RGB565 into DG_ScreenBuffer (which is over-allocated at four
 * bytes per pixel), so only the first W*H*2 bytes are meaningful. The PPA does
 * the 3.2x upscale to the panel in hardware; doing it on the CPU would not keep
 * up at this resolution.
 */
void p4_doom_draw_frame(const uint32_t *buffer)
{
    if (s_doom_rgb565 == NULL || s_frame_buffer == NULL) {
        return;
    }

    memcpy(s_doom_rgb565, buffer, DOOM_W * DOOM_H * 2);
    esp_cache_msync(s_doom_rgb565, DOOM_W * DOOM_H * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    ppa_srm_oper_config_t srm = {
        .in = {
            .buffer = s_doom_rgb565,
            .pic_w = DOOM_W,
            .pic_h = DOOM_H,
            .block_w = DOOM_W,
            .block_h = DOOM_H,
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .out = {
            .buffer = s_frame_buffer,
            .buffer_size = LCD_W * LCD_H * 2,
            .pic_w = LCD_W,
            .pic_h = LCD_H,
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        /*
         * The panel is mounted upside down and LVGL compensates with
         * ROTATE_180 when it renders. Doom writes to the frame buffer directly
         * and never goes through LVGL, so it has to do its own turn - without
         * this the game is inverted relative to the clock.
         */
        .rotation_angle = PPA_SRM_ROTATION_ANGLE_180,
        .scale_x = (float)LCD_W / (float)DOOM_W,
        .scale_y = (float)LCD_H / (float)DOOM_H,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };

    ppa_do_scale_rotate_mirror(s_ppa, &srm);

    /*
     * Present the buffer we just filled.
     *
     * The adapter runs the panel triple-buffered, and the DPI driver decides
     * which buffer to scan out by matching the pointer passed here against its
     * own list - so writing pixels into frame buffer 0 is invisible until this
     * call makes it the current one. Without it Doom runs happily and draws
     * nothing, which is exactly what it looked like.
     */
    if (s_exit_visible) {
        draw_exit_button();
    }

    /* Presents the buffer and syncs the cache for the whole region, which also
     * covers the pixels just written by hand above. */
    esp_lcd_panel_draw_bitmap(s_panel, 0, 0, LCD_W, LCD_H, s_frame_buffer);

    if (!s_drew_first_frame) {
        s_drew_first_frame = true;
        ESP_LOGI(TAG, "first frame presented");
    }
}

/* ------------------------------------------------------------------ input -- */

/*
 * DualSense -> Doom.
 *
 * Doom takes discrete key events, so the analogue stick is turned into held
 * arrow keys either side of a threshold. The threshold is well above the
 * deadzone so resting thumbs do not creep forward.
 */
#define STICK_THRESHOLD 0.45f

typedef struct {
    uint32_t mask;
    uint8_t  key;
} button_map_t;

static const button_map_t BUTTON_MAP[] = {
    { PAD_BTN_CROSS,    KEY_FIRE },
    { PAD_BTN_SQUARE,   KEY_USE },
    { PAD_BTN_R2,       KEY_FIRE },
    { PAD_BTN_L2,       KEY_USE },
    { PAD_BTN_L1,       KEY_STRAFE_L },
    { PAD_BTN_R1,       KEY_STRAFE_R },
    { PAD_BTN_OPTIONS,  KEY_ESCAPE },
    { PAD_BTN_CIRCLE,   KEY_ESCAPE },
    { PAD_BTN_TRIANGLE, KEY_ENTER },
    { PAD_BTN_TOUCHPAD, KEY_TAB },
};
#define BUTTON_MAP_COUNT ((int)(sizeof(BUTTON_MAP) / sizeof(BUTTON_MAP[0])))

static void queue_key(uint8_t key, bool pressed)
{
    key_event_t ev = { .key = key, .pressed = pressed };
    xQueueSend(s_keys, &ev, 0);
}

/* Emit a key event only when the held state actually flips. */
static void edge(bool now, bool *was, uint8_t key)
{
    if (now != *was) {
        *was = now;
        queue_key(key, now);
    }
}

static void input_task(void *arg)
{
    (void)arg;

    bool up = false, down = false, left = false, right = false;
    bool held[BUTTON_MAP_COUNT];
    memset(held, 0, sizeof(held));

    while (!s_quit) {
        poll_touch();

        pad_input_t in;
        if (app_pad_get(&in)) {
            /* Quitting is a deliberate two-step so it cannot happen mid-fight. */
            if ((in.buttons & PAD_BTN_PS) && (in.buttons & PAD_BTN_CREATE)) {
                s_quit = true;
                break;
            }

            bool want_up    = in.ly < -STICK_THRESHOLD || in.dpad == 0 ||
                              in.dpad == 1 || in.dpad == 7;
            bool want_down  = in.ly > STICK_THRESHOLD || in.dpad == 3 ||
                              in.dpad == 4 || in.dpad == 5;
            bool want_left  = in.lx < -STICK_THRESHOLD || in.dpad == 5 ||
                              in.dpad == 6 || in.dpad == 7;
            bool want_right = in.lx > STICK_THRESHOLD || in.dpad == 1 ||
                              in.dpad == 2 || in.dpad == 3;

            edge(want_up, &up, KEY_UPARROW);
            edge(want_down, &down, KEY_DOWNARROW);
            /* Right stick turns, left stick strafes - the modern layout. */
            edge(want_left, &left, KEY_STRAFE_L);
            edge(want_right, &right, KEY_STRAFE_R);

            static bool turn_l = false, turn_r = false;
            edge(in.rx < -STICK_THRESHOLD, &turn_l, KEY_LEFTARROW);
            edge(in.rx > STICK_THRESHOLD, &turn_r, KEY_RIGHTARROW);

            for (int i = 0; i < BUTTON_MAP_COUNT; i++) {
                edge((in.buttons & BUTTON_MAP[i].mask) != 0, &held[i],
                     BUTTON_MAP[i].key);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(16));
    }

    vTaskDelete(NULL);
}

int p4_doom_get_key(int *pressed, unsigned char *key)
{
    key_event_t ev;
    if (xQueueReceive(s_keys, &ev, 0) != pdTRUE) {
        return 0;
    }
    *pressed = ev.pressed;
    *key = ev.key;
    return 1;
}

/* -------------------------------------------------------------------- run -- */

esp_err_t app_doom_run(void)
{
    app_doom_stage("run entered");

    /*
     * Hand the radio's memory to Doom.
     *
     * The first attempt at this crashed in the SDIO driver, not in Doom:
     *   assert failed: sdio_push_data_to_queue sdio_drv.c:701 (pkt_rxbuff)
     * Loading the WAD drained internal RAM until the transport to the C6 could
     * not allocate a receive buffer for an incoming frame, and the assert took
     * the whole board down. Nothing here needs the network.
     */
    app_ble_scan_stop();
    app_net_suspend();

    if (s_running) {
        app_doom_stage("already running");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_panel == NULL) {
        ESP_LOGE(TAG, "no panel handle");
        return ESP_ERR_INVALID_STATE;
    }
    if (!app_doom_available()) {
        ESP_LOGE(TAG, "doom1.wad not found in the storage partition");
        return ESP_ERR_NOT_FOUND;
    }

    void *fb = NULL;
    esp_err_t err = esp_lcd_dpi_panel_get_frame_buffer(s_panel, 1, &fb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "cannot get frame buffer: %s", esp_err_to_name(err));
        return err;
    }
    s_frame_buffer = fb;
    app_doom_stage("framebuffer ok");

    s_doom_rgb565 = heap_caps_malloc(DOOM_W * DOOM_H * 2,
                                     MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
    if (s_doom_rgb565 == NULL) {
        ESP_LOGE(TAG, "cannot allocate the staging buffer");
        return ESP_ERR_NO_MEM;
    }

    ppa_client_config_t ppa_cfg = {
        .oper_type = PPA_OPERATION_SRM,
    };
    err = ppa_register_client(&ppa_cfg, &s_ppa);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ppa register failed: %s", esp_err_to_name(err));
        heap_caps_free(s_doom_rgb565);
        s_doom_rgb565 = NULL;
        return err;
    }

    app_doom_stage("ppa ok");

    s_keys = xQueueCreate(32, sizeof(key_event_t));
    s_quit = false;
    s_running = true;
    s_exit_visible = false;
    s_drew_first_frame = false;

    app_doom_loading_begin();

    xTaskCreate(input_task, "doom_in", 3072, NULL, 5, NULL);

    ESP_LOGI(TAG, "starting doom, fb=%p staging=%p", s_frame_buffer, s_doom_rgb565);

    esp_err_t result = ESP_OK;
    if (setjmp(s_fatal_jmp) == 0) {
        s_fatal_ready = true;

        app_doom_stage("calling doomgeneric_Create");

        /*
         * -gfxmode rgb565 is not optional.
         *
         * Without it doomgeneric defaults to rgba8888 and writes 32 bits per
         * pixel, while the draw hook below copies W*H*2 bytes and hands the PPA
         * an RGB565 buffer - i.e. the top-left quarter of the frame
         * reinterpreted as 16-bit pixels, which renders as a flat pale blue.
         */
        char *argv[] = { "doom", "-iwad", WAD_PATH, "-gfxmode", "rgb565" };
        doomgeneric_Create(5, argv);
        app_doom_stage("wad loaded, looping");
        ESP_LOGI(TAG, "wad loaded, entering main loop");

        /*
         * Only now freeze LVGL. Doom's start-up draws nothing, so LVGL is left
         * running through it to animate the loading bar; from here on Doom owns
         * the frame buffer and the adapter must not flush over it.
         */
        app_doom_loading_end();
        if (!bsp_display_lock(2000)) {
            ESP_LOGW(TAG, "could not take the lvgl lock, running anyway");
        }

        while (!s_quit) {
            doomgeneric_Tick();
            /* Doom's loop never blocks, so without this it pins the core and
             * starves the idle task until the task watchdog starts printing
             * register dumps. Doom runs at 35 fps; a millisecond is free. */
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    } else {
        /* Arrived here from doom_platform_fatal. */
        ESP_LOGE(TAG, "doom aborted during start-up");
        result = ESP_FAIL;
    }
    s_fatal_ready = false;

    ESP_LOGI(TAG, "leaving doom");
    bsp_display_unlock();
    s_running = false;

    /* Give the input task a tick to notice s_quit and delete itself. */
    vTaskDelay(pdMS_TO_TICKS(50));

    ppa_unregister_client(s_ppa);
    s_ppa = NULL;
    heap_caps_free(s_doom_rgb565);
    s_doom_rgb565 = NULL;
    vQueueDelete(s_keys);
    s_keys = NULL;

    app_net_resume();

    app_doom_stage(result == ESP_OK ? "exited cleanly" : "exited with error");
    return result;
}
