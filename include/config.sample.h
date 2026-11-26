#pragma once
/**
 * Local configuration TEMPLATE.
 *
 * Copy this file to include/config.h and fill in your values:
 *
 *     copy include\config.sample.h include\config.h      (Windows)
 *     cp include/config.sample.h include/config.h        (macOS/Linux)
 *
 * include/config.h is git-ignored, so your Wi-Fi password never gets committed.
 */

// ----------------------------------------------------------------------------- Wi-Fi
// Optional. With no Wi-Fi configured, the display starts in Wi-Fi setup mode:
// it shows a QR code to join its own network from a phone, where you pick your
// Wi-Fi and enter its password. That choice is saved on the device and takes
// precedence over these values. To change it later, hold the refresh button.
// 2.4 GHz networks only (the ESP32-S3 has no 5 GHz radio).
// #define WIFI_SSID     "your-ssid"
// #define WIFI_PASSWORD "your-password"

// ----------------------------------------------------------------------------- Forecast location
// Any point inside the forecast region you want. Avalanche Canada returns the
// forecast for the region that contains this point.
// Default: Fernie, BC (Lizard Range and Flathead / South Rockies area).
#define FORECAST_LATITUDE   49.5040
#define FORECAST_LONGITUDE  -115.0631

// Name shown in the header. The API's own region title is a long list of
// sub-areas, so a short friendly name reads better.
#define FORECAST_NAME       "Fernie, BC"

// "en" or "fr"
#define FORECAST_LANGUAGE   "en"

// ----------------------------------------------------------------------------- Time
// POSIX TZ string for the local time zone (used for the clock and refresh schedule).
// BC and Alberta now stay on daylight time all year (no more clock changes), so
// these are fixed offsets with no DST rule. Note the POSIX sign is inverted:
// "MDT6" means 6 hours *behind* UTC.
// Fernie (East Kootenay), Mountain Daylight Time, UTC-6:   "MDT6"
// Alberta, Mountain Daylight Time, UTC-6:                  "MDT6"
// Rest of BC, Pacific Daylight Time, UTC-7:                "PDT7"
// Regions that still switch clocks need a DST rule, e.g. Pacific with DST:
// "PST8PDT,M3.2.0,M11.1.0". More: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
// (that list may not reflect recent changes like BC's and Alberta's).
#define TIME_ZONE           "MDT6"
#define NTP_SERVER_1        "pool.ntp.org"
#define NTP_SERVER_2        "time.nist.gov"

// ----------------------------------------------------------------------------- Refresh schedule
// Forecasts are usually published around 16:00 Pacific (17:00 in Fernie/Alberta). The display checks
// once a day at this local time, then keeps checking every
// FORECAST_RECHECK_MINUTES until a newer forecast appears (up to
// FORECAST_RECHECK_MAX tries). It also fetches at power-on.
#define FORECAST_REFRESH_HOUR     17
#define FORECAST_REFRESH_MINUTE   30
#define FORECAST_RECHECK_MINUTES  30
#define FORECAST_RECHECK_MAX      8
// Retry interval after a network/download error.
#define FORECAST_RETRY_MINUTES    5

// ----------------------------------------------------------------------------- Weather tab (OpenWeatherMap)
// Free API key from https://home.openweathermap.org/api_keys (the free plan allows
// 1000 calls/day; each refresh uses 2). Leave empty to disable the Weather tab.
#define OPENWEATHER_API_KEY      ""
// 1 = metric (°C, km/h, mm), 0 = imperial (°F, mph, in)
#define WEATHER_UNITS_METRIC     1
#define WEATHER_REFRESH_MINUTES  30
// Weather location. Defaults to the forecast location above; set these to use
// a different point (e.g. a ski hill instead of town).
// #define WEATHER_LATITUDE      49.4627
// #define WEATHER_LONGITUDE     -115.0873

// ----------------------------------------------------------------------------- Screen
// Turn the screen (backlight) off overnight, local time. Touching the screen wakes
// it for SCREEN_WAKE_MINUTES; that first touch doesn't press anything. Set
// SCREEN_OFF_HOUR equal to SCREEN_ON_HOUR to keep the screen on all the time.
#define SCREEN_OFF_HOUR      23
#define SCREEN_ON_HOUR       6
#define SCREEN_WAKE_MINUTES  5
