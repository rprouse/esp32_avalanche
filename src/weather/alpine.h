#pragma once

#include <Arduino.h>
#include <vector>

/**
 * Mountain weather from Open-Meteo (free, no API key): freezing level, ridgetop
 * wind (700 hPa, about 3000 m) and snowfall, hourly for the past and next 72 h.
 *
 * Values are kept in metric; the UI converts for imperial display.
 * The default model (GEM, Environment Canada) doesn't publish a freezing level, so it
 * is computed from the temperatures and heights at the 850/700/600/500 hPa levels.
 */
namespace weather {

constexpr int16_t kFreezingUnknown = INT16_MIN;
constexpr int16_t kFreezingBelowGround = INT16_MIN + 1;  // already freezing at the model's ground level

struct AlpineHour {
    time_t time = 0;          // end of the hour, UTC epoch (snowfall is the sum of the preceding hour)
    int16_t freezingLevel = kFreezingUnknown;  // m above sea level, or one of the constants above
    int16_t windDeg = 0;      // 700 hPa wind direction (where it blows from)
    float windSpeed = NAN;    // 700 hPa wind, km/h
    float snowfall = 0;       // cm
};

struct Alpine {
    bool valid = false;
    bool metric = true;          // display units (set by the caller)
    int groundElevation = 0;     // m, height of the model grid cell
    std::vector<AlpineHour> hours;
    time_t fetched = 0;
    bool fromCache = false;

    /** Snowfall (cm) for hours ending in (from, to]. */
    float snowfall(time_t from, time_t to) const;
    /** The hour closest to `t` (within 30 min), or nullptr. */
    const AlpineHour *at(time_t t) const;
    /** Strongest wind among hours ending in (from, to], or nullptr. */
    const AlpineHour *maxWind(time_t from, time_t to) const;
};

/** Wind classes used in Canadian avalanche forecasts (km/h). */
enum class WindClass { Calm, Light, Moderate, Strong, Extreme };
WindClass windClass(float kmh);

/** Open-Meteo request settings, part of the cache key. */
String alpineKey(double lat, double lon, const char *model);

bool fetchAlpine(double lat, double lon, const char *model, Alpine &out, String &error, String *body = nullptr);
/** Parse a saved response. Sets everything except `metric`, `fetched` and `fromCache`. */
bool parseAlpine(const String &body, Alpine &out, String &error);

}  // namespace weather
