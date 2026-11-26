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
// 2.4 GHz networks only (the ESP32-S3 has no 5 GHz radio).
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

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
// Fernie is on Mountain Time:      "MST7MDT,M3.2.0,M11.1.0"
// Most of BC is on Pacific Time:   "PST8PDT,M3.2.0,M11.1.0"
// Alberta (Mountain):              "MST7MDT,M3.2.0,M11.1.0"
// More: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
#define TIME_ZONE           "MST7MDT,M3.2.0,M11.1.0"
#define NTP_SERVER_1        "pool.ntp.org"
#define NTP_SERVER_2        "time.nist.gov"

// ----------------------------------------------------------------------------- Refresh schedule
// Forecasts are usually published around 16:00-17:00 Pacific. The display checks
// once a day at this local time, then keeps checking every
// FORECAST_RECHECK_MINUTES until a newer forecast appears (up to
// FORECAST_RECHECK_MAX tries). It also fetches at power-on.
#define FORECAST_REFRESH_HOUR     17
#define FORECAST_REFRESH_MINUTE   30
#define FORECAST_RECHECK_MINUTES  30
#define FORECAST_RECHECK_MAX      8
// Retry interval after a network/download error.
#define FORECAST_RETRY_MINUTES    5
