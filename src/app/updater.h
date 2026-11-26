#pragma once

/**
 * Background task (core 0) that keeps Wi-Fi up, syncs the clock via NTP and
 * downloads the avalanche forecast and the weather, pushing results to the UI.
 *
 * Weather: every WEATHER_REFRESH_MINUTES (if OPENWEATHER_API_KEY is set).
 *
 * Avalanche forecast schedule:
 *   - fetch at boot
 *   - then daily at FORECAST_REFRESH_HOUR:FORECAST_REFRESH_MINUTE (local time)
 *   - if the daily fetch returns the same forecast (not published yet), re-check
 *     every FORECAST_RECHECK_MINUTES, up to FORECAST_RECHECK_MAX times
 *   - on errors, retry every FORECAST_RETRY_MINUTES
 *   - requestRefresh() fetches both immediately without changing the daily schedule
 */
namespace updater {

void start();

/** Fetch the avalanche forecast and weather now (refresh button). Safe from any task. */
void requestRefresh();

}  // namespace updater
