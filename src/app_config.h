#pragma once
/**
 * Pulls in the user's include/config.h (git-ignored). See include/config.sample.h.
 */
#if __has_include("config.h")
#include "config.h"
#else
#error "Missing include/config.h - copy include/config.sample.h to include/config.h and fill it in."
#endif

// Wi-Fi: optional. Normally set on the device (Wi-Fi setup mode, saved in NVS);
// these are only used when nothing has been saved there.
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

#ifndef FORECAST_LANGUAGE
#define FORECAST_LANGUAGE "en"
#endif
#ifndef NTP_SERVER_1
#define NTP_SERVER_1 "pool.ntp.org"
#endif
#ifndef NTP_SERVER_2
#define NTP_SERVER_2 "time.nist.gov"
#endif
#ifndef FORECAST_REFRESH_HOUR
#define FORECAST_REFRESH_HOUR 17
#endif
#ifndef FORECAST_REFRESH_MINUTE
#define FORECAST_REFRESH_MINUTE 30
#endif
#ifndef FORECAST_RECHECK_MINUTES
#define FORECAST_RECHECK_MINUTES 30
#endif
#ifndef FORECAST_RECHECK_MAX
#define FORECAST_RECHECK_MAX 8
#endif
#ifndef FORECAST_RETRY_MINUTES
#define FORECAST_RETRY_MINUTES 5
#endif

// Weather tab (optional; disabled when no API key is configured)
#ifndef OPENWEATHER_API_KEY
#define OPENWEATHER_API_KEY ""
#endif
#ifndef WEATHER_UNITS_METRIC
#define WEATHER_UNITS_METRIC 1
#endif
#ifndef WEATHER_REFRESH_MINUTES
#define WEATHER_REFRESH_MINUTES 30
#endif
#ifndef WEATHER_LATITUDE
#define WEATHER_LATITUDE FORECAST_LATITUDE
#endif
#ifndef WEATHER_LONGITUDE
#define WEATHER_LONGITUDE FORECAST_LONGITUDE
#endif

// Screen off overnight (equal hours = always on)
#ifndef SCREEN_OFF_HOUR
#define SCREEN_OFF_HOUR 23
#endif
#ifndef SCREEN_ON_HOUR
#define SCREEN_ON_HOUR 6
#endif
#ifndef SCREEN_WAKE_MINUTES
#define SCREEN_WAKE_MINUTES 5
#endif
