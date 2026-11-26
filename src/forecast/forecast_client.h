#pragma once

#include <Arduino.h>

#include "forecast.h"

/**
 * Downloads and parses the Avalanche Canada forecast for a point.
 *
 * Endpoint: https://api.avalanche.ca/forecasts/{lang}/products/point?lat={lat}&long={lon}
 * (~8 KB JSON, served via CloudFront). Requires Wi-Fi to be connected, and the
 * clock to be set (TLS certificate validation and local-time formatting need it).
 */
namespace forecast {

/**
 * Returns true on success. On failure, `error` holds a short description.
 * If `rawBody` is given it receives the JSON response (for caching).
 */
bool fetch(double lat, double lon, const char *lang, Forecast &out, String &error, String *rawBody = nullptr);

/** Parse an API response body. Exposed separately for testing. */
bool parse(const String &json, Forecast &out, String &error);

/** HTML -> plain text (paragraphs, <br>, lists, entities, common Unicode punctuation). */
String htmlToText(const char *html);

}  // namespace forecast
