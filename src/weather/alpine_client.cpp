#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <esp_heap_caps.h>
#include <math.h>

#include "alpine.h"
#include "net/root_ca.h"

namespace weather {
namespace {

constexpr uint32_t kHttpTimeoutMs = 15000;

// Pressure levels used for the freezing level. 850 hPa is about 1500 m, 500 hPa about 5500 m.
constexpr int kLevels[] = {850, 700, 600, 500};
constexpr int kLevelCount = sizeof(kLevels) / sizeof(kLevels[0]);

// The response is ~13 KB with ~1700 numbers; keep the parsed document in PSRAM.
struct SpiRamAllocator : ArduinoJson::Allocator {
    void *allocate(size_t size) override { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
    void deallocate(void *p) override { heap_caps_free(p); }
    void *reallocate(void *p, size_t size) override {
        return heap_caps_realloc(p, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
};
SpiRamAllocator s_allocator;

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
        error = code < 0 ? String("HTTP: ") + HTTPClient::errorToString(code) : String("HTTP ") + code;
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

float valueAt(JsonArrayConst a, size_t i) {
    JsonVariantConst v = a[i];
    return v.isNull() ? NAN : v.as<float>();
}

/**
 * Highest height where the temperature drops through 0 °C going up, interpolated
 * linearly between levels. Levels below the ground (extrapolated by the model) are skipped.
 */
int16_t freezingLevel(float groundZ, float groundT, const float *z, const float *t) {
    float pz[kLevelCount + 1], pt[kLevelCount + 1];
    int n = 0;
    if (!isnan(groundT)) {
        pz[n] = groundZ;
        pt[n++] = groundT;
    }
    for (int i = 0; i < kLevelCount; ++i) {
        if (isnan(z[i]) || isnan(t[i]) || z[i] <= groundZ) continue;
        pz[n] = z[i];
        pt[n++] = t[i];
    }
    if (n == 0) return kFreezingUnknown;
    for (int i = n - 2; i >= 0; --i) {
        if (pt[i] > 0 && pt[i + 1] <= 0) {
            return (int16_t)lroundf(pz[i] + (pz[i + 1] - pz[i]) * pt[i] / (pt[i] - pt[i + 1]));
        }
    }
    if (pt[0] <= 0) return kFreezingBelowGround;
    return (int16_t)lroundf(pz[n - 1]);  // above freezing all the way up (rare): report the top level
}

}  // namespace

float Alpine::snowfall(time_t from, time_t to) const {
    float sum = 0;
    for (const auto &h : hours) {
        if (h.time > from && h.time <= to) sum += h.snowfall;
    }
    return sum;
}

const AlpineHour *Alpine::at(time_t t) const {
    const AlpineHour *best = nullptr;
    long bestDiff = 1801;
    for (const auto &h : hours) {
        const long diff = labs((long)(h.time - t));
        if (diff < bestDiff) {
            bestDiff = diff;
            best = &h;
        }
    }
    return best;
}

const AlpineHour *Alpine::maxWind(time_t from, time_t to) const {
    const AlpineHour *best = nullptr;
    for (const auto &h : hours) {
        if (h.time <= from || h.time > to || isnan(h.windSpeed)) continue;
        if (best == nullptr || h.windSpeed > best->windSpeed) best = &h;
    }
    return best;
}

WindClass windClass(float kmh) {
    if (kmh < 1) return WindClass::Calm;
    if (kmh <= 25) return WindClass::Light;
    if (kmh <= 40) return WindClass::Moderate;
    if (kmh <= 60) return WindClass::Strong;
    return WindClass::Extreme;
}

String alpineKey(double lat, double lon, const char *model) {
    return String(lat, 4) + "," + String(lon, 4) + "," + model;
}

bool parseAlpine(const String &body, Alpine &out, String &error) {
    JsonDocument doc(&s_allocator);
    const DeserializationError err = deserializeJson(doc, body);
    if (err) {
        error = String("alpine JSON: ") + err.c_str();
        return false;
    }
    if (doc["error"] | false) {
        error = String("Open-Meteo: ") + (doc["reason"] | "error");
        return false;
    }
    JsonObjectConst h = doc["hourly"];
    JsonArrayConst times = h["time"];
    if (times.size() == 0) {
        error = "alpine forecast has no data";
        return false;
    }
    JsonArrayConst levelT[kLevelCount], levelZ[kLevelCount];
    for (int i = 0; i < kLevelCount; ++i) {
        char key[32];
        snprintf(key, sizeof key, "temperature_%dhPa", kLevels[i]);
        levelT[i] = h[key];
        snprintf(key, sizeof key, "geopotential_height_%dhPa", kLevels[i]);
        levelZ[i] = h[key];
    }
    JsonArrayConst groundT = h["temperature_2m"];
    JsonArrayConst windSpeed = h["wind_speed_700hPa"];
    JsonArrayConst windDir = h["wind_direction_700hPa"];
    JsonArrayConst snow = h["snowfall"];

    const float groundZ = doc["elevation"] | 0.0f;
    out.groundElevation = (int)lroundf(groundZ);
    out.hours.clear();
    out.hours.reserve(times.size());
    for (size_t i = 0; i < times.size(); ++i) {
        AlpineHour a;
        a.time = times[i] | 0;
        float z[kLevelCount], t[kLevelCount];
        for (int l = 0; l < kLevelCount; ++l) {
            z[l] = valueAt(levelZ[l], i);
            t[l] = valueAt(levelT[l], i);
        }
        a.freezingLevel = freezingLevel(groundZ, valueAt(groundT, i), z, t);
        a.windSpeed = valueAt(windSpeed, i);
        const float dir = valueAt(windDir, i);
        a.windDeg = isnan(dir) ? 0 : (int16_t)lroundf(dir);
        const float s = valueAt(snow, i);
        a.snowfall = isnan(s) ? 0 : s;
        out.hours.push_back(a);
    }
    out.valid = true;
    return true;
}

bool fetchAlpine(double lat, double lon, const char *model, Alpine &out, String &error, String *body) {
    char buf[96];
    snprintf(buf, sizeof buf, "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f", lat, lon);
    String url(buf);
    url += "&hourly=temperature_2m";
    for (int level : kLevels) url += ",temperature_" + String(level) + "hPa";
    for (int level : kLevels) url += ",geopotential_height_" + String(level) + "hPa";
    url += ",wind_speed_700hPa,wind_direction_700hPa,snowfall";
    url += "&models=";
    url += model;
    url += "&past_hours=72&forecast_hours=72&timeformat=unixtime&timezone=GMT";

    String response;
    if (!httpGet(url, response, error)) return false;
    if (!parseAlpine(response, out, error)) return false;
    out.fetched = time(nullptr);
    out.fromCache = false;
    if (body != nullptr) *body = response;
    return true;
}

}  // namespace weather
