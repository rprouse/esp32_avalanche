#pragma once

#include <lvgl.h>

#include "weather/alpine.h"
#include "weather/weather.h"

/**
 * Hourly tab: the next 48 h in 3-hour slots (weekday, time, icon, temperature
 * line with values, freezing line, snow/rain bars, chance of precipitation) and,
 * when mountain weather is available, freezing level and ridge wind rows.
 * Internal to the ui module; call with the display lock held.
 */
namespace ui {

void buildHourlyTab(lv_obj_t *tab);
void showHourly(const weather::Weather &w);
/** Mountain weather for the extra rows; `a` must stay valid (the ui module keeps it). */
void setHourlyAlpine(const weather::Alpine *a);
/** Message shown while there's no hourly data (e.g. no API key, download failed). */
void setHourlyMessage(const char *text);

// Shared with the Danger tab strip.
/** Text colour for a 700 hPa wind speed (km/h), by avalanche-forecast wind class. */
uint32_t windClassColor(float kmh);
/** Freezing level rounded for display (50 m / 100 ft), "<1000" when below the model ground. */
String fmtFreezingLevel(int16_t level, int groundElevation, bool metric);

}  // namespace ui
