#pragma once

#include "forecast/forecast.h"

/**
 * LVGL user interface. All functions must be called with the display lock held
 * (see display::Lock) unless they are called from an LVGL event callback.
 *
 * Layout (800x480):
 *   +--------------------------------------------------+
 *   | title        region             issued / status  |  56 px
 *   +--------------------------------------------------+
 *   | tab content (swipe left/right or tap a tab)      |
 *   +--------------------------------------------------+
 *   | Danger | Problems | Conditions | Details | Touch  |  64 px tab bar
 *   +--------------------------------------------------+
 */
namespace ui {

enum class StatusLevel { Info, Ok, Error };

void build(const forecast::Forecast &fc);

/** Replace the displayed forecast (e.g. after a daily download). */
void showForecast(const forecast::Forecast &fc);

/** Small status line in the header (connection/download state). */
void setStatus(const char *text, StatusLevel level);

}  // namespace ui
