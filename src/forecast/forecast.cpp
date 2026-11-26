#include "forecast.h"

namespace forecast {

const char *dangerName(Danger d) {
    switch (d) {
        case Danger::Low: return "1 - Low";
        case Danger::Moderate: return "2 - Moderate";
        case Danger::Considerable: return "3 - Considerable";
        case Danger::High: return "4 - High";
        case Danger::Extreme: return "5 - Extreme";
        default: return "No Rating";
    }
}

uint32_t dangerColor(Danger d) {
    switch (d) {
        case Danger::Low: return 0x50B848;
        case Danger::Moderate: return 0xFFF200;
        case Danger::Considerable: return 0xF7941E;
        case Danger::High: return 0xED1C24;
        case Danger::Extreme: return 0x231F20;
        default: return 0x9E9E9E;
    }
}

uint32_t dangerTextColor(Danger d) {
    return (d == Danger::High || d == Danger::Extreme) ? 0xFFFFFF : 0x000000;
}

Forecast placeholder(const char *region) {
    Forecast f;
    f.region = region;
    f.issued = "Waiting for forecast...";
    f.days[0].label = "Today";
    f.days[1].label = "Tomorrow";
    f.days[2].label = "Day 3";
    f.highlights = "Connecting to Wi-Fi and downloading the latest forecast from Avalanche Canada.";
    return f;
}

}  // namespace forecast
