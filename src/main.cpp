#include <Arduino.h>

#include "app/updater.h"
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

    {
        display::Lock lock;
        ui::build(forecast::placeholder(FORECAST_NAME));
    }
    Serial.println("UI ready");

    // Wi-Fi, NTP and forecast downloads run in their own task (core 0).
    updater::start();
    {
        display::Lock lock;
        ui::setRefreshCallback(updater::requestRefresh);
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
