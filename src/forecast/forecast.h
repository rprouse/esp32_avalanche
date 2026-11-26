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
    time_t date = 0;  // UTC epoch within that day (API gives 23:00Z = 16:00 Pacific)
    Danger alpine = Danger::NoRating;
    Danger treeline = Danger::NoRating;
    Danger belowTreeline = Danger::NoRating;
};

// Elevation bands and aspects, used to index Problem::aspectMask.
enum Band : uint8_t { kAlpine = 0, kTreeline = 1, kBelowTreeline = 2, kBands = 3 };
constexpr int kAspects = 8;  // N, NE, E, SE, S, SW, W, NW (bit 0 = N, clockwise)

struct Problem {
    String type;        // e.g. "Wind Slab"
    String elevations;  // e.g. "Alpine, Treeline"
    String aspects;     // e.g. "N, NE, E" or "All aspects"
    String likelihood;  // e.g. "Possible to likely"
    String size;        // e.g. "1 - 2"
    String comment;

    // For the graphics: which aspect/elevation combinations are affected
    // (aspectMask[band] bit i = aspect i), and likelihood/size ranges on 1..5.
    uint8_t aspectMask[kBands] = {0, 0, 0};
    float likelihoodMin = 0, likelihoodMax = 0;  // 1 unlikely .. 5 almost certain
    float sizeMin = 0, sizeMax = 0;              // destructive size 1 .. 5 (half sizes allowed)
};

struct Forecast {
    bool valid = false;
    String region;      // friendly name from config (e.g. "Fernie, BC")
    String title;       // API region title (list of sub-areas)
    String issued;      // formatted local time, e.g. "Wed Nov 25 16:00"
    String validUntil;  // formatted local time
    time_t issuedUtc = 0;
    time_t validUntilUtc = 0;
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
