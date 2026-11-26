#pragma once

#include <lvgl.h>

/**
 * Avalanche problem graphics, drawn natively for the dark theme (instead of
 * Avalanche Canada's PNGs, which are designed for a white background).
 * Must be called with the display lock held (like the rest of ui::).
 */
namespace ui {

/**
 * Aspect/elevation rose: 8 aspects (N at top, clockwise) x 3 rings
 * (centre = alpine, middle = treeline, outer = below treeline).
 * mask[band] bit i = aspect i affected (see forecast::Problem::aspectMask).
 * Returns a `size` x `size` object including the N/E/S/W letters.
 */
lv_obj_t *createAspectRose(lv_obj_t *parent, const uint8_t mask[3], int32_t size);

/**
 * Horizontal 5-step scale with the range lo..hi (1..5, half steps allowed) filled.
 * `labels` are drawn under the steps (first left-aligned, last right-aligned,
 * others centred); nullptr or "" entries are skipped.
 */
lv_obj_t *createRangeScale(lv_obj_t *parent, float lo, float hi, int32_t width, const char *const labels[5]);

}  // namespace ui
