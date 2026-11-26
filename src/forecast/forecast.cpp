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

Danger nextDanger(Danger d) {
    return d == Danger::Extreme ? Danger::Low : static_cast<Danger>(static_cast<uint8_t>(d) + 1);
}

Forecast sample() {
    Forecast f;
    f.region = "Sample Region";
    f.issued = "Sample data - not live";
    f.validUntil = "";
    f.days[0] = {"Today", Danger::Considerable, Danger::Moderate, Danger::Low};
    f.days[1] = {"Tomorrow", Danger::Considerable, Danger::Considerable, Danger::Moderate};
    f.days[2] = {"Day 3", Danger::Moderate, Danger::Moderate, Danger::Low};
    f.problems = {
        {"Wind Slab", "Alpine, Treeline", "N, NE, E, SE", "Likely", "1 - 2",
         "Recent strong SW winds have built reactive slabs on lee features."},
        {"Persistent Slab", "Alpine, Treeline", "All aspects", "Possible", "2 - 3",
         "A buried weak layer remains a concern in thin, rocky areas."},
    };
    f.highlights =
        "This is placeholder text. The real forecast will be downloaded daily from "
        "Avalanche Canada and shown here.";
    f.confidence = "Moderate";
    return f;
}

}  // namespace forecast
