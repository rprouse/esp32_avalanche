#include "cache.h"

#include <LittleFS.h>

namespace cache {
namespace {

bool s_mounted = false;

String pathFor(const char *name, const char *ext) { return String("/") + name + ext; }

}  // namespace

bool begin() {
    if (s_mounted) return true;
    // true = format if mount fails (first boot / blank partition).
    s_mounted = LittleFS.begin(true);
    if (s_mounted) {
        log_i("Cache: LittleFS mounted, %u/%u bytes used", (unsigned)LittleFS.usedBytes(),
              (unsigned)LittleFS.totalBytes());
    } else {
        log_e("Cache: LittleFS mount failed");
    }
    return s_mounted;
}

bool save(const char *name, time_t fetched, const String &key, const String &body) {
    if (!s_mounted) return false;
    const String tmp = pathFor(name, ".tmp");
    const String dst = pathFor(name, ".cache");
    File f = LittleFS.open(tmp, "w");
    if (!f) {
        log_w("Cache: can't write %s", tmp.c_str());
        return false;
    }
    const String header = String((long long)fetched) + " " + key + "\n";
    bool ok = f.print(header) == header.length();
    ok = ok && f.write(reinterpret_cast<const uint8_t *>(body.c_str()), body.length()) == body.length();
    f.close();
    if (!ok) {
        log_w("Cache: short write for %s", name);
        LittleFS.remove(tmp);
        return false;
    }
    // LittleFS rename atomically replaces the destination.
    if (!LittleFS.rename(tmp, dst)) {
        log_w("Cache: rename failed for %s", name);
        LittleFS.remove(tmp);
        return false;
    }
    log_i("Cache: saved %s (%u bytes)", name, (unsigned)body.length());
    return true;
}

bool load(const char *name, const String &key, time_t &fetched, String &body) {
    if (!s_mounted) return false;
    File f = LittleFS.open(pathFor(name, ".cache"), "r");
    if (!f) return false;

    const String header = f.readStringUntil('\n');
    const int space = header.indexOf(' ');
    if (space <= 0 || header.substring(space + 1) != key) {
        log_i("Cache: %s ignored (missing header or different location/settings)", name);
        f.close();
        return false;
    }
    fetched = (time_t)atoll(header.substring(0, space).c_str());

    body = String();
    body.reserve(f.size());
    char buf[512];
    int n;
    while ((n = f.read(reinterpret_cast<uint8_t *>(buf), sizeof buf)) > 0) body.concat(buf, n);
    f.close();
    return fetched > 0 && body.length() > 0;
}

}  // namespace cache
