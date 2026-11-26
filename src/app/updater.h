#pragma once

/**
 * Background task (core 0) that keeps Wi-Fi up, syncs the clock via NTP and
 * downloads the forecast on the schedule in config.h, pushing results to the UI.
 *
 * Schedule:
 *   - fetch at boot
 *   - then daily at FORECAST_REFRESH_HOUR:FORECAST_REFRESH_MINUTE (local time)
 *   - if the daily fetch returns the same forecast (not published yet), re-check
 *     every FORECAST_RECHECK_MINUTES, up to FORECAST_RECHECK_MAX times
 *   - on errors, retry every FORECAST_RETRY_MINUTES
 */
namespace updater {

void start();

}  // namespace updater
