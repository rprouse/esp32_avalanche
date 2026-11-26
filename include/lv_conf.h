/**
 * LVGL v9 configuration for the Waveshare ESP32-S3-Touch-LCD-7.
 *
 * Only overrides are listed here; everything else falls back to the defaults
 * in lvgl/src/lv_conf_internal.h. See the LVGL lv_conf_template.h for the full
 * list of options.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* Panel is driven as RGB565 over a 16-bit parallel bus. */
#define LV_COLOR_DEPTH 16

/* Use the C library allocator. On this board malloc() places large blocks in
 * PSRAM automatically, so LVGL objects/images are not limited by a fixed pool. */
#define LV_USE_STDLIB_MALLOC  LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING  LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

/* Locking is done by our own mutex in src/display/display_port.cpp. */
#define LV_USE_OS LV_OS_NONE

/* Display refresh period (ms). Full-frame rendering on 800x480 is not fast;
 * a forecast display is mostly static so this is plenty. */
#define LV_DEF_REFR_PERIOD 20
#define LV_DPI_DEF 130

/* Logging -> Serial (via printf) */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/* Fonts */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_40 1
#define LV_FONT_DEFAULT &lv_font_montserrat_20

/* QR codes on the Wi-Fi setup screen */
#define LV_USE_QRCODE 1

/* Handy while developing. Shows FPS/CPU in a corner when set to 1. */
#define LV_USE_SYSMON 0
#define LV_USE_PERF_MONITOR 0

/* Keep demos/examples out of the build */
#define LV_BUILD_EXAMPLES 0
#define LV_BUILD_DEMOS 0

#endif /* LV_CONF_H */
