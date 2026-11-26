#pragma once

#include <Arduino.h>
#include <lvgl.h>

namespace esp_panel::board {
class Board;
}

/**
 * Glue between ESP32_Display_Panel (hardware) and LVGL v9 (UI).
 *
 * - Initialises the board: CH422G IO expander, ST7262 RGB panel, GT911 touch, backlight.
 * - Renders LVGL in FULL mode directly into the panel's two PSRAM frame buffers and
 *   swaps them on VSYNC (no tearing, no extra copies).
 * - Runs lv_timer_handler() in its own FreeRTOS task.
 *
 * Any code outside the LVGL task that touches LVGL objects MUST hold the lock:
 *
 *     { display::Lock lock; lv_label_set_text(label, "hi"); }
 */
namespace display {

constexpr int32_t kWidth = 800;
constexpr int32_t kHeight = 480;

/** Bring up hardware + LVGL and start the LVGL task. Returns false on failure. */
bool begin();

/** Take the LVGL mutex (recursive). timeout_ms < 0 waits forever. */
bool lock(int32_t timeout_ms = -1);
void unlock();

/** RAII helper for lock()/unlock(). */
class Lock {
public:
    explicit Lock(int32_t timeout_ms = -1) : locked_(lock(timeout_ms)) {}
    ~Lock() { if (locked_) unlock(); }
    bool ok() const { return locked_; }
    Lock(const Lock &) = delete;
    Lock &operator=(const Lock &) = delete;
private:
    bool locked_;
};

void setBacklight(bool on);

lv_display_t *lvDisplay();
lv_indev_t *lvTouch();
esp_panel::board::Board *board();

}  // namespace display
