# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

ESP-IDF v5.5.5 + LVGL 9.5 firmware for a wall clock on the **Waveshare ESP32-P4-WIFI6-Touch-LCD-7B**
(1024x600 MIPI-DSI, GT911 touch, pre-v3 P4 silicon, Wi-Fi/BLE via the onboard ESP32-C6 over SDIO).
`README.md` is detailed and is the source of truth for the reasons behind most non-obvious choices; read the relevant section
before changing display, memory, flashing or Doom code. This file is the short map.

## Commands

```bash
. ~/esp/esp-idf/export.sh            # required in every shell (also provides pyserial for the scripts)
./tools/devflash.sh                  # build + app-flash once (prefers the CH34x port); FLASH_ALL=1 also writes SPIFFS/WAD
./tools/chunkflash.sh                # fallback when a flash dies "chip stopped responding": 512 KB chunks, read-back verify
./tools/devlog.sh 25 [pattern]       # reset, capture 25 s of boot log
./tools/devmon.sh 60 [pattern]       # watch console WITHOUT resetting
idf.py -p /dev/serial/by-id/usb-1a86_* coredump-info   # backtrace of the last panic (do this first after any reboot)
```

- There are no unit tests or linters. Verification is on hardware via compile-time harnesses in `main/ui/ui.c`:
  `idf.py -DCMAKE_C_FLAGS="-DCLOCK_FACE_SOAK_TEST" build flash` (fps/PSRAM per face),
  `-DCLOCK_SETTINGS_SOAK` (walks settings, watch `internal=`), `-DCLOCK_TOUCH_DEBUG` (on-screen touch readout).
  `CMAKE_C_FLAGS` is cached: `rm -rf build` when switching harnesses or back to a normal build.
- A failed flash leaves the chip wedged; retries fail earlier each time. Ask the user to **power cycle** before retrying
  rather than looping. Use one cable at a time; always resolve ports via `/dev/serial/by-id/` (ttyACM numbers swap).
- `main/secrets.h` (gitignored, copy from `secrets.h.example`) holds `CLOCK_API_BASE`, `CLOCK_API_TOKEN` and the default
  location (`CLOCK_LATITUDE/LONGITUDE/PLACE`). Never commit it or move any of it into tracked files (`sdkconfig.defaults`,
  Kconfig, code). Without a base URL the feeds are disabled; without a place, weather is. The README's "Data gateway"
  section is the gateway's contract: update it whenever an endpoint or JSON field changes.
- `sdkconfig`, `build/`, `managed_components/` and `dependencies.lock` are untracked; persistent config changes go in
  `sdkconfig.defaults` (delete `sdkconfig` to regenerate). Component versions are pinned in `main/idf_component.yml`.

## Architecture

- `main/main.c`: display bring-up. It duplicates the BSP's `display_start()` steps instead of calling
  `bsp_display_start_with_config()` so it keeps the `esp_lcd` panel handle Doom needs; re-check it if the BSP is bumped.
  Rotation is 180 with no touch mirroring (mirroring double-rotates touch).
- `app_settings.c`: one versioned NVS blob; `settings_init()` also does `nvs_flash_init()` and must run first. Bump
  `SETTINGS_VERSION` when a field's *meaning* changes. Kconfig ("Living Room Clock") values are first-boot defaults only.
- Background services, all non-blocking so the UI never stalls: `net/app_net.c` (Wi-Fi, scan, SNTP), `net/app_api.c`
  (task fetching weather/news/HN/markets from `CLOCK_API_BASE` into fixed-size structs the UI polls),
  `ble/app_ble.c` (NimBLE central, one remembered peer; C6 is BLE-only, so no audio/Classic devices),
  `input/app_pad.c` (USB HID host, wired DualSense).
- `ui/ui.c`: screens, status bar/dock chrome (fades after 8 s), gestures, the ~30 Hz tick loop, and the face registry
  `s_faces[]`. The chrome layer takes all touches and forwards short background taps to the face's `tap(x, y)`.
- `ui/ui_hud.c`: the single layout every face shares (clock column, weather, news/HN cards with QR-on-tap via
  `qr_sheet.c`, market cells). A face file in `ui/faces/` is mostly a `ui_hud_theme_t` plus background/overlay art, and
  exports a `clock_face_t` with `.tick = ui_hud_tick`, `.tap = ui_hud_tap`.
  Adding a face: new file, declare in `ui/ui.h`, add to `s_faces[]`, add to `main/CMakeLists.txt` `SRCS`.
  Faces are stored/defaulted by **name** (`DEFAULT_FACE_NAME` = "Phosphor"), never by index. `face_analog.c` is in the
  tree but not built or registered.
- `ui/ui_fonts.c`: Tiny TTF over embedded Ubuntu TTFs, cached per (family, size); every instance has a Montserrat fallback
  so `LV_SYMBOL_*` glyphs render. Symbol-only labels use `ui_symbol_font()`.
- `doom/app_doom.c` + `components/doomgeneric` (GPLv2; `doomgeneric_esp32p4.c` is the platform layer, `linker.lf` puts
  Doom's `.bss` in PSRAM): runs on its own 16 KB task, freezes LVGL after load, suspends Wi-Fi/BLE scanning, renders
  320x200 RGB565 and scales/rotates via the PPA straight into the DSI frame buffer. WAD is `spiffs/doom1.wad` (gitignored, user-supplied)
  in the `storage` partition (`partitions.csv`, which also has a `coredump` partition).

## Constraints that bite

- **Internal RAM is the budget**, not PSRAM or flash. Wi-Fi, BLE and USB host all need DMA-capable internal memory.
  `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=0` in `sdkconfig.defaults` is current (README's Memory notes section still mentions
  the older 64-byte setting). After adding a subsystem, run the settings soak and keep the `internal=` plateau well above
  60 KB.
- Use only the font sizes defined in `ui_fonts.h` (`UI_TEXT_SM/MD/LG`, `FACE_*`); each new size is a new font instance
  with two glyph caches in internal RAM.
- Software rendering at 1024x600: no large blur/shadow radii (use `ui_add_glow()`), keep animated objects' bounding
  boxes tight, no full-screen opacity animations. In `tick()`, only touch widgets when the shown value changes; drive
  animations from the passed time or `lv_tick_elaps()`, not `lv_anim`.

## Rules from the 2026-10-21 review fixes (keep these true)

- `ui_open_app(fn)` hides the chrome and stops the tick: if `fn` can fail, it must call `ui_close_app()` itself
  (see `launch_doom_from_dock`). `games_ui_launch_doom()` returns false on failure.
- `app_doom_run()`: precondition checks come before `app_net_suspend()`; every later exit path calls `app_net_resume()`,
  `app_doom_loading_end()`, and only unlocks LVGL if `s_lvgl_locked`. Keep `setjmp` in `run_until_quit()` (no locals
  across longjmp). Delete `s_keys` only after `input_task` has cleared `s_input_running`.
- Online means `APP_NET_CONNECTED || APP_NET_SYNCED`; never `< / >=` comparisons on `app_net_state_t` (FAILED sorts last).
- `s_generation` only moves after a successful fetch. `api_get()` logs and drops bodies over `RESPONSE_MAX` (64 KB).
- Face taps from the top layer are ignored after a gesture or >24 px of travel (LVGL sends SHORT_CLICKED after swipes).
- Empty headline cards never "hold".
- Toolchain: since the Ubuntu 24.04 upgrade, ESP-IDF uses the `idf5.5_py3.12_env` (recreated 2026-10-21 with
  `~/esp/esp-idf/install.sh esp32p4`); the old build dir needed `idf.py fullclean`.

## Public repo rules (since 2026-10-22)

The repo (github.com/Cogential/living-room-clock) was cleaned to be made public: history was rewritten with
git-filter-repo to drop the user's personal photo, `doom1.wad`, the gateway domain and the home location. Keep it that way:
- Never commit personal data: photos (`main/assets/photo.rgb565` is the generated forest from `tools/gen_forest.py`),
  the gateway URL/token, home coordinates or place names, Wi-Fi details. They belong in `main/secrets.h`.
- Never commit game data (`spiffs/` and `*.wad` are gitignored). Keep the user's local `spiffs/doom1.wad`.
- Face names stay generic (no game/franchise names; "Rapture" is OK by the user). Renamed 2026-10-22: Pip-Boy->Phosphor,
  Hyrule->Grove, Aperture->Lab, Stardew->Harvest, Blockcraft->Voxel, Outrun->Synthwave.
- The food/barcode feature was removed at the user's request (2026-10-22); don't bring it back.
- License is GPL-2.0 (doomgeneric). Fonts carry `main/assets/UBUNTU-FONT-LICENCE.txt`.
- Pre-cleanup history backup (private, never push it): `/data/cogent/backups/clock-pre-public-2026-10-22/`.
