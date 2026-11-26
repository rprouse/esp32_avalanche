#pragma once
/**
 * Pulls in the user's include/config.h (git-ignored). See include/config.sample.h.
 */
#if __has_include("config.h")
#include "config.h"
#else
#error "Missing include/config.h - copy include/config.sample.h to include/config.h and fill in your Wi-Fi details."
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
