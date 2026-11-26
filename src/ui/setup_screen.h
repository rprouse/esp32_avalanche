#pragma once

#include "ui/ui.h"

/**
 * Full-screen Wi-Fi setup page (issue #8), shown instead of the tabview while the
 * device runs its setup access point. Two QR codes: join the AP, then open the
 * setup page. Call with the display lock held.
 */
namespace setup_screen {

/** `onCancel` = nullptr hides the Cancel button (nothing to go back to). */
void build(const char *apSsid, const char *apPass, const char *url, void (*onCancel)());

void setStatus(const char *text, ui::StatusLevel level);

}  // namespace setup_screen
