#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <time.h>

#include "net/root_ca.h"
#include "weather.h"
#include "weather_icons.h"

namespace weather {
namespace {

constexpr uint32_t kHttpTimeoutMs = 15000;
constexpr int kMaxDays = 5;

bool httpGet(const String &url, String &body, String &error) {
    NetworkClientSecure client;
    client.setCACert(net::kRootCAs);
    client.setTimeout(kHttpTimeoutMs);  // milliseconds

    HTTPClient http;
    http.setTimeout(kHttpTimeoutMs);
    http.setUserAgent("esp32-avalanche-display/1.0");
    if (!http.begin(client, url)) {
        error = "HTTP begin failed";
        return false;
    }
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        if (code == 401) error = "invalid API key (HTTP 401)";
        else error = code < 0 ? String("HTTP: ") + HTTPClient::errorToString(code) : String("HTTP ") + code;
        http.end();
        return false;
    }
    body = http.getString();
    http.end();
    if (body.isEmpty()) {
        error = "empty response";
        return false;
    }
    return true;
}

String capitalize(const char *s) {
    String out(s ? s : "");
    if (out.length()) out.setCharAt(0, toupper(out[0]));
    return out;
}

String baseUrl(const char *endpoint, double lat, double lon, const char *apiKey, bool metric, const char *lang) {
    char buf[96];
    snprintf(buf, sizeof buf, "https://api.openweathermap.org/data/2.5/%s?lat=%.4f&lon=%.4f", endpoint, lat, lon);
    return String(buf) + "&units=" + (metric ? "metric" : "imperial") + "&lang=" + lang + "&appid=" + apiKey;
}

bool parseCurrent(const String &body, bool metric, Current &c, String &error) {
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, body);
    if (err) {
        error = String("weather JSON: ") + err.c_str();
        return false;
    }
    c.temp = doc["main"]["temp"] | 0.0f;
    c.feelsLike = doc["main"]["feels_like"] | 0.0f;
    c.humidity = doc["main"]["humidity"] | 0;
    c.pressure = doc["main"]["pressure"] | 0;
    c.conditionId = doc["weather"][0]["id"] | 0;
    c.description = capitalize(doc["weather"][0]["description"]);
    const float windScale = metric ? 3.6f : 1.0f;  // m/s -> km/h
    c.windSpeed = (doc["wind"]["speed"] | 0.0f) * windScale;
    c.windGust = (doc["wind"]["gust"] | 0.0f) * windScale;
    c.windDeg = doc["wind"]["deg"] | 0;
    c.clouds = doc["clouds"]["all"] | 0;
    const float visM = doc["visibility"] | 0.0f;
    c.visibility = metric ? visM / 1000.0f : visM / 1609.34f;
    c.sunrise = doc["sys"]["sunrise"] | 0;
    c.sunset = doc["sys"]["sunset"] | 0;
    c.observed = doc["dt"] | 0;
    return true;
}

int localDayKey(time_t t) {
    struct tm tm;
    localtime_r(&t, &tm);
    return (tm.tm_year + 1900) * 1000 + tm.tm_yday;
}

bool parseForecast(const String &body, bool metric, time_t now, Weather &out, String &error) {
    JsonDocument filter;
    JsonObject item = filter["list"][0].to<JsonObject>();
    item["dt"] = true;
    item["main"]["temp_min"] = true;
    item["main"]["temp_max"] = true;
    item["main"]["temp"] = true;
    item["sys"]["pod"] = true;
    item["weather"][0]["id"] = true;
    item["weather"][0]["description"] = true;
    item["pop"] = true;
    item["rain"]["3h"] = true;
    item["snow"]["3h"] = true;

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
    if (err) {
        error = String("forecast JSON: ") + err.c_str();
        return false;
    }

    const float precipScale = metric ? 1.0f : 1.0f / 25.4f;  // API always reports mm
    const int today = localDayKey(now);
    std::vector<Day> &days = out.days;
    days.clear();
    out.hours.clear();
    int currentKey = -1;
    int bestMiddayDistance = 99;
    for (JsonObjectConst slot : doc["list"].as<JsonArrayConst>()) {
        const time_t dt = slot["dt"] | 0;

        // Hourly: slots that haven't ended yet, up to kHourSlots.
        if (dt + 3 * 3600 > now && (int)out.hours.size() < kHourSlots) {
            Hour h;
            h.time = dt;
            h.temp = slot["main"]["temp"] | 0.0f;
            h.conditionId = slot["weather"][0]["id"] | 0;
            h.night = strcmp(slot["sys"]["pod"] | "d", "n") == 0;
            h.pop = (int)lroundf((slot["pop"] | 0.0f) * 100);
            h.snow = (slot["snow"]["3h"] | 0.0f) * precipScale;
            h.rain = (slot["rain"]["3h"] | 0.0f) * precipScale;
            out.hours.push_back(h);
        }

        const int key = localDayKey(dt);
        if (key != currentKey) {
            if ((int)days.size() == kMaxDays) break;
            currentKey = key;
            Day d;
            if (key == today) {
                d.label = "Today";
            } else {
                struct tm tm;
                localtime_r(&dt, &tm);
                char buf[8];
                strftime(buf, sizeof buf, "%a", &tm);
                d.label = buf;
            }
            d.tempMin = 1e6;
            d.tempMax = -1e6;
            days.push_back(d);
            bestMiddayDistance = 99;
        }
        Day &d = days.back();
        d.tempMin = min(d.tempMin, slot["main"]["temp_min"] | 0.0f);
        d.tempMax = max(d.tempMax, slot["main"]["temp_max"] | 0.0f);
        d.pop = max(d.pop, (int)lroundf((slot["pop"] | 0.0f) * 100));
        d.rain += (slot["rain"]["3h"] | 0.0f) * precipScale;
        d.snow += (slot["snow"]["3h"] | 0.0f) * precipScale;

        // Representative condition: the slot closest to 13:00 local.
        struct tm tm;
        localtime_r(&dt, &tm);
        const int distance = abs(tm.tm_hour - 13);
        if (distance < bestMiddayDistance) {
            bestMiddayDistance = distance;
            d.conditionId = slot["weather"][0]["id"] | 0;
            d.description = capitalize(slot["weather"][0]["description"]);
        }
    }
    if (days.empty()) {
        error = "forecast has no data";
        return false;
    }
    return true;
}

}  // namespace

bool parse(const String &currentBody, const String &forecastBody, bool metric, time_t now, Weather &out,
           String &error) {
    if (!parseCurrent(currentBody, metric, out.now, error)) return false;
    if (!parseForecast(forecastBody, metric, now, out, error)) return false;
    out.metric = metric;
    out.valid = true;
    return true;
}

bool fetch(double lat, double lon, const char *apiKey, bool metric, const char *lang, Weather &out, String &error,
           String *currentBody, String *forecastBody) {
    String current, forecast;
    if (!httpGet(baseUrl("weather", lat, lon, apiKey, metric, lang), current, error)) return false;
    if (!httpGet(baseUrl("forecast", lat, lon, apiKey, metric, lang), forecast, error)) return false;
    const time_t now = time(nullptr);
    if (!parse(current, forecast, metric, now, out, error)) return false;
    out.fetched = now;
    out.fromCache = false;
    if (currentBody != nullptr) *currentBody = current;
    if (forecastBody != nullptr) *forecastBody = forecast;
    return true;
}

// ------------------------------------------------------------------ icons
// Mapping from OpenWeatherMap condition codes, as in the original getMeteoconIcon().

namespace {
enum class Icon { Thunderstorm, Drizzle, LightRain, Rain, Sleet, Snow, Fog, Wind, ClearDay, ClearNight,
                  PartlyCloudyDay, PartlyCloudyNight, Cloudy, Unknown };

Icon iconFor(int id, bool night) {
    if (id / 100 == 2) return Icon::Thunderstorm;
    if (id / 100 == 3) return Icon::Drizzle;
    if (id == 500) return Icon::LightRain;
    if (id == 511) return Icon::Sleet;
    if (id / 100 == 5) return Icon::Rain;
    if (id >= 611 && id <= 616) return Icon::Sleet;
    if (id / 100 == 6) return Icon::Snow;
    if (id == 771 || id == 781) return Icon::Wind;
    if (id / 100 == 7) return Icon::Fog;
    if (id == 800) return night ? Icon::ClearNight : Icon::ClearDay;
    if (id == 801) return night ? Icon::PartlyCloudyNight : Icon::PartlyCloudyDay;
    if (id / 100 == 8) return Icon::Cloudy;
    return Icon::Unknown;
}
}  // namespace

const lv_image_dsc_t *largeIcon(int id, bool night) {
    switch (iconFor(id, night)) {
        case Icon::Thunderstorm: return &wi_large_thunderstorm;
        case Icon::Drizzle: return &wi_large_drizzle;
        case Icon::LightRain: return &wi_large_lightrain;
        case Icon::Rain: return &wi_large_rain;
        case Icon::Sleet: return &wi_large_sleet;
        case Icon::Snow: return &wi_large_snow;
        case Icon::Fog: return &wi_large_fog;
        case Icon::Wind: return &wi_large_wind;
        case Icon::ClearDay: return &wi_large_clear_day;
        case Icon::ClearNight: return &wi_large_clear_night;
        case Icon::PartlyCloudyDay: return &wi_large_partly_cloudy_day;
        case Icon::PartlyCloudyNight: return &wi_large_partly_cloudy_night;
        case Icon::Cloudy: return &wi_large_cloudy;
        default: return &wi_large_unknown;
    }
}

const lv_image_dsc_t *tinyIcon(int id, bool night) {
    switch (iconFor(id, night)) {
        case Icon::Thunderstorm: return &wi_tiny_thunderstorm;
        case Icon::Drizzle: return &wi_tiny_drizzle;
        case Icon::LightRain: return &wi_tiny_lightrain;
        case Icon::Rain: return &wi_tiny_rain;
        case Icon::Sleet: return &wi_tiny_sleet;
        case Icon::Snow: return &wi_tiny_snow;
        case Icon::Fog: return &wi_tiny_fog;
        case Icon::Wind: return &wi_tiny_wind;
        case Icon::ClearDay: return &wi_tiny_clear_day;
        case Icon::ClearNight: return &wi_tiny_clear_night;
        case Icon::PartlyCloudyDay: return &wi_tiny_partly_cloudy_day;
        case Icon::PartlyCloudyNight: return &wi_tiny_partly_cloudy_night;
        case Icon::Cloudy: return &wi_tiny_cloudy;
        default: return &wi_tiny_unknown;
    }
}

const lv_image_dsc_t *smallIcon(int id) {
    switch (iconFor(id, false)) {
        case Icon::Thunderstorm: return &wi_small_thunderstorm;
        case Icon::Drizzle: return &wi_small_drizzle;
        case Icon::LightRain: return &wi_small_lightrain;
        case Icon::Rain: return &wi_small_rain;
        case Icon::Sleet: return &wi_small_sleet;
        case Icon::Snow: return &wi_small_snow;
        case Icon::Fog: return &wi_small_fog;
        case Icon::Wind: return &wi_small_wind;
        case Icon::ClearDay: return &wi_small_clear_day;
        case Icon::PartlyCloudyDay: return &wi_small_partly_cloudy_day;
        case Icon::Cloudy: return &wi_small_cloudy;
        default: return &wi_small_unknown;
    }
}

const char *windDirName(int deg) {
    static const char *const kDirs[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    return kDirs[((deg % 360) + 360 + 22) % 360 / 45];
}

const lv_image_dsc_t *windIcon(int deg) {
    static const lv_image_dsc_t *const kIcons[8] = {&wi_wind_n, &wi_wind_ne, &wi_wind_e, &wi_wind_se,
                                                    &wi_wind_s, &wi_wind_sw, &wi_wind_w, &wi_wind_nw};
    return kIcons[((deg % 360) + 360 + 22) % 360 / 45];
}

}  // namespace weather
