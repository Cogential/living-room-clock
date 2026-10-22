# Living Room Clock

A wall clock for the **Waveshare ESP32-P4-WIFI6-Touch-LCD-7B** (1024x600 MIPI-DSI,
GT911 touch), built on ESP-IDF v5.5.5 + LVGL 9.5.

Thirteen clock faces (picked in Settings), weather, headlines, Hacker News and
market panels from a data gateway you run yourself, NTP time sync over the
board's onboard ESP32-C6 radio, and DOOM.

## Building

1. Install [ESP-IDF v5.5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32p4/get-started/)
   with the `esp32p4` target.
2. `cp main/secrets.h.example main/secrets.h` and fill it in: your data gateway's
   URL and token, and your location for the weather (see **Data gateway**). It is
   gitignored. Skip it and the clock still builds and keeps time, just without
   the feeds.
3. Optional: put `doom1.wad` in `spiffs/` for DOOM (see **DOOM**).
4. Build and flash:

```bash
. ~/esp/esp-idf/export.sh
./tools/devflash.sh          # build + app-flash once (prefers the CH34x port)
FLASH_ALL=1 ./tools/devflash.sh   # first time, or after changing spiffs/: also writes the storage partition
./tools/chunkflash.sh        # app only, after a build: fallback when a flash dies "chip stopped responding"
./tools/devlog.sh 25         # reset, then capture 25s of boot log
./tools/devmon.sh 60         # watch the console WITHOUT resetting
```

Long writes to this board can die partway with "chip stopped responding", more
often the bigger the image. `devflash.sh` makes **one** attempt on purpose: a
failed write leaves the chip wedged and every retry fails earlier than the last,
so power cycle the board before trying again. If it keeps failing,
`chunkflash.sh` writes the image in 512 KB pieces, retries any piece that fails
on its own, and verifies the whole thing by reading it back. It writes the app
only, from an existing build. See **Serial ports** below for which port to use.

## Setting it up

Everything is configured **on the device**: tap the gear in the top right.

- **Wi-Fi** - scans, lists networks strongest first, on-screen keyboard for the
  password. Credentials are stored in NVS.
- **Time zone** - picker of common zones.
- **Brightness / night dimming** - see below.
- **Bluetooth** - BLE scan and connect.

The Kconfig values under `idf.py menuconfig` -> "Living Room Clock" are only
first-boot defaults; once anything is saved on-device, NVS wins.

Networking is best-effort and never blocks the UI: with no credentials, a wrong
password or a dead router, the display still comes up and runs off the RTC. The
status bar shows the sync state, and a banner appears until the time is real.
The radio is started even with no credentials so the settings screen can scan.

## Using it

- **Faces change in Settings only.** Swiping wakes the chrome but does not cycle
  faces - on a wall clock a stray brush past the screen should not change it.
- Touching the screen shows only the **gear** and the network state. The face
  name is deliberately not displayed: it is chrome about the app, not about the
  time.
- Picking a face returns to the settings menu rather than sitting on the list.
- The status bar and dock **fade out after 8 seconds**; touch anywhere to bring
  them back.
- The **gear** in the status bar opens settings.
- The face you were last on is restored at boot.

## Settings

| Group | What it does |
|---|---|
| Clock face | Picks from the thirteen faces |
| Brightness | Backlight level, applied live as you drag |
| Night dimming | Drops to a lower brightness between two hours, wrapping past midnight |
| 24-hour time | Switches every face between 24h and 12h + AM/PM |
| Time zone | POSIX TZ, applied without a reboot |
| Sync time now | Forces an SNTP resync |
| Wi-Fi | Scan, pick, enter password, join |
| Bluetooth | BLE scan / connect / forget |
| System | IP, uptime, free PSRAM, firmware and IDF version |

## Bluetooth: BLE only, no speakers

The ESP32-P4 has no radio; Bluetooth comes from the ESP32-C6 over the hosted
SDIO link, and **the C6 is BLE-only with no Classic BR/EDR**. A2DP - what
speakers and headphones use - requires BR/EDR, so audio devices cannot pair with
this board at all. No firmware change alters that. The slave says so itself at
boot:

```
transport: Features supported are:
transport:   * WLAN
transport:     - HCI over SDIO
transport:     - BLE only
```

What BLE *is* good for here: room sensors (thermometers, humidity), buttons and
remotes. The Bluetooth page connects and remembers one peer, reconnecting at
boot. Reading characteristics off a specific sensor is not wired up yet - that
is the natural next step, as a GATT client keyed to whatever you attach.

For actual sound, the board has an **ES8311 codec and a PH2.0 speaker terminal**;
wire a passive speaker to it and drive it via `bsp_audio_codec_speaker_init()`.

## The faces

Thirteen of them, chosen in **Settings -> Clock face**. **Every face is the same
HUD** - clock top left, then a panel per feed - and only the styling changes, so
information is always in the same place. The clock area carries no face
switcher: nothing to fat-finger on a wall clock.

**Phosphor** is the default on a fresh install.

| Face | Look |
|---|---|
| **Dashboard** | Navy gradient, rounded glass panels, thin numerals. The plainest. |
| **Photo** | A forest at dawn behind dark glass panels; the clock sits on the picture with a per-glyph shadow. Use your own picture: see below. |
| **Minimal** | Warm paper, white panels, muted section colours. The one that reads in daylight. |
| **Cyberpunk** | Hazard yellow on black, magenta ghost behind the numerals, `//` headings, scanlines in the gaps. |
| **Synthwave** | Violet panels, a half-transparent banded sun sinking behind the clock onto a neon horizon. |
| **Phosphor** | Green phosphor, monochrome - sections told apart by `>` headings, not colour; amber only for a falling price. Blinking block cursor. |
| **Rapture** | Art deco under the sea: brass-edged square panels on oxidised teal, a stepped ziggurat above the clock. |
| **Harvest** | Wooden boards on a green field; the clock is carved into a sign. Dates read "Fall 28", not 28/09. |
| **Tavern** | Ink on parchment. Roman numeral hour leads the date, an ornate rule beneath, a wax seal counting seconds. |
| **Voxel** | Sky, blocky clouds, a strip of turf; stone panels with a hard offset shadow instead of a blur. |
| **Lab** | White signage plates on a seamed wall; two accent-coloured bars close on the middle as the minute runs out. |
| **Grove** | Gold on forest green, a small triangle motif over the clock. |
| **Cuckoo** | Carved case beside a left-aligned clock. Pendulum swings continuously; the bird comes out on the hour and at :30, and once when the face is opened. Change the `tm_min == 0 \|\| tm_min == 30` test in `face_cuckoo.c` to make it hourly only. |

### Your own picture on the Photo face

The picture is `main/assets/photo.rgb565`: 1024x600, raw RGB565, little-endian,
embedded in the app. The forest is drawn by `tools/gen_forest.py`. To use a
photo instead, convert it (Pillow) and rebuild:

```python
from PIL import Image
import struct
img = Image.open("mine.jpg").convert("RGB").resize((1024, 600))
with open("main/assets/photo.rgb565", "wb") as f:
    for r, g, b in img.getdata():
        f.write(struct.pack("<H", ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)))
```

Keep personal photos out of a public fork: the file is committed.

## The HUD

`ui/ui_hud.c` owns the layout every face shares:

```
    0..262   clock column (x 20..600)  time, date, seconds
                weather panel (x 616..1004)  temp, conditions, feels/humidity/
                                             wind/AQI, sunrise/sunset, 3-day
  278..440   news card  |  Hacker News card   headline wraps to three lines
  456..584   five market cells                 price, change in up/down colour
```

A face passes a `ui_hud_theme_t` - text colours, panel fill/border/radius/shadow,
one accent per section, clock family and colour, an optional ghost duplicate,
an optional date formatter, and a seconds style (bar, two closing bars, or none)
- and draws its own artwork: the background before `ui_hud_create()`, anything
that must sit over the panels after it. Free space for artwork is the clock
column around the digits (y ~26..178), the gutters, and anything behind the
translucent panels.

Headlines are **clipped, not wrapped** past three lines (`LV_LABEL_LONG_DOT` in a
fixed slot), and every value that could grow - prices, forecast, sources - is
clipped rather than allowed to wrap into its neighbour.

**Tapping a headline card shows its article as a QR code** (`ui/qr_sheet.c`),
so it can be read on a phone - the clock has no browser and cannot usefully
have one. The chrome layer sits above every face and takes all touches, so it
forwards short taps on empty background to the face's optional `tap(x, y)`;
faces set `.tap = ui_hud_tap`. The sheet closes on any tap or after a minute.

## Data gateway

The clock does not talk to weather, news or market services directly. It asks
one HTTPS server - your **data gateway** - for small, pre-digested JSON, so the
firmware never parses a 200 KB RSS feed or holds API keys for five services in
flash. You run the gateway; the clock only needs its address.

**Configure it** in `main/secrets.h` (copy `secrets.h.example`):

| Define | Meaning |
|---|---|
| `CLOCK_API_BASE` | Base URL, no trailing slash, e.g. `https://clock.example.net`. Leave it out and the panels say "No data gateway". |
| `CLOCK_API_TOKEN` | Sent as `Authorization: Bearer <token>`. Empty for an open gateway (keep it on your LAN or tailnet then). |
| `CLOCK_LATITUDE`, `CLOCK_LONGITUDE`, `CLOCK_PLACE` | Where the weather is for. First-boot defaults only; no place means no weather. |

The gateway must serve **HTTPS with a publicly trusted certificate** (the client
uses ESP-IDF's CA bundle; Let's Encrypt is fine, self-signed is not). Each body
must be under 64 KB, and links over 200 characters are dropped rather than
shortened into broken QR codes. The stock symbols are in `WATCHLIST` in
`main/net/app_api.c`.

### What it has to serve

Every endpoint is a `GET` returning JSON. Missing fields fall back to blanks; a
non-200 status is retried a minute later.

| Endpoint | Response |
|---|---|
| `/weather?lat=..&lon=..` | `{"temp":21,"feels":20,"humidity":64,"wind":12,"aqi":31,"hi":24,"lo":13,"desc":"Partly cloudy","forecast":[{"d":"Thu","hi":22,"lo":11}, ...3 days]}` - whole numbers in the units you want shown (the clock adds only `°` and `%`); `aqi` -1 if unknown. |
| `/space/sun?lat=..&lon=..` | `{"sunrise":"2026-10-22T11:32:00+00:00","sunset":"2026-10-22T22:18:00+00:00"}` - UTC ISO 8601; the clock converts to its time zone. |
| `/news` | `{"items":[{"title":"...","source":"BBC News","link":"https://..."}, ...]}` - up to 20. |
| `/hn/top` | `{"items":[{"title":"...","score":312,"comments":148,"url":"https://...","hn_url":"https://news.ycombinator.com/item?id=..."}, ...]}` - up to 20; `comments` null for job posts. |
| `/finance/quote?symbol=AAPL` | `{"symbol":"AAPL","price":231.4,"changePct":-0.82}` |
| `/finance/crypto?ids=bitcoin` | `{"prices":{"bitcoin":{"usd":67250.1,"usd_24h_change":1.9}}}` (CoinGecko's `simple/price` shape) |
| `/finance/fear-greed` | `{"score":62,"rating":"greed"}` |

Any endpoint can be left out: that panel just stays on "Waiting" / `--`.

### Where the data can come from

A gateway is a small program - a Python/Node server on a Raspberry Pi or NAS,
or a Cloudflare Worker - that calls these and reshapes the replies. Check each
provider's current terms and limits; free tiers change.

| Panel | No key needed | Free key |
|---|---|---|
| Weather, AQI | [Open-Meteo](https://open-meteo.com/) forecast + air-quality APIs (non-commercial use) | OpenWeatherMap |
| Sunrise/sunset | Open-Meteo `daily=sunrise,sunset`, or [sunrise-sunset.org](https://sunrise-sunset.org/api) | - |
| News | RSS feeds (BBC, CBC, NPR, ...), or [GDELT DOC 2.0](https://blog.gdeltproject.org/gdelt-doc-2-0-api-debuts/) (noisier, ~1 request per 5 s) | [Guardian Open Platform](https://open-platform.theguardian.com/) (non-commercial), [NewsData.io](https://newsdata.io/), GNews, NewsAPI.org (development only) |
| Hacker News | [Official HN API](https://github.com/HackerNews/API) or [Algolia HN Search](https://hn.algolia.com/api) (`/api/v1/search?tags=front_page`, one request) | - |
| Stocks | - | Finnhub, Alpha Vantage (tight daily limits) |
| Bitcoin | [CoinGecko](https://www.coingecko.com/en/api) `simple/price` | - |
| Fear & Greed | [alternative.me](https://alternative.me/crypto/fear-and-greed-index/) (crypto index, no key) | - |

Cache on the gateway side: the clock polls weather and markets every 10
minutes, news and HN every 15, sun every 6 hours, and a cache keeps you well
inside every free tier.

## Layout

```
main/
  main.c                 display bring-up, then hand off to the UI
  app_settings.c         NVS-backed settings + timezone table
  Kconfig.projbuild      first-boot defaults only
  net/app_net.c          Wi-Fi + scanning + SNTP, entirely non-blocking
  net/app_api.c          data gateway client: weather, news, HN, markets
  secrets.h.example      gateway URL/token and location; copy to secrets.h
  doom/app_doom.c        DOOM launcher, input and PPA blit
  ble/app_ble.c          NimBLE central: scan, connect, remember one peer
  ui/ui.c                screens, dock, status bar, gestures, tick loop
  ui/ui_hud.c            the layout every face shares
  ui/qr_sheet.c          headline -> QR code sheet
  ui/ui_theme.c          shared layer/gradient/glow helpers
  ui/ui_widgets.c        settings row/slider/switch building blocks
  ui/settings_ui.c       root settings page + timezone picker
  ui/settings_wifi_ui.c  network list and password keyboard
  ui/settings_ble_ui.c   BLE scan and pairing
  ui/ui_fonts.c          Tiny TTF font cache over the embedded Ubuntu faces
  ui/faces/*.c           one file per clock face
  assets/*.ttf           embedded Ubuntu Thin / Medium / Mono Bold
  assets/photo.rgb565    the Photo face's picture
components/doomgeneric/  the Doom engine (GPLv2) and its ESP32-P4 platform layer
spiffs/                  storage partition contents (doom1.wad); not committed
tools/
  devflash.sh            build + one app-flash attempt, resolving the port
  chunkflash.sh          flash in 512 KB pieces with read-back verify
  devlog.sh              reset, then capture the boot log
  devmon.sh              watch the console without resetting
  gen_forest.py          regenerates the Photo face's forest scene
```

## Serial ports

The board has two USB ports and they do different jobs:

- **CH34x (UART0) for flashing.** Large writes over the native USB-JTAG port die
  partway far more often; the CH34x wrote the 5 MB storage image first time.
  `devflash.sh` and `chunkflash.sh` prefer it when it is plugged in.
- **USB-JTAG for the console.** `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`, so
  `devlog.sh`/`devmon.sh` read logs there. The CH34x shows only the ROM
  bootloader's first lines.

Use one cable at a time where you can. Their `/dev/ttyACMx` numbers **swap
between plugs**, so always resolve through `/dev/serial/by-id/`
(`usb-1a86_*` for the CH34x, `usb-Espressif_USB_JTAG_*` for the chip). The helper
scripts do.

Modem control lines matter, and differently per port:

- **USB-JTAG**: RTS drives EN. Asserting it - which is what opening a port
  normally does - holds the chip in reset and the capture comes back silent.
  DTR must be asserted or the CDC console withholds output. Two "empty log"
  dead ends came from this.
- **CH34x**: DTR/RTS are wired to the auto-reset circuit, so merely opening it
  with a library that asserts them reboots the board into download mode. That
  looks exactly like a hung app.

Because the USB-JTAG device is part of the chip, it disappears and re-enumerates
across a reset; reopening must be retried. `tools/devlog.sh` does that.

## Touch orientation

`main.c` sets `rotation = ROTATE_180` with **no** touch mirroring, even though
Waveshare's own example pairs that rotation with `mirror_x = mirror_y = 1`. This
adapter version already maps touch through `rotation`, so the extra mirroring
applied a second 180 degree turn and every tap landed in the diagonally opposite
corner.

That failed quietly rather than obviously: the dock still "worked" - tapping
Aurora activated Analog - and the gear appeared dead only because its mirror
image is empty background. If you change `rotation`, re-check the touch flags
with it, and use the overlay below rather than reasoning about it.

```bash
rm -rf build
idf.py -DCMAKE_C_FLAGS="-DCLOCK_TOUCH_DEBUG" build flash
```

Puts a yellow readout on the left edge showing the last touch coordinate and
which handler claimed it (`(background)`, `-> GEAR`, `-> DOCK n`). Reading it
off the panel beats trying to catch a serial capture at the moment of a tap.

### Adding a face

1. Copy a file in `ui/faces/`: define a `ui_hud_theme_t`, draw any background
   art in `create()` around `ui_hud_create()`, and export a `const clock_face_t`
   with `.tick = ui_hud_tick` (or your own that calls it) and `.tap = ui_hud_tap`.
2. Declare it in `ui/ui.h` and add it to `s_faces[]` in `ui/ui.c`.
3. Add the source to `main/CMakeLists.txt`.

The settings picker sizes itself from `FACE_COUNT`, so there is nothing else to
widen. To change which face a fresh install starts on, edit `DEFAULT_FACE_NAME`
in `ui.c` - it is resolved by name, so reordering the list cannot silently
change the default.

Animated faces should drive from the `clock_time_t` passed to `tick()`, or from
`lv_tick_elaps()` against a stored start time, rather than `lv_anim`. Cuckoo does
the latter: the whole door-and-bird sequence is a pure function of elapsed time,
so it cannot wedge half-open if a frame is dropped, and switching face mid-call
leaves nothing behind. Anything that moves every frame must also keep a tight
bounding box - see the pendulum, and the note about the analog hands below.

Faces keep their widgets in module statics because only one is ever alive at a
time. `tick()` is called ~30x/second; do the cheap thing and only touch widgets
when the value they show actually changed (see the `s_last_minute` guards).

**Use the sizes in `ui_fonts.h`** (`FACE_THIN_XL`, `FACE_MONO_XL`, ...). Every
distinct pixel size is a separate font instance with two caches of its own, so
inventing sizes per face is how a dozen faces would quietly exhaust internal
RAM. The whole app currently needs eight font instances regardless of face count.

`ui_theme.h` has the shared decoration: gradients, glows, scanlines, corner
brackets, plain rects for pixel/deco work. Prefer those over new primitives, and
remember the shadow rule under **Performance notes**.

## Type

Big numerals are rendered at arbitrary pixel sizes from embedded TTFs via LVGL's
Tiny TTF, rather than pre-baked bitmap fonts. That is what keeps a 250 px clock
crisp. `ui_font(family, size)` caches instances, so asking twice is free.

## Icons

`LV_SYMBOL_*` glyphs live at private-use codepoints (`LV_SYMBOL_SETTINGS` is
`0xF013`) that only LVGL's **built-in** fonts carry. The Ubuntu faces have
nothing there, so a symbol drawn with `ui_font()` renders as blank - which is how
the settings gear shipped invisible for a while.

`ui_symbol_font()` snaps to the nearest compiled-in Montserrat. Every `ui_font()`
instance now also carries that Montserrat as its **fallback**, so a symbol inside
Ubuntu text renders instead of blanking. That is what fixes `lv_keyboard`, whose
backspace, enter, arrow, OK and close keys share one font with the letters and
showed as empty boxes in the Wi-Fi password sheet.

A label that is *only* a symbol still uses `ui_symbol_font()` directly, so the
icon sits on the Montserrat baseline rather than borrowing Ubuntu's metrics.
Where a symbol would otherwise be mixed into a sentence, the symbol is dropped
and the text carries the meaning instead.

## Performance notes

This panel is 1024x600 rendered in software, and it is easy to fall off a cliff.
Measured with the soak harness below:

- **Never use large blur radii.** A 620 px glow circle with a 310 px box-shadow
  took the whole UI from ~28 fps to **under 1 fps** - LVGL's software shadow is
  recomputed every refresh and scales with blur radius over area. `ui_add_glow()`
  stacks translucent discs instead. Shadows under ~20 px are fine.
- **Size objects to what they actually cover.** The analog hands were originally
  full-screen `lv_line` objects, so every second invalidated all 1024x600 and
  halved the frame rate. They now track a tight bounding box.
- **Don't crossfade full screens.** Animating `opa` on a full-screen object forces
  an alpha layer every frame: it cost a fifth of the frame budget and stalled
  ~600 ms mid-animation. Faces are separate screens moved with
  `lv_screen_load_anim()`, which only repositions - and the slide direction
  matches the swipe.

Current numbers: 106-117 ticks per 4 s window against a 120 cap (~27 fps), worst
frame gap ~150 ms during a transition, PSRAM flat across face cycles.

## Settings storage

Settings live in one versioned blob in NVS (`app_settings.c`). Two things there
are worth knowing.

**NVS must be initialised before anything opens it.** `nvs_flash_init()` used to
live in `app_net_start()`, which runs *after* `settings_init()`. Every boot the
read failed and defaults were used, while saves - happening later, once NVS was
up - succeeded. Wi-Fi credentials therefore looked like they were erased on every
reboot when they were actually sitting in flash unread. It now runs first, in
`settings_init()`.

**Bump `SETTINGS_VERSION` when the meaning of a field changes, not just its
layout.** The face used to be stored as an index into the face list; adding and
reordering faces made a stored index point at a different face, which is how a
saved setting quietly resolved to the wrong one. It is stored by name now, and
stale blobs are discarded rather than reinterpreted.

A stored face that no longer exists - because it was removed from the list -
falls back to `DEFAULT_FACE_NAME`, not to index 0. Deleting a face is therefore
safe without another version bump.

## Controllers

The USB-A port is a real host (`bsp_usb_host_start()`), and a **DualSense
plugged in by cable** enumerates as an ordinary HID gamepad. Settings ->
Controller shows sticks, triggers, buttons and d-pad live, plus the raw report
bytes - the decode offsets came from documentation rather than from measuring
this device, so the raw view is what settles any disagreement.

**Wireless is not possible.** The DualSense speaks Bluetooth Classic (BR/EDR)
and the C6 is BLE-only, the same wall that rules out Bluetooth speakers. An
Xbox Series controller uses BLE HID and is the candidate worth trying if you
want wireless, though that is untested here.

If the board browns out when a controller is plugged in, power it from
something stronger than a laptop USB port: the host port draws from the board's
own supply.

## DOOM

Settings -> DOOM launches it full screen; **PS + Create** quits back to the
clock. The engine is [doomgeneric](https://github.com/ozkl/doomgeneric) via the
[ESP32-P4 port](https://github.com/alexkid77/ESP32P4DOOM). **Doom's source is
GPLv2**, which is why this whole project is (see **License**).

**The game data is not included.** To add it:

1. Get the shareware `doom1.wad` (Doom episode 1, about 4 MB). id Software
   released it for free distribution, and it is easy to find, e.g. in the
   [Doom Wiki's shareware article](https://doomwiki.org/wiki/DOOM1.WAD) or in
   the Debian/Ubuntu `doom-wad-shareware` package
   (`/usr/share/games/doom/doom1.wad`). If you own the full game, `doom.wad`
   from it is not supported here: the storage partition is sized for the
   shareware file.
2. Copy it to `spiffs/doom1.wad` (lower case).
3. Flash the storage partition once: `FLASH_ALL=1 ./tools/devflash.sh`. Later
   app-only flashes leave it alone.

Without it the DOOM button just logs "doom1.wad missing" and the clock carries on.

Doom renders 320x200 in RGB565; the **PPA** scales it to 1024x600 straight into
the MIPI-DSI frame buffer. Start-up takes about ten seconds, so the stage
breadcrumbs below double as a loading bar.

It cannot be a clock face: it owns the panel. It runs on its own task, and LVGL
is frozen only once start-up is done - during loading LVGL still runs so the
progress screen can animate.

### Things that were not obvious

Every one of these presented as something other than what it was.

- **`-gfxmode rgb565` is mandatory.** doomgeneric defaults to `rgba8888`, so
  without it Doom writes 32 bits per pixel while the draw hook copies `W*H*2`
  bytes as RGB565 - the top-left quarter of the frame, which renders as a flat
  pale blue. Copying the reference port's draw code is not enough; its argv
  matters too.
- **`SPIRAM_MALLOC_ALWAYSINTERNAL` must be 0.** At 64, allocations under that
  size were forced into internal RAM. R_Init makes thousands composing
  textures: free internal RAM fell to ~19 KB, then each further allocation
  scanned a nearly-full fragmented heap and R_Init never finished. Earlier the
  same pressure starved the SDIO driver into `assert failed:
  sdio_rx_get_buffer`. At 0 the low-water mark through a whole session is
  ~100 KB.
- **The main loop must yield.** `doomgeneric_Tick()` never blocks, so without a
  `vTaskDelay(1)` it pins the core, starves the idle task and the task watchdog
  prints register dumps forever. That looks like a crash loop and is not one.
- **The PPA has to rotate 180.** The panel is mounted upside down and LVGL
  compensates when rendering; Doom bypasses LVGL entirely and must turn itself.
- **Suspend the radio.** Wi-Fi holds DMA-capable internal buffers and every
  broadcast frame costs the SDIO transport another. `app_doom_run()` stops
  Wi-Fi and BLE scanning for the session. The clock does not sync time while
  you play.
- **Without a controller, tap the screen and an X appears top-right.** LVGL is
  frozen while Doom owns the panel, so its input device never runs: the touch
  controller is read directly and the button is drawn into the frame buffer
  after each game frame. Touch and frame buffer coordinates share a frame here
  (the panel is upside down, and LVGL rotates both its rendering and its touch
  to compensate, while Doom rotates its own output in the PPA), so one rectangle
  serves for drawing and hit-testing. If a tap ever lands in the opposite
  corner, that shared assumption is what to revisit.
- **The task stack is 16 KB, not 32.** Task stacks come from internal RAM and
  32 KB could not be allocated once the UI, Wi-Fi, BLE and USB host were up;
  `xTaskCreate` returned pdFAIL and the button silently did nothing. The
  reference runs Doom on the main task with 8 KB.
- **Doom's `.bss` lives in PSRAM.** ~240 KB of it. See
  `components/doomgeneric/linker.lf` - the stock `extram_bss` scheme does *not*
  cover ordinary `.bss`, so that fragment defines its own.
- **The panel handle.** `bsp_display_start_with_config()` creates the panel in a
  local and returns only the `lv_display_t`, with no getter, so it is
  unrecoverable afterwards. `main.c` repeats the BSP's calls instead - re-check
  `display_start()` against the BSP if you update it.

Sound is not wired up - `FEATURE_SOUND` is off and the ES8311 path is the next
step.

### Debugging something you cannot watch

Nobody is looking at the console at the moment a button is tapped, and a crash
reboots the board before anything can be read. Two facilities exist for that,
and both earned their place:

**Core dumps.** `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH` with a `coredump`
partition; read a panic backtrace after the fact with:

```bash
idf.py -p /dev/serial/by-id/usb-1a86_* coredump-info
```

This is what showed that a crash blamed on Doom's WAD loading was really the
SDIO driver asserting. Reach for it as soon as something reboots - inferring
from symptoms cost several rounds first.

**Stage hooks.** `app_doom_stage()` is called at each start-up stage from
`d_main.c` and drives the loading bar. It began as an NVS breadcrumb for
diagnosing launches nobody could watch; that persistence is gone now the thing
works, but the hooks stayed because the loading bar needs them. If a launch
starts failing again, writing the stage to NVS is a three-line change.

`I_Error` is still intercepted via `doom_platform_fatal()`, which is not
logging but correctness: with `ORIGCODE` off, `I_Error` prints and then
*returns*, leaving the caller to continue in a broken state and fall over
somewhere unrelated. It longjmps back to the launcher instead.

## Memory notes

**Internal RAM is the scarce resource, not PSRAM.** `SPIRAM_MALLOC_ALWAYSINTERNAL`
defaults to 16 KB, which means every LVGL object, style and font-cache entry -
all comfortably under that - stays in internal RAM. Walking the settings pages
drove free internal memory from ~98 KB to ~15 KB, and then the SDIO driver that
carries both Wi-Fi and BLE asserted:

```
assert failed: sdio_rx_get_buffer sdio_drv.c:670 (*buf)
```

Lowering that threshold moves the traffic to PSRAM. 256 was enough until USB
host was added, at which point BLE could no longer allocate an HCI command
buffer and scanning failed outright:

```
E vhci_drv: Tx ble_transport_to_ll_cmd_impl: malloc failed
E ble: scan start failed (17)
```

At 64 bytes the plateau went from 35-70 KB back up to 73-101 KB and the failure
went away. It is now **0** (`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=0`), which Doom's
texture setup needed - see **DOOM**. The measured frame rate did not change at either setting. Anything
that genuinely needs internal or DMA memory asks for it by capability and is
unaffected.

**Internal RAM is the budget to watch when adding anything.** Wi-Fi, BLE and USB
host all want DMA-capable internal memory, and they are additive: USB host alone
cost about 20 KB. Run the settings soak after adding a subsystem and watch the
`internal=` figure - if the plateau drops near 40 KB, something will fail to
allocate before long. Flash and PSRAM are nowhere near their limits by
comparison.

Each Tiny TTF font instance also allocates *two* LRU caches of
`LV_TINY_TTF_CACHE_GLYPH_CNT` entries, so every distinct pixel size costs real
memory. Chrome and settings therefore stick to the three sizes in `ui_fonts.h`
(`UI_TEXT_SM/MD/LG`); only the faces ask for arbitrary sizes, and those get a
smaller cache since they only ever render digits.

### Soak harnesses

```bash
idf.py -DCMAKE_C_FLAGS="-DCLOCK_FACE_SOAK_TEST" build flash   # cycles faces
idf.py -DCMAKE_C_FLAGS="-DCLOCK_SETTINGS_SOAK"  build flash   # walks settings
```

The face soak cycles every face every 4 s and logs free PSRAM, ticks served and
the worst inter-tick gap. `ticks` is the real signal: 120 means the UI is keeping
up, anything near 1 means a refresh is taking seconds.

The settings soak walks the whole settings tree - root, Wi-Fi, Bluetooth,
timezone and back - logging free internal memory at each step. Every settings
page is built in code and destroyed by LVGL's screen loader, so a missing null
check, a timer outliving its page, or memory pressure only appears when the page
is actually constructed. Watch `internal=`; it should plateau well above 60 KB.

Note that `-DCMAKE_C_FLAGS` is cached by CMake - `rm -rf build` when switching
between harnesses or back to a normal build.

## Hardware notes

- This board carries **pre-v3 P4 silicon** (reported v1.3). Waveshare's stock
  `sdkconfig.defaults` targets post-v3 and the bootloader will refuse to run, so
  `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` and `CONFIG_ESP32P4_REV_MIN_100=y` are set.
- Wi-Fi is not on the P4. It is proxied to the onboard ESP32-C6 over SDIO via
  `esp_wifi_remote` + `esp_hosted`, which needs the C6 running matching slave
  firmware (as shipped from Waveshare).
- `W (…) ledc: GPIO 32 is not usable` at boot comes from the BSP's backlight
  setup and is harmless - the backlight is driven anyway.

## License

GPL-2.0 (see `LICENSE`). The firmware links the Doom engine
(`components/doomgeneric`, GPLv2), so the combined work is GPLv2.

- The Ubuntu fonts in `main/assets/` are under the Ubuntu Font Licence 1.0
  (`main/assets/UBUNTU-FONT-LICENCE.txt`).
- `doom1.wad` is not included; see **DOOM**.
- DOOM is a trademark of id Software; this project is not affiliated with it.
