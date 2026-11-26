#pragma once

/**
 * Turns the screen (backlight) off overnight, from SCREEN_OFF_HOUR to
 * SCREEN_ON_HOUR local time (see config.h). A touch wakes it for
 * SCREEN_WAKE_MINUTES. Disabled when the two hours are equal, and inactive
 * until the clock has been set. Downloads continue while the screen is off.
 */
namespace screen_schedule {

/** Starts the check timer. Call once, with the display lock held. */
void start();

}  // namespace screen_schedule
