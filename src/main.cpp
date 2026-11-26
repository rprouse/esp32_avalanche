#include <Arduino.h>

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
        ui::build(forecast::sample());
    }

    Serial.println("UI ready");
}

void loop() {
    // LVGL runs in its own task (see display_port.cpp). Future: Wi-Fi, NTP and the
    // daily forecast download go here or in their own task; update the UI with
    //   { display::Lock lock; ui::showForecast(fc); }
    static uint32_t last = 0;
    if (millis() - last > 10000) {
        last = millis();
        Serial.printf("heap free: %u  psram free: %u\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
    }
    delay(50);
}
