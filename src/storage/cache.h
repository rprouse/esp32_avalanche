#pragma once

#include <Arduino.h>

/**
 * Small persistent cache of raw API responses on the LittleFS partition
 * ("spiffs" in default_16MB.csv, 3.4 MB), so the last forecast and weather can be
 * shown immediately after a power loss, before (or without) Wi-Fi.
 *
 * File format: "<fetched epoch> <key>\n" followed by the body. `key` identifies the
 * request (location/language/units); load() ignores entries whose key differs, so
 * a config change never shows data for the wrong place. Writes go to a temp file
 * that is then renamed over the old one, so a power cut can't leave a torn file.
 */
namespace cache {

/** Mount LittleFS (formats the partition on first use). */
bool begin();

bool save(const char *name, time_t fetched, const String &key, const String &body);

/** Returns false if missing, unreadable or saved with a different key. */
bool load(const char *name, const String &key, time_t &fetched, String &body);

}  // namespace cache
