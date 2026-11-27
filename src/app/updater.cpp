#include "updater.h"

#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <lvgl.h>  // LV_SYMBOL_REFRESH
#include <time.h>

#include "app/ota.h"
#include "app_config.h"
#include "display/display_port.h"
#include "forecast/forecast_client.h"
#include "net/wifi_creds.h"
#include "storage/cache.h"
#include "ui/ui.h"
#include "weather/alpine.h"
#include "weather/weather.h"

namespace updater {
namespace {

constexpr uint32_t kWifiConnectTimeoutMs = 20000;
constexpr uint32_t kTimeSyncTimeoutMs = 30000;
constexpr time_t kValidTime = 1700000000;  // anything before Nov 2023 means "clock not set"

TaskHandle_t s_task = nullptr;
std::atomic<bool> s_manualRefresh{false};

/** Sleep up to `ms`, waking early if requestRefresh() is called. */
void waitOrRefresh(uint32_t ms) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(ms));
}

void status(const String &text, ui::StatusLevel level) {
    log_i("%s", text.c_str());
    display::Lock lock;
    ui::setStatus(text.c_str(), level);
}

// Saved on the device (Wi-Fi setup mode) or from config.h. Loaded once by the task.
wifi_creds::Creds s_creds;
int s_wifiFailures = 0;
constexpr int kUnconfirmedFailureLimit = 3;

bool ensureWifi() {
    if (WiFi.status() == WL_CONNECTED) return true;
    status("Connecting to " + s_creds.ssid + "...", ui::StatusLevel::Info);
    WiFi.persistent(false);  // credentials live in our own NVS namespace (wifi_creds)
    WiFi.setHostname(OTA_HOSTNAME);  // DHCP name; must be set before mode()/begin()
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(s_creds.ssid.c_str(), s_creds.pass.isEmpty() ? nullptr : s_creds.pass.c_str());
    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < kWifiConnectTimeoutMs) {
        delay(250);
    }
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect();
        ++s_wifiFailures;
        // A network that has never worked here (e.g. a display given to a friend with
        // someone else's config.h) goes to setup mode. One that has worked never does,
        // so a router outage can't strand the display in setup.
        if (s_wifiFailures >= kUnconfirmedFailureLimit && !wifi_creds::confirmed(s_creds.ssid)) {
            log_w("Wi-Fi \"%s\" has never connected; starting Wi-Fi setup", s_creds.ssid.c_str());
            wifi_creds::requestSetup();
            esp_restart();
        }
        return false;
    }
    s_wifiFailures = 0;
    wifi_creds::markConfirmed(s_creds.ssid);
    log_i("Wi-Fi connected, IP %s, RSSI %d", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    return true;
}

bool ensureTime() {
    // Start SNTP only once Wi-Fi is up; started earlier, its first request fails
    // and the retry comes much later (~30 s). SNTP keeps the clock synced after this.
    static bool sntpStarted = false;
    if (!sntpStarted) {
        configTzTime(TIME_ZONE, NTP_SERVER_1, NTP_SERVER_2);
        sntpStarted = true;
    }
    if (time(nullptr) > kValidTime) return true;
    status("Setting clock...", ui::StatusLevel::Info);
    const uint32_t start = millis();
    while (time(nullptr) < kValidTime && millis() - start < kTimeSyncTimeoutMs) {
        delay(250);
    }
    return time(nullptr) > kValidTime;
}

time_t nextDailyCheck(time_t now) {
    struct tm tm;
    localtime_r(&now, &tm);
    tm.tm_hour = FORECAST_REFRESH_HOUR;
    tm.tm_min = FORECAST_REFRESH_MINUTE;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    time_t t = mktime(&tm);
    if (t <= now) {
        tm.tm_mday += 1;
        tm.tm_isdst = -1;
        t = mktime(&tm);
    }
    return t;
}

String hhmm(time_t t) {
    struct tm tm;
    localtime_r(&t, &tm);
    char buf[16];
    strftime(buf, sizeof buf, "%H:%M", &tm);
    return buf;
}

// Cache keys: a cached response is only reused for the same request settings.
String avalancheKey() {
    return String(FORECAST_LATITUDE, 4) + "," + String(FORECAST_LONGITUDE, 4) + "," + FORECAST_LANGUAGE;
}
String weatherKey() {
    return String(WEATHER_LATITUDE, 4) + "," + String(WEATHER_LONGITUDE, 4) + "," + FORECAST_LANGUAGE + "," +
           (WEATHER_UNITS_METRIC ? "metric" : "imperial");
}

String dateTime(time_t t) {
    struct tm tm;
    localtime_r(&t, &tm);
    char buf[32];
    strftime(buf, sizeof buf, "%a %b %d, %H:%M", &tm);
    return buf;
}

// ------------------------------------------------------------------ avalanche forecast

struct AvalancheState {
    time_t next = 0;  // 0 = due now
    time_t shownIssued = 0;
    bool haveForecast = false;
    bool dailyCheck = false;  // the pending fetch is a scheduled daily check
    int rechecks = 0;
} s_av;

// `manual` = triggered by the refresh button: fetch now, but keep the daily schedule.
void updateAvalanche(time_t now, bool manual) {
    const time_t scheduled = s_av.next;
    status(manual ? "Refreshing..." : "Downloading forecast...", ui::StatusLevel::Info);
    forecast::Forecast fc;
    fc.region = FORECAST_NAME;
    String error;
    String body;
    if (!forecast::fetch(FORECAST_LATITUDE, FORECAST_LONGITUDE, FORECAST_LANGUAGE, fc, error, &body)) {
        log_w("Forecast download failed: %s", error.c_str());
        status((s_av.haveForecast ? "Update failed: " : "Download failed: ") + error, ui::StatusLevel::Error);
        if (!manual || scheduled == 0) s_av.next = now + FORECAST_RETRY_MINUTES * 60;
        return;
    }
    const bool changed = fc.issuedUtc != s_av.shownIssued;
    if (changed) {
        s_av.shownIssued = fc.issuedUtc;
        s_av.haveForecast = true;
        cache::save("avalanche", now, avalancheKey(), body);  // only when it changes: ~1 write/day
        display::Lock lock;
        ui::showForecast(fc);
    }
    if (manual && scheduled != 0) {
        // Keep the existing schedule, unless this found the forecast we were re-checking for.
        if (changed && s_av.rechecks > 0) {
            s_av.rechecks = 0;
            s_av.next = nextDailyCheck(now);
        }
    } else if (s_av.dailyCheck && !changed && s_av.rechecks < FORECAST_RECHECK_MAX) {
        // Today's forecast isn't out yet; look again soon.
        ++s_av.rechecks;
        s_av.next = now + FORECAST_RECHECK_MINUTES * 60;
    } else {
        s_av.rechecks = 0;
        s_av.dailyCheck = true;
        s_av.next = nextDailyCheck(now);
    }
    status("Updated " + hhmm(now) + ", next check " + hhmm(s_av.next), ui::StatusLevel::Ok);
    log_i("Forecast %s (issued %s). Free heap %u, PSRAM %u", changed ? "updated" : "unchanged", fc.issued.c_str(),
          (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
}

// ------------------------------------------------------------------ weather

constexpr bool kWeatherEnabled = sizeof(OPENWEATHER_API_KEY) > 1;  // non-empty string literal
time_t s_weatherNext = 0;

void updateWeather(time_t now) {
    weather::Weather w;
    String error;
    String currentBody, forecastBody;
    if (!weather::fetch(WEATHER_LATITUDE, WEATHER_LONGITUDE, OPENWEATHER_API_KEY, WEATHER_UNITS_METRIC,
                        FORECAST_LANGUAGE, w, error, &currentBody, &forecastBody)) {
        log_w("Weather download failed: %s", error.c_str());
        display::Lock lock;
        ui::setWeatherError(("Weather update failed: " + error).c_str());
        s_weatherNext = now + FORECAST_RETRY_MINUTES * 60;
        return;
    }
    cache::save("wx_current", w.fetched, weatherKey(), currentBody);
    cache::save("wx_forecast", w.fetched, weatherKey(), forecastBody);
    {
        display::Lock lock;
        ui::showWeather(w);
    }
    s_weatherNext = now + WEATHER_REFRESH_MINUTES * 60;
    log_i("Weather updated: %.1f, %s. Free heap %u", w.now.temp, w.now.description.c_str(),
          (unsigned)ESP.getFreeHeap());
}

// ------------------------------------------------------------------ mountain weather (Open-Meteo)

constexpr bool kAlpineEnabled = ALPINE_WEATHER != 0;
time_t s_alpineNext = 0;

String alpineKey() {
    return weather::alpineKey(WEATHER_LATITUDE, WEATHER_LONGITUDE, ALPINE_MODEL);
}

void updateAlpine(time_t now) {
    weather::Alpine a;
    String error, body;
    if (!weather::fetchAlpine(WEATHER_LATITUDE, WEATHER_LONGITUDE, ALPINE_MODEL, a, error, &body)) {
        // Keep showing the last data; the next attempt comes sooner.
        log_w("Mountain weather download failed: %s", error.c_str());
        s_alpineNext = now + FORECAST_RETRY_MINUTES * 60;
        return;
    }
    a.metric = WEATHER_UNITS_METRIC;
    cache::save("alpine", a.fetched, alpineKey(), body);
    {
        display::Lock lock;
        ui::showAlpine(a);
    }
    s_alpineNext = now + WEATHER_REFRESH_MINUTES * 60;
    log_i("Mountain weather updated (%u hours). Free heap %u, largest block %u", (unsigned)a.hours.size(),
          (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
}

// Show whatever was saved before the last power-off, before touching the network.
void loadCached() {
    if (!cache::begin()) return;

    time_t fetched = 0;
    String body;
    if (cache::load("avalanche", avalancheKey(), fetched, body)) {
        forecast::Forecast fc;
        fc.region = FORECAST_NAME;
        String error;
        if (forecast::parse(body, fc, error)) {
            s_av.shownIssued = fc.issuedUtc;
            s_av.haveForecast = true;
            display::Lock lock;
            ui::showForecast(fc);
            ui::setStatus(("Saved forecast, downloaded " + dateTime(fetched)).c_str(), ui::StatusLevel::Info);
            log_i("Showing cached forecast (issued %s)", fc.issued.c_str());
        } else {
            log_w("Cached forecast unreadable: %s", error.c_str());
        }
    }

    if (kAlpineEnabled && cache::load("alpine", alpineKey(), fetched, body)) {
        weather::Alpine a;
        String error;
        if (weather::parseAlpine(body, a, error)) {
            a.metric = WEATHER_UNITS_METRIC;
            a.fetched = fetched;
            a.fromCache = true;
            display::Lock lock;
            ui::showAlpine(a);
            log_i("Showing cached mountain weather");
        } else {
            log_w("Cached mountain weather unreadable: %s", error.c_str());
        }
    }

    if (!kWeatherEnabled) return;
    String current, forecastBody;
    time_t fetchedCurrent = 0, fetchedForecast = 0;
    if (cache::load("wx_current", weatherKey(), fetchedCurrent, current) &&
        cache::load("wx_forecast", weatherKey(), fetchedForecast, forecastBody)) {
        weather::Weather w;
        String error;
        // Clock is usually not set yet after a power loss: group days relative to the download time.
        const time_t now = time(nullptr) > kValidTime ? time(nullptr) : fetchedForecast;
        if (weather::parse(current, forecastBody, WEATHER_UNITS_METRIC, now, w, error)) {
            w.fetched = fetchedCurrent;
            w.fromCache = true;
            display::Lock lock;
            ui::showWeather(w);
            log_i("Showing cached weather");
        } else {
            log_w("Cached weather unreadable: %s", error.c_str());
        }
    }
}

void task(void *) {
    // Apply the time zone now so anything formatted before NTP sync is local.
    setenv("TZ", TIME_ZONE, 1);
    tzset();

    if (!kWeatherEnabled) {
        display::Lock lock;
        ui::setWeatherError("Add OPENWEATHER_API_KEY to include/config.h to show the weather.");
    }
    loadCached();
    s_creds = wifi_creds::load();

    for (;;) {
        if (!ensureWifi()) {
            status(s_av.haveForecast ? String("Offline - showing saved forecast. Hold " LV_SYMBOL_REFRESH " to change Wi-Fi.")
                                     : "Can't connect to " + s_creds.ssid + ". Hold " LV_SYMBOL_REFRESH
                                       " to change Wi-Fi.",
                   ui::StatusLevel::Error);
            waitOrRefresh(FORECAST_RETRY_MINUTES * 60UL * 1000UL);
            continue;
        }
        if (!ensureTime()) {
            status("Clock sync failed - retrying", ui::StatusLevel::Error);
            waitOrRefresh(FORECAST_RETRY_MINUTES * 60UL * 1000UL);
            continue;
        }

        // Leave the network (and heap) to a firmware upload; it restarts the device when done.
        if (ota::active()) {
            waitOrRefresh(1000);
            continue;
        }

        const time_t now = time(nullptr);
        const bool manual = s_manualRefresh.exchange(false);
        if (manual) {
            display::Lock lock;
            ui::setRefreshing(true);
        }
        if (manual || now >= s_av.next) updateAvalanche(now, manual);
        if (kWeatherEnabled && (manual || now >= s_weatherNext)) updateWeather(time(nullptr));
        if (kAlpineEnabled && (manual || now >= s_alpineNext)) updateAlpine(time(nullptr));
        if (manual) {
            display::Lock lock;
            ui::setRefreshing(false);
        }

        waitOrRefresh(1000);
    }
}

}  // namespace

void start() {
    xTaskCreatePinnedToCore(task, "updater", 16 * 1024, nullptr, 1, &s_task, 0);
}

void requestRefresh() {
    s_manualRefresh = true;
    if (s_task != nullptr) xTaskNotifyGive(s_task);
}

}  // namespace updater
