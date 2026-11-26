#include "updater.h"

#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <time.h>

#include "app_config.h"
#include "display/display_port.h"
#include "forecast/forecast_client.h"
#include "ui/ui.h"
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

bool ensureWifi() {
    if (WiFi.status() == WL_CONNECTED) return true;
    status(String("Connecting to ") + WIFI_SSID + "...", ui::StatusLevel::Info);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < kWifiConnectTimeoutMs) {
        delay(250);
    }
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect();
        return false;
    }
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
    if (!forecast::fetch(FORECAST_LATITUDE, FORECAST_LONGITUDE, FORECAST_LANGUAGE, fc, error)) {
        log_w("Forecast download failed: %s", error.c_str());
        status((s_av.haveForecast ? "Update failed: " : "Download failed: ") + error, ui::StatusLevel::Error);
        if (!manual || scheduled == 0) s_av.next = now + FORECAST_RETRY_MINUTES * 60;
        return;
    }
    const bool changed = fc.issuedUtc != s_av.shownIssued;
    if (changed) {
        s_av.shownIssued = fc.issuedUtc;
        s_av.haveForecast = true;
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
    if (!weather::fetch(WEATHER_LATITUDE, WEATHER_LONGITUDE, OPENWEATHER_API_KEY, WEATHER_UNITS_METRIC,
                        FORECAST_LANGUAGE, w, error)) {
        log_w("Weather download failed: %s", error.c_str());
        display::Lock lock;
        ui::setWeatherError(("Weather update failed: " + error).c_str());
        s_weatherNext = now + FORECAST_RETRY_MINUTES * 60;
        return;
    }
    {
        display::Lock lock;
        ui::showWeather(w);
    }
    s_weatherNext = now + WEATHER_REFRESH_MINUTES * 60;
    log_i("Weather updated: %.1f, %s. Free heap %u", w.now.temp, w.now.description.c_str(),
          (unsigned)ESP.getFreeHeap());
}

void task(void *) {
    // Apply the time zone now so anything formatted before NTP sync is local.
    setenv("TZ", TIME_ZONE, 1);
    tzset();

    if (!kWeatherEnabled) {
        display::Lock lock;
        ui::setWeatherError("Add OPENWEATHER_API_KEY to include/config.h to show the weather.");
    }

    for (;;) {
        if (!ensureWifi()) {
            status("Wi-Fi connection failed - retrying", ui::StatusLevel::Error);
            waitOrRefresh(FORECAST_RETRY_MINUTES * 60UL * 1000UL);
            continue;
        }
        if (!ensureTime()) {
            status("Clock sync failed - retrying", ui::StatusLevel::Error);
            waitOrRefresh(FORECAST_RETRY_MINUTES * 60UL * 1000UL);
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
