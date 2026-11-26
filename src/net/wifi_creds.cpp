#include "wifi_creds.h"

#include <Preferences.h>

#include "app_config.h"

namespace wifi_creds {
namespace {

constexpr const char *kNamespace = "wifi";

}  // namespace

Creds load() {
    Creds c;
    Preferences p;
    if (p.begin(kNamespace, true)) {
        // isKey() first: getString() logs a missing key as an error.
        if (p.isKey("ssid")) c.ssid = p.getString("ssid", "");
        if (p.isKey("pass")) c.pass = p.getString("pass", "");
        p.end();
    }
    if (!c.ssid.isEmpty()) {
        c.saved = true;
    } else {
        c.ssid = WIFI_SSID;
        c.pass = WIFI_PASSWORD;
    }
    return c;
}

bool save(const String &ssid, const String &pass) {
    Preferences p;
    if (!p.begin(kNamespace, false)) return false;
    bool ok = p.putString("ssid", ssid) > 0;
    if (pass.isEmpty()) {
        if (p.isKey("pass")) p.remove("pass");  // open network
    } else {
        ok = ok && p.putString("pass", pass) > 0;
    }
    p.putString("okssid", ssid);
    p.end();
    log_i("Wi-Fi credentials saved for \"%s\"", ssid.c_str());
    return ok;
}

void forget() {
    Preferences p;
    if (!p.begin(kNamespace, false)) return;
    if (p.isKey("ssid")) p.remove("ssid");
    if (p.isKey("pass")) p.remove("pass");
    p.end();
    log_i("Saved Wi-Fi credentials erased");
}

bool confirmed(const String &ssid) {
    Preferences p;
    if (!p.begin(kNamespace, true)) return false;
    const String ok = p.isKey("okssid") ? p.getString("okssid", "") : String();
    p.end();
    return !ssid.isEmpty() && ok == ssid;
}

void markConfirmed(const String &ssid) {
    if (confirmed(ssid)) return;  // avoid an NVS write on every connect
    Preferences p;
    if (!p.begin(kNamespace, false)) return;
    p.putString("okssid", ssid);
    p.end();
}

void requestSetup() {
    Preferences p;
    if (!p.begin(kNamespace, false)) return;
    p.putBool("setup", true);
    p.end();
}

bool takeSetupRequest() {
    Preferences p;
    if (!p.begin(kNamespace, false)) return false;
    const bool req = p.getBool("setup", false);
    if (req) p.remove("setup");
    p.end();
    return req;
}

}  // namespace wifi_creds
