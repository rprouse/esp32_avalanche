#pragma once

#include <Arduino.h>

/**
 * Wi-Fi credentials, set on the device in Wi-Fi setup mode (issue #8).
 *
 * Saved in NVS (Preferences namespace "wifi"). When nothing is saved, falls back
 * to WIFI_SSID / WIFI_PASSWORD from config.h (both optional). Everything else
 * (location, API keys, ...) stays compile-time in config.h.
 *
 * NVS keys: ssid, pass, okssid (the SSID that last connected successfully), setup
 * (enter setup mode on the next boot). Never log the password.
 */
namespace wifi_creds {

struct Creds {
    String ssid;
    String pass;
    bool saved = false;  // true = from NVS, false = from config.h
};

Creds load();

/** Save new credentials (they've just been tested) and mark them as confirmed. */
bool save(const String &ssid, const String &pass);

/** Erase saved credentials; the config.h values (if any) apply again. */
void forget();

/** True if `ssid` has connected successfully at least once. */
bool confirmed(const String &ssid);
void markConfirmed(const String &ssid);

/** Boot into Wi-Fi setup mode next time (one-shot: takeSetupRequest() clears it). */
void requestSetup();
bool takeSetupRequest();

}  // namespace wifi_creds
