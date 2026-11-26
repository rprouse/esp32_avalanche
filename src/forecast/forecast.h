#pragma once

#include <Arduino.h>
#include <vector>

/**
 * Forecast data model. Shaped after the Avalanche Canada public forecast
 * (danger ratings by elevation band for 3 days, avalanche problems, highlights).
 *
 * TODO: populate from the Avalanche Canada API instead of sample().
 */
namespace forecast {

// North American Public Avalanche Danger Scale
enum class Danger : uint8_t { NoRating = 0, Low, Moderate, Considerable, High, Extreme };

constexpr int kDays = 3;

struct DayRating {
    String label;  // e.g. "Today", "Wed"
    Danger alpine = Danger::NoRating;
    Danger treeline = Danger::NoRating;
    Danger belowTreeline = Danger::NoRating;
};

struct Problem {
    String type;        // e.g. "Wind Slab"
    String elevations;  // e.g. "Alpine, Treeline"
    String aspects;     // e.g. "N, NE, E"
    String likelihood;  // e.g. "Likely"
    String size;        // e.g. "1 - 2"
    String comment;
};

struct Forecast {
    String region;
    String issued;
    String validUntil;
    DayRating days[kDays];
    std::vector<Problem> problems;
    String highlights;
    String confidence;
};

const char *dangerName(Danger d);
uint32_t dangerColor(Danger d);      // 0xRRGGBB background
uint32_t dangerTextColor(Danger d);  // 0xRRGGBB foreground with good contrast
Danger nextDanger(Danger d);         // cycles through levels (used by the touch demo)

/** Placeholder forecast so the UI can be built before the network layer exists. */
Forecast sample();

}  // namespace forecast
