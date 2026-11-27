#include "ota.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <atomic>

#include "app_config.h"
#include "display/display_port.h"
#include "ui/ui.h"

namespace ota {
namespace {

std::atomic<bool> s_active{false};
int s_lastPercent = -1;

const char *errorText(ota_error_t e) {
    switch (e) {
        case OTA_AUTH_ERROR: return "Wrong OTA password";
        case OTA_BEGIN_ERROR: return "Could not start the update (image too large?)";
        case OTA_CONNECT_ERROR: return "Could not connect back to the uploading computer";
        case OTA_RECEIVE_ERROR: return "Connection lost while receiving";
        case OTA_END_ERROR: return "Image check failed";
    }
    return "Unknown error";
}

void setup() {
    ArduinoOTA.setHostname(OTA_HOSTNAME);  // also the mDNS name: <host>.local
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.setRebootOnSuccess(true);

    ArduinoOTA.onStart([] {
        s_active = true;
        s_lastPercent = -1;
        log_i("OTA update starting (%s)", ArduinoOTA.getCommand() == U_FLASH ? "firmware" : "filesystem");
        display::Lock lock;
        ui::showFirmwareUpdate(0);
    });
    ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
        const int percent = total > 0 ? static_cast<int>(done * 100ULL / total) : 0;
        // Every UI change redraws the full 800x480 frame, so only update in 2 % steps.
        if (percent / 2 == s_lastPercent / 2) return;
        s_lastPercent = percent;
        display::Lock lock;
        ui::showFirmwareUpdate(percent);
    });
    ArduinoOTA.onEnd([] {
        log_i("OTA update complete, restarting");
        display::Lock lock;
        ui::endFirmwareUpdate(nullptr);
    });
    ArduinoOTA.onError([](ota_error_t e) {
        const char *text = errorText(e);
        log_w("OTA update failed: %s", text);
        s_active = false;
        display::Lock lock;
        ui::endFirmwareUpdate(text);
    });
    ArduinoOTA.begin();
}

void task(void *) {
    while (WiFi.status() != WL_CONNECTED) delay(500);
    setup();
    log_i("OTA ready: %s.local (%s), port 3232", OTA_HOSTNAME, WiFi.localIP().toString().c_str());
    for (;;) {
        // Receives the whole image inside handle() once an upload starts.
        ArduinoOTA.handle();
        delay(50);
    }
}

}  // namespace

void start() {
    if (sizeof(OTA_PASSWORD) <= 1) {  // empty string literal
        log_i("OTA updates disabled (set OTA_PASSWORD in include/config.h)");
        return;
    }
    xTaskCreatePinnedToCore(task, "ota", 8 * 1024, nullptr, 1, nullptr, 0);
}

bool active() { return s_active; }

}  // namespace ota
