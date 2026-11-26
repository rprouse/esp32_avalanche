#pragma once

#include <lvgl.h>

#include "weather/weather.h"

/**
 * Hourly tab: the next 48 h in 3-hour slots (weekday, time, icon, temperature
 * line with values, freezing line, snow/rain bars, chance of precipitation).
 * Internal to the ui module; call with the display lock held.
 */
namespace ui {

void buildHourlyTab(lv_obj_t *tab);
void showHourly(const weather::Weather &w);
/** Message shown while there's no hourly data (e.g. no API key, download failed). */
void setHourlyMessage(const char *text);

}  // namespace ui
