#include "updater.h"

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "app_config.h"
#include "display/display_port.h"
#include "forecast/forecast_client.h"
#include "ui/ui.h"

namespace updater {
namespace {

constexpr uint32_t kWifiConnectTimeoutMs = 20000;
constexpr uint32_t kTimeSyncTimeoutMs = 30000;
constexpr time_t kValidTime = 1700000000;  // anything before Nov 2023 means "clock not set"

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

void task(void *) {
    // Starts SNTP in the background; it keeps the clock synced from then on.
    configTzTime(TIME_ZONE, NTP_SERVER_1, NTP_SERVER_2);

    time_t shownIssued = 0;
    bool haveForecast = false;
    bool dailyCheck = false;  // true when the current fetch is a scheduled daily check
    int rechecks = 0;

    for (;;) {
        uint32_t waitMs = FORECAST_RETRY_MINUTES * 60UL * 1000UL;
        time_t next = 0;

        if (!ensureWifi()) {
            status("Wi-Fi connection failed - retrying", ui::StatusLevel::Error);
        } else if (!ensureTime()) {
            status("Clock sync failed - retrying", ui::StatusLevel::Error);
        } else {
            status("Downloading forecast...", ui::StatusLevel::Info);
            forecast::Forecast fc;
            fc.region = FORECAST_NAME;
            String error;
            if (!forecast::fetch(FORECAST_LATITUDE, FORECAST_LONGITUDE, FORECAST_LANGUAGE, fc, error)) {
                log_w("Forecast download failed: %s", error.c_str());
                status((haveForecast ? "Update failed: " : "Download failed: ") + error, ui::StatusLevel::Error);
            } else {
                const time_t now = time(nullptr);
                const bool changed = fc.issuedUtc != shownIssued;
                if (changed) {
                    shownIssued = fc.issuedUtc;
                    haveForecast = true;
                    display::Lock lock;
                    ui::showForecast(fc);
                }
                if (dailyCheck && !changed && rechecks < FORECAST_RECHECK_MAX) {
                    // Today's forecast isn't out yet; look again soon.
                    ++rechecks;
                    next = now + FORECAST_RECHECK_MINUTES * 60;
                } else {
                    rechecks = 0;
                    dailyCheck = true;
                    next = nextDailyCheck(now);
                }
                status("Checked " + hhmm(now) + ", next check " + hhmm(next), ui::StatusLevel::Ok);
                log_i("Forecast %s (issued %s). Free heap %u, PSRAM %u", changed ? "updated" : "unchanged",
                      fc.issued.c_str(), (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
            }
        }

        // Sleep until the next check (by wall clock when we have one).
        if (next != 0) {
            while (time(nullptr) < next) delay(1000);
        } else {
            delay(waitMs);
        }
    }
}

}  // namespace

void start() {
    xTaskCreatePinnedToCore(task, "updater", 16 * 1024, nullptr, 1, nullptr, 0);
}

}  // namespace updater
