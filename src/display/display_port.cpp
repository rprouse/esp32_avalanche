#include "display_port.h"

#include <climits>
#include <esp_display_panel.hpp>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

using namespace esp_panel::drivers;
using esp_panel::board::Board;

namespace display {
namespace {

constexpr uint32_t kTaskStack = 12 * 1024;
constexpr UBaseType_t kTaskPriority = 2;
constexpr uint32_t kTaskMinDelayMs = 2;
constexpr uint32_t kTaskMaxDelayMs = 500;

Board *s_board = nullptr;
LCD *s_lcd = nullptr;
Touch *s_touch = nullptr;
lv_display_t *s_disp = nullptr;
lv_indev_t *s_indev = nullptr;
SemaphoreHandle_t s_mutex = nullptr;
TaskHandle_t s_task = nullptr;

uint32_t tickCb() { return millis(); }

// Called from the RGB panel ISR when a frame has been scanned out (VSYNC).
IRAM_ATTR bool onVsync(void *user_data) {
    BaseType_t need_yield = pdFALSE;
    xTaskNotifyFromISR(static_cast<TaskHandle_t>(user_data), ULONG_MAX, eNoAction, &need_yield);
    return need_yield == pdTRUE;
}

// LVGL renders the whole screen into one of the panel's frame buffers (FULL mode).
// When the last chunk is done, point the panel at that buffer and wait for VSYNC so
// LVGL does not start drawing into the buffer that is still being scanned out.
void flushCb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    LV_UNUSED(area);
    if (lv_display_flush_is_last(disp)) {
        s_lcd->switchFrameBufferTo(px_map);
        ulTaskNotifyValueClear(nullptr, ULONG_MAX);
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
    }
    lv_display_flush_ready(disp);
}

void touchReadCb(lv_indev_t *indev, lv_indev_data_t *data) {
    LV_UNUSED(indev);
    TouchPoint point;
    if (s_touch != nullptr && s_touch->readPoints(&point, 1, 0) > 0) {
        data->point.x = point.x;
        data->point.y = point.y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void lvglTask(void *) {
    uint32_t delay_ms = kTaskMaxDelayMs;
    for (;;) {
        if (lock()) {
            delay_ms = lv_timer_handler();
            unlock();
        }
        delay_ms = constrain(delay_ms, kTaskMinDelayMs, kTaskMaxDelayMs);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

}  // namespace

bool begin() {
    s_board = new Board();
    if (!s_board->init()) {
        log_e("Board init failed");
        return false;
    }

    s_lcd = s_board->getLCD();
    if (s_lcd == nullptr) {
        log_e("No LCD");
        return false;
    }
    // Two full-size frame buffers in PSRAM for tear-free buffer swapping, plus a
    // bounce buffer in internal RAM. The bounce buffer is what prevents the
    // "screen drift" the ESP32-S3 RGB peripheral is known for under PSRAM load.
    s_lcd->configFrameBufferNumber(2);
    static_cast<BusRGB *>(s_lcd->getBus())->configRGB_BounceBufferSize(s_lcd->getFrameWidth() * 10);

    if (!s_board->begin()) {
        log_e("Board begin failed");
        return false;
    }
    s_touch = s_board->getTouch();
    if (s_touch == nullptr) {
        log_w("No touch controller found; continuing without touch");
    }

    s_mutex = xSemaphoreCreateRecursiveMutex();
    lv_init();
    lv_tick_set_cb(tickCb);

    const int32_t w = s_lcd->getFrameWidth();
    const int32_t h = s_lcd->getFrameHeight();
    void *buf1 = s_lcd->getFrameBufferByIndex(0);
    void *buf2 = s_lcd->getFrameBufferByIndex(1);
    if (buf1 == nullptr || buf2 == nullptr) {
        log_e("Frame buffers not available (is PSRAM enabled?)");
        return false;
    }

    s_disp = lv_display_create(w, h);
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(s_disp, buf1, buf2, w * h * sizeof(uint16_t), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(s_disp, flushCb);

    if (s_touch != nullptr) {
        s_indev = lv_indev_create();
        lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(s_indev, touchReadCb);
        lv_indev_set_display(s_indev, s_disp);
    }

    if (xTaskCreatePinnedToCore(lvglTask, "lvgl", kTaskStack, nullptr, kTaskPriority, &s_task,
                                ARDUINO_RUNNING_CORE) != pdPASS) {
        log_e("Failed to create LVGL task");
        return false;
    }
    s_lcd->attachRefreshFinishCallback(onVsync, s_task);

    log_i("Display ready: %dx%d, touch=%s", (int)w, (int)h, s_touch ? "yes" : "no");
    return true;
}

bool lock(int32_t timeout_ms) {
    if (s_mutex == nullptr) return false;
    const TickType_t ticks = timeout_ms < 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(s_mutex, ticks) == pdTRUE;
}

void unlock() {
    if (s_mutex != nullptr) xSemaphoreGiveRecursive(s_mutex);
}

void setBacklight(bool on) {
    if (s_board == nullptr || s_board->getBacklight() == nullptr) return;
    if (on) {
        s_board->getBacklight()->on();
    } else {
        s_board->getBacklight()->off();
    }
}

lv_display_t *lvDisplay() { return s_disp; }
lv_indev_t *lvTouch() { return s_indev; }
Board *board() { return s_board; }

}  // namespace display
