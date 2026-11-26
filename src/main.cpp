#include <Arduino.h>

#include "app/screen_schedule.h"
#include "app/updater.h"
#include "app/wifi_setup.h"
#include "net/wifi_creds.h"
#include "app_config.h"
#include "display/display_port.h"
#include "forecast/forecast.h"
#include "ui/ui.h"

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("=== Avalanche forecast display ===");
    Serial.printf("PSRAM: %u bytes, free heap: %u bytes\n", (unsigned)ESP.getPsramSize(), (unsigned)ESP.getFreeHeap());

    if (!display::begin()) {
        Serial.println("Display init FAILED - halting");
        for (;;) delay(1000);
    }

    // Wi-Fi setup mode: nothing configured yet, or "Change Wi-Fi" was chosen (one-shot flag).
    const bool haveWifi = !wifi_creds::load().ssid.isEmpty();
    if (!haveWifi || wifi_creds::takeSetupRequest()) {
        Serial.println("Starting Wi-Fi setup mode");
        wifi_setup::start(haveWifi);
        return;
    }

    {
        display::Lock lock;
        ui::build(forecast::placeholder(FORECAST_NAME));
        screen_schedule::start();
    }
    Serial.println("UI ready");

    // Wi-Fi, NTP and forecast downloads run in their own task (core 0).
    updater::start();
    {
        display::Lock lock;
        ui::setRefreshCallback(updater::requestRefresh);
        ui::setWifiSetupCallback([] {
            wifi_creds::requestSetup();
            esp_restart();
        });
    }
}

void loop() {
    static uint32_t last = 0;
    if (millis() - last > 60000) {
        last = millis();
        Serial.printf("heap free: %u  psram free: %u\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
    }
    delay(100);
}
