#pragma once

/**
 * Over-the-air firmware updates (ArduinoOTA / espota, issue #3).
 *
 * Disabled unless OTA_PASSWORD is set in config.h. Once Wi-Fi connects, the
 * device listens on UDP 3232 and advertises itself over mDNS as
 * `<OTA_HOSTNAME>.local`. Upload with:
 *
 *     pio run -t upload --upload-port backcountry.local
 *
 * (tools/pio_ota.py switches to espota and adds the password from config.h.)
 * An update takes over the screen with a progress bar and restarts when done.
 */
namespace ota {

/** Starts the OTA task (core 0). It waits for Wi-Fi before listening. */
void start();

/** True while an update is being received; the updater skips downloads meanwhile. */
bool active();

}  // namespace ota
