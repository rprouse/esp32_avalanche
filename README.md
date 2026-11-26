# esp32_avalanche

Avalanche Canada forecast display for the **Waveshare ESP32-S3-Touch-LCD-7** (800x480, GT911 touch),
built with PlatformIO (pioarduino), Arduino-ESP32 3.3 and LVGL 9.

## Build & flash

1. Open the folder in VS Code with the pioarduino IDE extension.
2. Plug the board in using the USB-C port labelled **UART1** (slide switch on UART1).
3. Run **Upload and Monitor** (or `pio run -t upload -t monitor` from PowerShell).

## What it does now

- Header plus four tabs (swipe or tap): **Danger** (3-day ratings by elevation, tap a cell to cycle it),
  **Problems**, **Details**, **Touch Test** (crosshair, coordinates, and alignment targets).
- The forecast data is placeholder sample data.

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
