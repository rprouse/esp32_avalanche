#pragma once

#include <Arduino.h>
#include <vector>

/**
 * Forecast data model, mapped from the Avalanche Canada point-forecast API
 * (see forecast_client.cpp). Text fields are plain text (HTML already stripped).
 */
namespace forecast {

// North American Public Avalanche Danger Scale
enum class Danger : uint8_t { NoRating = 0, Low, Moderate, Considerable, High, Extreme };

constexpr int kDays = 3;

struct DayRating {
    String label;  // e.g. "Thursday"
    Danger alpine = Danger::NoRating;
    Danger treeline = Danger::NoRating;
    Danger belowTreeline = Danger::NoRating;
};

struct Problem {
    String type;        // e.g. "Wind Slab"
    String elevations;  // e.g. "Alpine, Treeline"
    String aspects;     // e.g. "N, NE, E" or "All aspects"
    String likelihood;  // e.g. "Possible to likely"
    String size;        // e.g. "1 - 2"
    String comment;
};

struct Forecast {
    bool valid = false;
    String region;      // friendly name from config (e.g. "Fernie, BC")
    String title;       // API region title (list of sub-areas)
    String issued;      // formatted local time, e.g. "Wed Nov 25 16:00"
    String validUntil;  // formatted local time
    time_t issuedUtc = 0;
    DayRating days[kDays];
    std::vector<Problem> problems;
    String highlights;
    std::vector<String> travelAdvice;
    String avalancheSummary;
    String snowpackSummary;
    String weatherSummary;
    String confidence;  // e.g. "Moderate - The snowpack structure is well understood. ..."
};

const char *dangerName(Danger d);
uint32_t dangerColor(Danger d);      // 0xRRGGBB background
uint32_t dangerTextColor(Danger d);  // 0xRRGGBB foreground with good contrast

/** Placeholder shown before the first successful download. */
Forecast placeholder(const char *region);

}  // namespace forecast
