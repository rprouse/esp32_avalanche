#pragma once

#include "forecast/forecast.h"
#include "weather/weather.h"

/**
 * LVGL user interface. All functions must be called with the display lock held
 * (see display::Lock) unless they are called from an LVGL event callback.
 *
 * Layout (800x480):
 *   +--------------------------------------------------+
 *   | logo title         HH:MM          region [refresh]|  56 px
 *   +--------------------------------------------------+
 *   | tab content (swipe left/right or tap a tab)      |
 *   | (Danger tab footer: issued time + update status) |
 *   +--------------------------------------------------+
 *   | Weather | Hourly | Danger | Problems | Conditions | Details |  64 px tab bar
 *   +--------------------------------------------------+
 */
namespace ui {

enum class StatusLevel { Info, Ok, Error };

void build(const forecast::Forecast &fc);

/** Replace the displayed forecast (e.g. after a daily download). */
void showForecast(const forecast::Forecast &fc);

/** Status line (connection/download state) in the Danger tab footer. */
void setStatus(const char *text, StatusLevel level);

/** Weather tab: show new data, or an error/instruction message. */
void showWeather(const weather::Weather &w);
void setWeatherError(const char *text);

/** Header refresh button: callback runs in the LVGL task (keep it short, e.g. notify a task). */
void setRefreshCallback(void (*cb)());
/** Grey out the refresh button while a refresh is running. */
void setRefreshing(bool busy);

}  // namespace ui
