# esp32_avalanche

Avalanche Canada forecast display for the **Waveshare ESP32-S3-Touch-LCD-7** (800x480, GT911 touch),
built with PlatformIO (pioarduino), Arduino-ESP32 3.3 and LVGL 9.

## Build & flash

1. Copy `include/config.sample.h` to `include/config.h` and fill in your Wi-Fi details, forecast
   location (default: Fernie, BC) and time zone. `config.h` is git-ignored.
2. Open the folder in VS Code with the pioarduino IDE extension.
3. Plug the board in using the USB-C port labelled **UART1** (slide switch on UART1).
4. Run **Upload and Monitor** (or `pio run -t upload -t monitor` from PowerShell).

## What it does

- Downloads the [Avalanche Canada](https://avalanche.ca) forecast for the configured location at power-on
  and once a day (default 17:30 local), re-checking until the new forecast is published.
- Tabs (swipe or tap):
  - **Weather** (shown first): current conditions, wind, sun and moon, and a 5-day forecast with snow and rain
  - **Hourly**: the next 48 hours in 3-hour steps, with a temperature line, freezing line and snow/rain bars
  - **Danger**: 3-day ratings by elevation band
  - **Problems**: avalanche problems with an aspect/elevation rose and likelihood and size scales
  - **Conditions**: avalanche, snowpack and weather summaries
  - **Details**: highlights, travel advice and confidence
- The Weather tab needs a free [OpenWeatherMap](https://openweathermap.org/api) API key in `config.h`
  (`OPENWEATHER_API_KEY`); without one the tab explains how to enable it.
- The last forecast and weather are saved to flash and shown immediately at power-on, even with no
  internet, until fresh data downloads.
- The screen turns off overnight (default 23:00-06:00); touch it to wake it for a few minutes.
- The refresh button in the header downloads both reports immediately. The Danger tab footer shows when
  the avalanche forecast was issued and last updated.

See [AGENTS.md](AGENTS.md) for hardware details, architecture, pitfalls and the roadmap.

## Using the native "USB" port (not currently enabled)

The board's second USB-C port (labelled **USB**) is the ESP32-S3's built-in USB. It can flash the board
and act as a serial port, but its pins (GPIO19/20) are shared with **CAN** through a switch chip. Expander
pin **EXIO5** on the CH422G selects between them: low = USB, high = CAN.

The display library drives every CH422G pin high at startup, so once this firmware boots the USB port
switches to CAN and disappears from the PC. The expander isn't reset when the ESP32 resets, so the port
stays off until power is fully removed.

To enable it (this disables CAN), add this after `board->begin()` in `src/display/display_port.cpp`:

```cpp
s_board->getIO_Expander()->getBase()->digitalWrite(5, 0);  // EXIO5 low = native USB
```

Then:
- **Flashing** over the USB port works with automatic reset (the S3's USB-Serial/JTAG unit).
  Until that firmware is on the board, put it in download mode by hand: hold BOOT, tap RESET, release BOOT.
- **Serial output** stays on the UART1 port. To move `Serial` to USB, set `-DARDUINO_USB_CDC_ON_BOOT=1`
  in `platformio.ini` and erase the flash once (`pio run -t erase`).

## Credits

The Weather tab is adapted from the CYD weather station sketch by Daniel Eichhorn (squix) and Bodmer's
[OpenWeather](https://github.com/Bodmer/OpenWeather) library. The weather, moon and wind icons are Bodmer's,
used under the FreeBSD license (see [assets/weather-icons](assets/weather-icons)). The mountain logo is the
Material Icons "terrain" icon by Google, Apache 2.0 (see [assets/icons](assets/icons)).

## License

MIT. See [LICENSE](LICENSE). The weather icons (FreeBSD) and the logo (Apache 2.0) keep their own licenses. `include/esp_panel_board_custom_conf.h` is derived from Espressif/Waveshare
sample code and remains under the Apache License 2.0, as stated in its header.
