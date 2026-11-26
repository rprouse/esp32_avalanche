#pragma once

/**
 * Wi-Fi setup mode (issue #8). Runs instead of the updater when no Wi-Fi is
 * configured, or after "Change Wi-Fi" (long-press refresh).
 *
 * - Starts a WPA2 access point "Backcountry-XXXX" with a random password shown on
 *   screen (with QR codes), a DNS catch-all and a captive-portal web page.
 * - The page lists scanned networks. On submit the device tries the network
 *   (AP+STA) and only saves it (wifi_creds) if it connects, then restarts.
 * - With credentials already saved, the screen has a Cancel button and setup
 *   restarts into normal mode after 10 idle minutes.
 *
 * Call start() from setup() after display::begin(), instead of ui::build().
 */
namespace wifi_setup {

void start(bool haveCredentials);

}  // namespace wifi_setup
