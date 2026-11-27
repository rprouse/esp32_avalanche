# AGENTS.md - esp32_avalanche

Guidance for AI agents (and humans) working on this repo.

## Project goal

A wall/desk display that downloads the **Avalanche Canada** forecast for a local region
once a day and shows it on a 7" touch screen. Touch (tap tabs / swipe) switches between
screens: danger ratings, avalanche problems, details, etc.

Current state: working end to end. Connects to Wi-Fi, syncs time via NTP, downloads the
Avalanche Canada forecast for the configured point (default Fernie, BC) at boot and then
daily, and shows it in a tabbed LVGL UI (Danger, Problems, Conditions, Details, Weather).
The Weather tab uses OpenWeatherMap (optional API key in `config.h`).

## Local configuration (`include/config.h`, git-ignored)

Copy `include/config.sample.h` to `include/config.h` and fill it in. It holds the
(optional) Wi-Fi SSID/password, forecast latitude/longitude, display name, language, POSIX time zone,
NTP servers and refresh schedule. **Never commit `config.h`** and never print the
Wi-Fi password in logs or messages. `src/app_config.h` includes it, fails the build
with a clear error if it's missing, and supplies defaults for optional settings. When
adding a new setting, add it to `config.sample.h` **and** give it a default in
`app_config.h` so existing `config.h` files keep building.

## Hardware: Waveshare ESP32-S3-Touch-LCD-7 (touch version, SKU 27078, board rev V1.2)

> Not to be confused with the "Cheap Yellow Display" (ESP32-2432S028R). Nothing from CYD
> configs (TFT_eSPI, ILI9341, XPT2046, SPI pins) applies to this board.

| Item | Detail |
|---|---|
| Module | ESP32-S3-WROOM-1-**N16R8**: dual-core LX7 @ 240 MHz, 512 KB SRAM |
| Flash | 16 MB, QIO |
| PSRAM | 8 MB, **octal (OPI)** -> `memory_type = qio_opi` |
| LCD | 7" IPS 800x480, **ST7262** driver, **16-bit parallel RGB565** (no SPI command interface) |
| Touch | **GT911** capacitive, 5-point, I2C, factory calibrated (no runtime calibration needed) |
| IO expander | **CH422G** at I2C 0x20 (shares bus with touch) |
| Wireless | 2.4 GHz Wi-Fi b/g/n, BLE 5 |
| Power | 5 V USB-C (~450 mA), 3.7 V Li-ion on PH2.0 header with charger (CS8501) |
| Other | TF card (SPI), RS485, CAN (TJA1051), I2C header, analog sensor header, UART header |

### GPIO map

| Function | GPIO |
|---|---|
| LCD data R3..R7 | 1, 2, 42, 41, 40 |
| LCD data G2..G7 | 39, 0, 45, 48, 47, 21 |
| LCD data B3..B7 | 14, 38, 18, 17, 10 |
| LCD HSYNC / VSYNC / DE / PCLK | 46 / 3 / 5 / 7 |
| I2C SDA / SCL (touch + CH422G + header) | 8 / 9 |
| Touch INT | 4 |
| TF card MOSI / SCK / MISO | 11 / 12 / 13 (CS on EXIO4) |
| RS485 TX / RX | 15 / 16 |
| CAN TX / RX **or** native USB D+ / D- | 20 / 19 (muxed, see EXIO5) |
| UART0 TX / RX (USB-UART port) | 43 / 44 |
| Sensor AD | 6 |

LCD data-pin order as the ESP32 RGB peripheral sees it (DATA0..15):
`14, 38, 18, 17, 10, 39, 0, 45, 48, 47, 21, 1, 2, 42, 41, 40`.

### CH422G expander pins (EXIO)

| EXIO | Function |
|---|---|
| 1 | Touch (GT911) reset |
| 2 | LCD backlight enable (on/off only, **no PWM dimming** in hardware) |
| 3 | LCD reset |
| 4 | TF card CS (active low) |
| 5 | USB_SEL: low = native USB on GPIO19/20, high = CAN |
| 6 | LCD VDD enable |

The library calls `enableAllIO_Output()` on the CH422G at startup, then sequences LCD/touch
resets through it (see `include/esp_panel_board_custom_conf.h`).

### USB ports

- **"UART1" USB-C** (CH343P USB-UART -> UART0, GPIO43/44): **use this one** for upload and
  serial monitor (shows as "USB-Enhanced-SERIAL CH343", e.g. COM5). Auto-reset/boot via
  DTR/RTS works. The UART1/UART2 slide switch (SW1) **must be on UART1**. In the UART2
  position the CH343 goes to the UART2 header instead, and esptool fails with
  "No serial data received" (this board arrived set to UART2).
- **"USB" USB-C** (native ESP32-S3 USB): routed through a mux controlled by EXIO5 and shared
  with CAN. Not used. If you ever switch to it, set `-DARDUINO_USB_CDC_ON_BOOT=1` and make
  sure EXIO5 is held low.

### Manufacturer resources

- Docs: https://docs.waveshare.com/ESP32-S3-Touch-LCD-7 (Arduino page: `/Arduino`, FAQ: `/FAQ`)
- Product: https://www.waveshare.com/esp32-s3-touch-lcd-7.htm
- Local copy of the vendor repo: `D:\src\Electronics\ESP32-S3-Touch-LCD-7`
  - `hardware/schematics/ESP32-S3-Touch-LCD-7-Sch.pdf` - schematic (V1.2)
  - `examples/Arduino/examples/10_lvgl_v9_demo` - reference LVGL v9 port (our display port is derived from it)
  - `examples/Arduino/examples/08_DrawColorBar` - minimal panel test without LVGL
  - `firmware/*.bin` - prebuilt test images (flash at 0x0 with esptool) for checking hardware

## Toolchain / build

- **PlatformIO via the pioarduino fork** (VS Code extension `pioarduino.pioarduino-ide`).
- PlatformIO core lives at `D:\packages\platformio` (`PLATFORMIO_CORE_DIR`). The CLI is
  `D:\packages\platformio\penv\Scripts\pio.exe`. It is not on PATH.
- **Run pio from PowerShell or cmd, not Git Bash/MSYS.** pioarduino's `idf_tools.py` refuses
  to run under MSYS ("MSys/Mingw is not supported") and the toolchain won't install.
- Platform pinned to `55.03.313` = Arduino-ESP32 **3.3.13** / ESP-IDF **5.5.6**. The newer
  `61.x` line (Arduino 4 / IDF 6.1) is **not** verified: IDF 6 removes the legacy I2C driver
  that ESP32_IO_Expander/ESP32_Display_Panel use. Test before upgrading.
- Board JSON: `esp32-s3-devkitc1-n16r8` (matches N16R8). No custom board file is needed.
- Commands:
  ```powershell
  $env:PLATFORMIO_CORE_DIR='D:\packages\platformio'
  $pio='D:\packages\platformio\penv\Scripts\pio.exe'
  & $pio run                    # build
  & $pio run -t upload          # flash (UART1 port)
  & $pio device monitor         # serial @115200
  & $pio run -t upload -t monitor
  & $pio run -t upload --upload-port backcountry.local   # over Wi-Fi (needs OTA_PASSWORD)
  ```
- First build downloads ~1-2 GB of toolchain; later incremental builds take ~30 s.

### Libraries (pinned in platformio.ini)

| Library | Version | Why |
|---|---|---|
| ESP32_Display_Panel | v1.0.4 (git tag) | Espressif's driver for the RGB bus, ST7262, GT911, CH422G, backlight |
| ESP32_IO_Expander | v1.1.1 (git tag) | CH422G driver (dependency of the above) |
| esp-lib-utils | v0.2.3 (git tag) | Required by Display_Panel 1.0.4 (needs >=0.2.0 <0.3.0) |
| lvgl/lvgl | 9.5.0 | UI. Same version Waveshare tested with this board |

The Espressif libs are **not** in the PlatformIO registry, so they're pulled from GitHub by tag.

### Config files

- `include/esp_panel_board_custom_conf.h` - board definition for ESP32_Display_Panel (pins,
  RGB timings, touch/expander init sequences). Copied from Waveshare's v9 demo. Found by the
  library through `-I include`. **Edit RGB timings here** (PCLK is 16 MHz; lower it if you
  see drift/flicker).
- `include/lv_conf.h` - minimal LVGL config (overrides only; defaults come from
  `lv_conf_internal.h`). Add fonts/widgets here.

## Code layout

```
src/
  main.cpp                    setup(): display::begin() -> ui::build(placeholder) -> updater::start()
  app_config.h                includes include/config.h + defaults
  app/updater.*               Task on core 0: Wi-Fi, NTP, fetch schedule, retries, pushes to UI
  app/screen_schedule.*       Backlight off overnight (lv_timer), touch to wake
  app/wifi_setup.*            Wi-Fi setup mode: soft AP, DNS catch-all, captive web page, connect test
  app/ota.*                   Over-the-air firmware updates (ArduinoOTA), progress overlay
  net/wifi_creds.*            Wi-Fi credentials in NVS (falls back to config.h), setup-mode flag
  display/display_port.*      Board + LVGL glue: init, flush, touch read, LVGL task, mutex
  ui/ui.*                     All LVGL screens (header with refresh button, tabview)
  ui/setup_screen.*           Full-screen Wi-Fi setup page (QR codes + status)
  forecast/forecast.*         Forecast data model, danger-scale names/colours, placeholder
  forecast/forecast_client.*  HTTPS download, JSON -> Forecast, HTML -> plain text
  weather/weather.h           Weather model + icon/moon helpers
  weather/weather_client.cpp  OpenWeatherMap current + 5 day/3 h forecast -> daily rows; icon mapping
  weather/moon_phase.cpp      Moon phase (ported from the CYD weather station / Bodmer)
  weather/weather_icons.*     GENERATED LVGL images (git-ignored), see below
  net/root_ca.*               Pinned root CA bundle for all HTTPS hosts
  storage/cache.*             LittleFS cache of the last API responses (survives power loss)
assets/weather-icons/         Source BMP icons (Bodmer, FreeBSD license)
tools/convert_icons.py        BMP -> LVGL RGB565A8 converter (brightness -> alpha)
tools/pio_generate_icons.py   PlatformIO pre-build hook that runs the converter when stale
tools/make_logo.py            SVG (assets/icons/terrain.svg) -> A8 logo (src/ui/logo_icon.c)
tools/pio_ota.py              PlatformIO pre-script: espota + password from config.h for network upload ports
include/
  lv_conf.h, esp_panel_board_custom_conf.h
  config.sample.h             template for config.h (committed)
  config.h                    local secrets/settings (git-ignored)
```

### Display architecture (read before touching display_port.cpp)

- The RGB peripheral continuously scans a frame buffer out of PSRAM. We allocate **two**
  full-screen RGB565 frame buffers (2 x 768 KB in PSRAM) and a **bounce buffer** (800x10 px
  in internal RAM) which prevents the well-known S3 RGB "screen drift".
- LVGL renders in `LV_DISPLAY_RENDER_MODE_FULL` straight into whichever frame buffer is not
  on screen; `flushCb` swaps buffers and waits for the VSYNC callback. Tear-free, but every
  change redraws the full 800x480 frame, which is fine for a mostly static UI. Avoid
  continuous animations.
- `lv_timer_handler()` runs in a dedicated FreeRTOS task ("lvgl", core 1, priority 2).
- **Thread safety:** LVGL is not thread-safe. Any LVGL call from outside LVGL event callbacks
  (e.g. from `loop()` or a network task) must hold the lock:
  `{ display::Lock lock; ui::showForecast(fc); }`.
- Touch is polled by LVGL through `Touch::readPoints()`; coordinates are already screen
  coordinates (no swap/mirror). If touch ever appears mirrored, fix it with
  `ESP_PANEL_BOARD_TOUCH_SWAP_XY / MIRROR_X / MIRROR_Y` in the board conf, not in code.
- No touch calibration routine is needed. The GT911 is capacitive and factory calibrated;
  alignment was verified on hardware with a test screen (removed; see git history,
  commit 1afa426, `buildTouchTab` in `src/ui/ui.cpp`, if it's ever needed again).

### UI conventions

- 800x480 landscape, dark theme, 56 px header, 64 px bottom tab bar (big touch targets).
- Tab order: Weather (default), Hourly, Danger, Problems, Conditions, Details.
- Hourly tab (`src/ui/hourly_view.*`): the next 16 forecast slots (48 h in 3 h steps) from
  `Weather::hours`. It shows weekday/time, 36 px icons (day/night from the API's `sys.pod`), a
  temperature `lv_line` with values, a dashed freezing line, and snow (white) / rain (blue) bars
  scaled to max(5 mm, the largest slot). Absolute layout constants are at the top of the file.
  `lv_line` keeps a pointer to its points, so the point arrays are static. The tab adds ~100 small
  LVGL objects (~28 KB internal heap).
- Header: mountain logo + "Backcountry" (left), 24 h clock in gold `#FFC857` (centre; an LVGL
  timer that only changes the text once a minute), location + refresh button (right).
  The logo is Material Icons "terrain" (Apache 2.0, `assets/icons/`), mirrored and rasterized to
  an A8 image by `tools/make_logo.py` (output `src/ui/logo_icon.c` is small and committed;
  re-run the script after changing the SVG). A8 images are drawn in the `image_recolor` colour.
- The refresh button calls `updater::requestRefresh()`, which wakes the updater task to fetch both sources now without
  changing the daily avalanche schedule; the button is disabled while it runs.
- The avalanche issued time and download status (`ui::setStatus`) are in the Danger tab footer.
- Problems tab graphics (`src/ui/problem_graphics.*`) are drawn natively with LVGL draw callbacks
  rather than using Avalanche Canada's PNGs, which are designed for white backgrounds (their dark
  labels vanish on the navy theme). They are:
  - an aspect/elevation rose (8 aspects x 3 rings: centre alpine, outer below treeline) built
    from `data.aspectElevations` (e.g. `nw_alp`) into `Problem::aspectMask`
  - 5-step likelihood and size scales (`likelihoodMin/Max`, `sizeMin/Max`, half steps allowed)
  Drawing parameters are packed into the object's `user_data` pointer, so there's nothing to free.
  The Weather tab shows its own "Updated" time.
- Fonts enabled: Montserrat 14/20/24/28/40 (default 20). Enable more in `lv_conf.h`.
- Danger colours follow the North American Public Avalanche Danger Scale (see
  `forecast::dangerColor`). Text on High/Extreme is white, black otherwise.
- `ui::showForecast()` rebuilds the data tabs from a `forecast::Forecast`. Keep data fetching
  and parsing out of `ui/`.

## Known pitfalls

- **Wi-Fi + RGB panel drift/flicker**: heavy Wi-Fi traffic competes with the LCD for PSRAM
  bandwidth. Mitigations, in order: keep the bounce buffer; lower PCLK (e.g. 14 MHz) in the
  board conf; do downloads while the UI is idle; as a last resort, use pioarduino's
  `custom_sdkconfig` to enable `CONFIG_SPIRAM_FETCH_INSTRUCTIONS` / `CONFIG_SPIRAM_RODATA`
  and `CONFIG_LCD_RGB_RESTART_IN_VSYNC` (this triggers a slow full IDF rebuild).
- Changing `ARDUINO_USB_CDC_ON_BOOT` or partitions: do a full erase (`pio run -t erase`) first.
- Backlight is on/off only (EXIO2); there is no brightness control. `display::setScreenOn()` /
  `screen_schedule` turn it off from `SCREEN_OFF_HOUR` to `SCREEN_ON_HOUR` (default 23 to 6). The
  first touch on a dark screen wakes it for `SCREEN_WAKE_MINUTES` and is swallowed until release
  (in `touchReadCb`). Backlight writes go over the same I2C bus as touch, so only call them from
  the LVGL task. To test without waiting for night, build with
  `$env:PLATFORMIO_BUILD_FLAGS='-DSCREEN_OFF_HOUR=<now> -DSCREEN_ON_HOUR=<now+1> -DSCREEN_WAKE_MINUTES=1'`
  (works as long as config.h doesn't define them).
- Don't use `Wire` on GPIO8/9 alongside the panel library without setting the board conf's
  `*_SKIP_INIT_HOST` options, or the I2C host gets initialised twice.
- The SD card CS is on the expander (EXIO4), so the stock `SD` library's CS pin argument
  won't work directly. See Waveshare's `03_SD_Test` example.

## Avalanche Canada API

- Endpoint: `GET https://api.avalanche.ca/forecasts/{en|fr}/products/point?lat=..&long=..`
  returns the forecast for the region containing the point (~8 KB JSON, via CloudFront).
  No API key needed.
- Fields used (all under `report`): `dateIssued`, `validUntil` (ISO-8601 UTC), `title`,
  `highlights` (HTML), `confidence.rating.display` + `confidence.statements[]`,
  `summaries[]` (`type.value` = `avalanche-summary` / `snowpack-summary` / `weather-summary`,
  `content` HTML), `dangerRatings[3]` (`date.display`, `ratings.{alp,tln,btl}.rating.value` =
  `low|moderate|considerable|high|extreme|norating|...`), `problems[]` (`type.display`,
  `comment` HTML, `data.elevations[].display`, `data.aspects[].value`,
  `data.likelihood.value` like `possible_unlikely`, `data.expectedSize.{min,max}` as
  **strings** like `"1.0"`), `terrainAndTravelAdvice[]`.
- TLS: the chain ends at **Amazon Root CA 1**, which is pinned in `forecast_client.cpp`
  (valid to 2038). If the host moves off AWS, downloads fail with a TLS error.
- The server answers HTTP/1.0 with no Content-Length and closes the connection.
  `HTTPClient::getStream()` + streaming parse returned empty data, so we read the body with
  `getString()` and parse that. `Stream::setTimeout()` is in **milliseconds**.
- Text fields are HTML; `forecast::htmlToText()` converts them and maps Unicode
  punctuation and accented letters to characters the built-in Montserrat fonts contain
  (ASCII plus `°` and `•`). French text loses its accents for that reason; a custom
  font would be needed for full French support.

## Weather tab (OpenWeatherMap)

- Adapted from `D:\src\Electronics\ESP32_CYD_Weather_Station_With_Forecast` (CYD 320x240,
  TFT_eSPI). Only the data/icon logic was carried over; the UI is rebuilt in LVGL.
- Uses the **free** endpoints `GET /data/2.5/weather` (current) and `GET /data/2.5/forecast`
  (40 x 3-hour slots) on `api.openweathermap.org`, `units=metric|imperial`. Each refresh is 2
  calls; default every 30 min. Wind arrives in m/s for metric and is shown in km/h.
  Precipitation (`rain.3h`/`snow.3h`) is always in mm.
- Forecast slots are grouped by **local** calendar day (`localtime_r` with the configured
  TZ). Each day shows min/max temperature, max POP, summed snow/rain, and the condition
  from the slot nearest 13:00.
- Icons: 24-bit BMPs on black in `assets/weather-icons/` (100 px current, 50 px forecast,
  60 px moon, 50 px wind). The converter also builds a 36 px set (`wi_tiny_*`) by area-averaging
  the 100 px icons, for the Hourly tab. `tools/convert_icons.py` converts them to LVGL `RGB565A8` images
  with alpha = brightness (un-premultiplied) so they blend onto the dark UI. The ~4.6 MB
  generated C file is **not committed**; the PlatformIO pre-build hook regenerates it when the
  BMPs or the converter change. Image data is ~860 KB of flash.
- `OPENWEATHER_API_KEY` empty/missing => weather is disabled and the tab shows how to enable it.
  **Never log the request URL**; it contains the API key.
- TLS: api.openweathermap.org chains to **USERTrust RSA Certification Authority**; both roots
  live in `src/net/root_ca.cpp`.
- NTP is started (`configTzTime`) only after Wi-Fi connects. Starting it earlier made the first
  sync take ~30 s.

## Wi-Fi setup mode (`src/app/wifi_setup.*`, issue #8)

Only the Wi-Fi SSID/password can be set on the device; everything else stays in `config.h`.

- **Storage:** NVS via `Preferences`, namespace `wifi`: `ssid`, `pass`, `okssid` (the SSID that last
  connected), `setup` (one-shot "start setup on next boot"). `wifi_creds::load()` uses NVS if set,
  else `WIFI_SSID`/`WIFI_PASSWORD` from `config.h` (now optional, default `""`). `WiFi.persistent(false)`
  everywhere, so the driver's own NVS copy isn't used. Never log the password.
- **Entering setup** (decided in `setup()` in `main.cpp`, instead of building the tabview and starting
  the updater): no SSID at all; or the `setup` flag, set by holding refresh 3 s + confirming
  (`ui::setWifiSetupCallback`), or by the updater after 3 failed connects to an SSID that has
  **never** connected (`okssid`). A confirmed network never triggers setup, so router outages
  can't strand the display; the Danger footer says "Hold ⟳ to change Wi-Fi" instead.
- **Refresh button:** tap = `LV_EVENT_SHORT_CLICKED` (not sent after a long press), hold 3 s = confirm dialog.
- **Setup task** (core 0): scans first (STA), then `WIFI_AP_STA` with AP `Backcountry-XXXX` (last 2 MAC
  bytes), random 8-char WPA2 password (shown only on screen), AP IP 192.168.4.1. `DNSServer` answers
  every name; `WebServer` serves the page (`kPage`, inline HTML/JS, no external resources) and redirects
  unknown hosts to it, which triggers the phone's captive-portal sheet.
  Endpoints: `GET /scan.json[?refresh=1]`, `POST /save` (ssid, pass), `GET /status.json`, `POST /forget`.
- **Test before save:** `/save` starts `WiFi.begin()` while the AP stays up; the loop saves only after
  `WL_CONNECTED`, then restarts. The failure reason comes from `ARDUINO_EVENT_WIFI_STA_DISCONNECTED`
  (no AP found vs. auth/handshake failure). Connecting may move the AP to the router's channel and
  briefly drop the phone; the on-screen status is the source of truth.
- **Leaving:** success or "Forget" restarts. With credentials saved, a Cancel button restarts at once,
  and 10 min without HTTP requests or AP station changes restarts too. The `setup` flag is cleared
  when read, so a power cycle in setup also returns to normal mode.

## Over-the-air updates (`src/app/ota.*`, issue #3)

- ArduinoOTA (espota protocol, UDP 3232 + a TCP connection back to the uploading computer). **Off
  unless `OTA_PASSWORD` is set** in `config.h`. `OTA_HOSTNAME` (default `backcountry`) is the DHCP name
  (set in `updater::ensureWifi`) and the mDNS name `backcountry.local`.
- The `ota` task (core 0) waits until the updater's Wi-Fi is connected, calls `ArduinoOTA.begin()`, then
  polls `handle()`. The whole image is received inside `handle()`; it doesn't block the UI task. Not
  active in Wi-Fi setup mode.
- Upload: `pio run -t upload --upload-port backcountry.local` (or the IP). `tools/pio_ota.py` (pre-script)
  sees a network upload port, switches `UPLOAD_PROTOCOL` to `espota` and adds `--auth=<OTA_PASSWORD>` read
  from `config.h` via `UPLOAD_FLAGS`, so the password is never in `platformio.ini`. Serial ports are
  untouched. The password must match what the running firmware was built with.
- Upload stuck at "Waiting for device... No response from device" after "Authenticating... OK" = the
  device couldn't open the TCP connection back to the PC: a Windows Firewall rule blocks inbound to
  PlatformIO's `python.exe` (on this dev PC it's the uv-managed `cpython-3.13.x` interpreter). On the dev PC
  `backcountry.local` did not resolve (ProtonVPN running), so use the IP there.
- `default_16MB.csv` has two 6.4 MB app slots, so no partition change was needed. The new image boots
  only after it's fully written and verified (MD5); otherwise the old one keeps running. App rollback
  (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`) is **not** enabled, so a build that crashes at boot needs
  USB to recover. Test over USB first when touching startup code.
- UI: `ui::showFirmwareUpdate(percent)` / `ui::endFirmwareUpdate(error)` put a full-screen overlay on
  `lv_layer_top()` that swallows touches. Progress updates are throttled to 2 % steps (each redraws the
  whole frame). On failure the overlay closes itself after 6 s.
- While `ota::active()`, the updater skips downloads. Expect possible brief display flicker during the
  upload: flash writes stall PSRAM access (see the Wi-Fi + RGB pitfall).

## Stale forecast warning (`applyFreshness()` in `src/ui/ui.cpp`)

A safety cue so an old forecast is never mistaken for a current one. It's subtle but obvious:
no pop-ups and nothing hidden.

| State | When | Shows |
|---|---|---|
| Fresh | before `validUntil + 2 h` | normal |
| Overdue | `validUntil + 2 h` .. `+ 24 h` | amber warning icon before the header location, location in amber, Danger footer "Not yet updated - issued ..." |
| Expired | more than `validUntil + 24 h` | same in red-orange, footer "Expired - issued ...", current/future day cells outlined |
| Unknown | forecast shown but clock not set | amber, footer "... can't confirm it's current (clock not set)" |

- The 2 h grace avoids a false alarm every evening: `validUntil` is about 17:00 local, the daily
  check is 17:30, and re-checks follow if the forecast is late.
- Days whose date has passed (from `dangerRatings[].date.value`) are always faded to 40%, with a
  "Past" tag under the day name.
- Weather: the "Updated" line turns amber when the weather is more than 2 h old.
- Re-evaluated every minute from the header clock's LVGL timer and on every new forecast/weather.
  It restyles only when the computed state changes, since each change redraws the full frame.
- **Test hook:** build with `$env:PLATFORMIO_BUILD_FLAGS='-DFRESHNESS_TEST_OFFSET_HOURS=46'` to
  pretend it's 46 h later for these checks only (clear the variable afterwards). Use ~22 h after an
  evening forecast for Overdue, ~46 h for Expired.

## Offline cache (`src/storage/cache.*`)

- The raw JSON responses are saved on the LittleFS filesystem in the `spiffs` partition of
  `default_16MB.csv` (3.4 MB, mounted with `LittleFS.begin(true)`, formatted on first use):
  `/avalanche.cache` (~8 KB, written only when the issued time changes), and `/wx_current.cache` +
  `/wx_forecast.cache` (~17 KB, written on every weather refresh).
- At boot `updater::loadCached()` parses them with the **same parsers** as live downloads and
  shows them before Wi-Fi connects (about 1.5 s after power-on). Live data replaces them once it
  downloads.
- Each file starts with a header line `<fetched epoch> <key>`. The key is lat,lon,lang(,units), and
  entries saved for different settings are ignored, so a `config.h` change never shows another
  place's data.
- Writes go to `<name>.tmp` and are renamed over the old file, so a power cut mid-write keeps the
  previous copy.
- After a power loss the clock is unset until NTP syncs. Weather days ("Today", etc.) and the
  day/night icon then use the download time, and the Weather tab says "Saved Wed 18:57"
  instead of "Updated 18:57". If Wi-Fi is down, the Danger footer says
  "Offline - showing saved forecast". The forecast's own issued time shows how old it is.
- To clear the cache: `pio run -t erase` (erases all flash), or delete the files from code.

## Roadmap / next steps

Planned work is tracked in GitHub issues: https://github.com/rprouse/esp32_avalanche/issues
(`gh issue list`). Each issue includes context and pointers into the code. Keep ideas
there rather than in this file. 