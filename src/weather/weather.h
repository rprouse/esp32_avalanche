#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <vector>

/**
 * Weather from OpenWeatherMap's free APIs (current weather + 5 day / 3 hour forecast),
 * reduced to what the Weather tab shows. Adapted from the CYD weather station project
 * (Daniel Eichhorn / Bodmer's OpenWeather example).
 */
namespace weather {

struct Current {
    float temp = 0;        // in display units (°C or °F)
    float feelsLike = 0;
    int conditionId = 0;   // OpenWeatherMap condition code, e.g. 600 = light snow
    String description;    // e.g. "Light snow"
    float windSpeed = 0;   // km/h (metric) or mph (imperial)
    float windGust = 0;
    int windDeg = 0;
    int humidity = 0;      // %
    int pressure = 0;      // hPa
    int clouds = 0;        // %
    float visibility = 0;  // km (metric) or miles (imperial)
    time_t sunrise = 0;    // UTC epoch
    time_t sunset = 0;
    time_t observed = 0;
};

struct Day {
    String label;          // "Today", "Thu", ...
    float tempMin = 0;
    float tempMax = 0;
    int conditionId = 0;   // representative (around midday) condition
    String description;
    int pop = 0;           // max probability of precipitation, %
    float snow = 0;        // total, mm (metric) or in (imperial)
    float rain = 0;
};

struct Weather {
    bool valid = false;
    bool metric = true;
    Current now;
    std::vector<Day> days;  // today + following days (up to 5)
    time_t fetched = 0;
};

/** Downloads current conditions and forecast. Wi-Fi and clock must be ready. */
bool fetch(double lat, double lon, const char *apiKey, bool metric, const char *lang, Weather &out,
           String &error);

/** Icon for a condition code. `night` picks the moon variants for clear/cloudy. */
const lv_image_dsc_t *largeIcon(int conditionId, bool night);  // 100x100
const lv_image_dsc_t *smallIcon(int conditionId);              // 50x50
const lv_image_dsc_t *windIcon(int windDeg);                   // 50x50 compass
const char *windDirName(int windDeg);                          // "N", "NE", ...

/** Moon phase at `t`: icon (60x60) and name such as "Waxing gibbous". */
const lv_image_dsc_t *moonIcon(time_t t);
const char *moonPhaseName(time_t t);
int moonIllumination(time_t t);  // percent

}  // namespace weather
