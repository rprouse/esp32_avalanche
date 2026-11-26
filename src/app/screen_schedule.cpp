#include "screen_schedule.h"

#include <Arduino.h>
#include <lvgl.h>
#include <time.h>

#include "app_config.h"
#include "display/display_port.h"

namespace screen_schedule {
namespace {

constexpr time_t kValidTime = 1700000000;  // before this the clock isn't set
constexpr uint32_t kWakeMs = SCREEN_WAKE_MINUTES * 60UL * 1000UL;

bool isNight(int hour) {
    if (SCREEN_OFF_HOUR == SCREEN_ON_HOUR) return false;  // disabled
    if (SCREEN_OFF_HOUR > SCREEN_ON_HOUR) return hour >= SCREEN_OFF_HOUR || hour < SCREEN_ON_HOUR;  // e.g. 23 -> 6
    return hour >= SCREEN_OFF_HOUR && hour < SCREEN_ON_HOUR;                                       // e.g. 1 -> 5
}

// Runs in the LVGL task (lv_timer), the same task that reads touch, so the
// backlight I2C writes never race the touch controller.
void check(lv_timer_t *) {
    const time_t now = time(nullptr);
    if (now < kValidTime) {
        display::setScreenOn(true);
        return;
    }
    struct tm tm;
    localtime_r(&now, &tm);
    if (!isNight(tm.tm_hour)) {
        display::setScreenOn(true);
        return;
    }
    // Night: stay on only within SCREEN_WAKE_MINUTES of the last touch.
    const uint32_t last = display::lastTouchMs();
    const bool recentlyTouched = last != 0 && millis() - last < kWakeMs;
    if (display::screenOn() && !recentlyTouched) display::setScreenOn(false);
}

}  // namespace

void start() {
    lv_timer_create(check, 1000, nullptr);
}

}  // namespace screen_schedule
