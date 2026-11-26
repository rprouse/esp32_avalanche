#include "forecast_client.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <time.h>

#include "net/root_ca.h"

namespace forecast {
namespace {

constexpr uint32_t kHttpTimeoutMs = 15000;

Danger parseDanger(const char *v) {
    if (v == nullptr) return Danger::NoRating;
    if (!strcmp(v, "low")) return Danger::Low;
    if (!strcmp(v, "moderate")) return Danger::Moderate;
    if (!strcmp(v, "considerable")) return Danger::Considerable;
    if (!strcmp(v, "high")) return Danger::High;
    if (!strcmp(v, "extreme")) return Danger::Extreme;
    return Danger::NoRating;  // "norating", "early_season", "spring", ...
}

// Likelihood: text plus range on the 1 (unlikely) .. 5 (almost certain) scale.
void parseLikelihood(const char *v, Problem &p) {
    struct Map { const char *key, *text; float lo, hi; };
    static const Map kMap[] = {
        {"unlikely", "Unlikely", 1, 1},
        {"possible_unlikely", "Unlikely to possible", 1, 2},
        {"possible", "Possible", 2, 2},
        {"likely_possible", "Possible to likely", 2, 3},
        {"likely", "Likely", 3, 3},
        {"very_likely_likely", "Likely to very likely", 3, 4},
        {"very_likely", "Very likely", 4, 4},
        {"almost_certain_very_likely", "Very likely to almost certain", 4, 5},
        {"almost_certain", "Almost certain", 5, 5},
    };
    if (v == nullptr) return;
    for (const auto &m : kMap) {
        if (!strcmp(v, m.key)) {
            p.likelihood = m.text;
            p.likelihoodMin = m.lo;
            p.likelihoodMax = m.hi;
            return;
        }
    }
    String s(v);
    s.replace('_', ' ');
    if (s.length()) s.setCharAt(0, toupper(s[0]));
    p.likelihood = s;
}

int aspectIndex(const String &a) {
    static const char *const kNames[kAspects] = {"n", "ne", "e", "se", "s", "sw", "w", "nw"};
    for (int i = 0; i < kAspects; ++i) {
        if (a == kNames[i]) return i;
    }
    return -1;
}

int bandIndex(const String &b) {
    if (b == "alp") return kAlpine;
    if (b == "tln") return kTreeline;
    if (b == "btl") return kBelowTreeline;
    return -1;
}

double jsonNumber(JsonVariantConst v) {
    if (v.is<const char *>()) return atof(v.as<const char *>());  // API sends "1.0"
    return v.as<double>();
}

String formatSize(JsonVariantConst size) {
    const double mn = jsonNumber(size["min"]);
    const double mx = jsonNumber(size["max"]);
    char buf[24];
    if (mn <= 0 && mx <= 0) return "";
    if (mn == mx || mn <= 0) snprintf(buf, sizeof buf, "%g", mx);
    else snprintf(buf, sizeof buf, "%g - %g", mn, mx);
    return buf;
}

// Days since 1970-01-01 for a civil date (Howard Hinnant's algorithm).
int64_t daysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

// "2026-11-25T23:00:00.000Z" -> UTC epoch seconds. Returns 0 on failure.
time_t parseIsoUtc(const char *s) {
    int y, mo, d, h = 0, mi = 0, sec = 0;
    if (s == nullptr || sscanf(s, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &sec) < 3) return 0;
    return static_cast<time_t>(daysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60 + sec);
}

String formatLocal(time_t t) {
    if (t == 0) return "";
    struct tm tm;
    localtime_r(&t, &tm);
    char buf[32];
    strftime(buf, sizeof buf, "%a %b %d, %H:%M", &tm);
    return buf;
}

// Replace a few common Unicode characters the built-in fonts don't contain.
void appendUtf8(String &out, const char *&p) {
    const auto *u = reinterpret_cast<const uint8_t *>(p);
    if (u[0] == 0xE2 && u[1] == 0x80) {
        switch (u[2]) {
            case 0x98: case 0x99: out += '\''; p += 3; return;  // ‘ ’
            case 0x9C: case 0x9D: out += '"'; p += 3; return;   // “ ”
            case 0x93: case 0x94: out += '-'; p += 3; return;   // – —
            case 0xA6: out += "..."; p += 3; return;            // …
            case 0xA2: out += "\xE2\x80\xA2"; p += 3; return;   // • (in font)
        }
    }
    if (u[0] == 0xC2 && u[1] == 0xA0) { out += ' '; p += 2; return; }  // nbsp
    if (u[0] == 0xC3 && u[1] >= 0x80 && u[1] <= 0xBF) {
        // Latin-1 letters with accents -> base letter (fonts are ASCII + a few symbols).
        static const char kMap[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
        out += kMap[u[1] - 0x80];
        p += 2;
        return;
    }
    out += *p++;
}

void appendEntity(String &out, const char *&p) {
    struct Ent { const char *name; const char *text; };
    static const Ent kEnts[] = {{"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&#39;", "'"},
                                {"&apos;", "'"}, {"&nbsp;", " "}, {"&deg;", "\xC2\xB0"}, {"&ndash;", "-"},
                                {"&mdash;", "-"}, {"&rsquo;", "'"}, {"&lsquo;", "'"}, {"&ldquo;", "\""},
                                {"&rdquo;", "\""}, {"&hellip;", "..."}};
    for (const auto &e : kEnts) {
        const size_t n = strlen(e.name);
        if (!strncmp(p, e.name, n)) { out += e.text; p += n; return; }
    }
    out += *p++;
}

}  // namespace

String htmlToText(const char *html) {
    String out;
    if (html == nullptr) return out;
    out.reserve(strlen(html));
    const char *p = html;
    while (*p) {
        if (*p == '<') {
            const char *end = strchr(p, '>');
            if (end == nullptr) break;
            String tag(p + 1, end - p - 1);
            tag.toLowerCase();
            tag.trim();
            if (tag == "/p" || tag == "/ul" || tag == "/ol" || tag.startsWith("/h")) out += "\n\n";
            else if (tag.startsWith("br")) out += "\n";
            else if (tag == "li" || tag.startsWith("li ")) out += "\n\xE2\x80\xA2 ";
            p = end + 1;
        } else if (*p == '&') {
            appendEntity(out, p);
        } else if (*p == '\r' || *p == '\t') {
            out += ' ';
            ++p;
        } else if (static_cast<uint8_t>(*p) >= 0x80) {
            appendUtf8(out, p);
        } else {
            out += *p++;
        }
    }
    // Tidy whitespace: trim spaces around newlines, max one blank line.
    String clean;
    clean.reserve(out.length());
    int newlines = 0;
    bool pendingSpace = false;
    for (size_t i = 0; i < out.length(); ++i) {
        const char c = out[i];
        if (c == '\n') {
            pendingSpace = false;
            if (clean.length() && newlines < 2) clean += '\n';
            ++newlines;
        } else if (c == ' ') {
            pendingSpace = clean.length() > 0 && newlines == 0;
        } else {
            if (pendingSpace) clean += ' ';
            pendingSpace = false;
            newlines = 0;
            clean += c;
        }
    }
    clean.trim();
    return clean;
}

bool parse(const String &json, Forecast &out, String &error) {
    // Only keep the fields we display; the rest of the document is skipped while streaming.
    JsonDocument filter;
    JsonObject r = filter["report"].to<JsonObject>();
    r["dateIssued"] = true;
    r["validUntil"] = true;
    r["title"] = true;
    r["highlights"] = true;
    r["terrainAndTravelAdvice"] = true;
    r["confidence"]["rating"]["display"] = true;
    r["confidence"]["statements"] = true;
    r["summaries"][0]["type"]["value"] = true;
    r["summaries"][0]["content"] = true;
    JsonObject dr = r["dangerRatings"][0].to<JsonObject>();
    dr["date"]["display"] = true;
    for (const char *band : {"alp", "tln", "btl"}) dr["ratings"][band]["rating"]["value"] = true;
    JsonObject pr = r["problems"][0].to<JsonObject>();
    pr["type"]["display"] = true;
    pr["comment"] = true;
    pr["data"]["elevations"][0]["display"] = true;
    pr["data"]["aspects"][0]["value"] = true;
    pr["data"]["elevations"][0]["value"] = true;
    pr["data"]["aspectElevations"] = true;
    pr["data"]["likelihood"]["value"] = true;
    pr["data"]["expectedSize"] = true;

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, json, DeserializationOption::Filter(filter));
    if (err) {
        error = String("JSON: ") + err.c_str();
        return false;
    }
    JsonObjectConst report = doc["report"];
    if (report.isNull()) {
        error = "No forecast for this location";
        return false;
    }

    out.title = report["title"] | "";
    out.issuedUtc = parseIsoUtc(report["dateIssued"]);
    out.issued = formatLocal(out.issuedUtc);
    out.validUntil = formatLocal(parseIsoUtc(report["validUntil"]));
    out.highlights = htmlToText(report["highlights"]);

    out.travelAdvice.clear();
    for (JsonVariantConst a : report["terrainAndTravelAdvice"].as<JsonArrayConst>()) {
        out.travelAdvice.push_back(htmlToText(a.as<const char *>()));
    }

    out.confidence = report["confidence"]["rating"]["display"] | "";
    for (JsonVariantConst s : report["confidence"]["statements"].as<JsonArrayConst>()) {
        out.confidence += "\n\xE2\x80\xA2 " + htmlToText(s.as<const char *>());
    }

    for (JsonObjectConst s : report["summaries"].as<JsonArrayConst>()) {
        const char *type = s["type"]["value"] | "";
        String text = htmlToText(s["content"]);
        if (!strcmp(type, "avalanche-summary")) out.avalancheSummary = text;
        else if (!strcmp(type, "snowpack-summary")) out.snowpackSummary = text;
        else if (!strcmp(type, "weather-summary")) out.weatherSummary = text;
    }

    int day = 0;
    for (JsonObjectConst d : report["dangerRatings"].as<JsonArrayConst>()) {
        if (day >= kDays) break;
        DayRating &dr = out.days[day++];
        dr.label = d["date"]["display"] | "";
        dr.alpine = parseDanger(d["ratings"]["alp"]["rating"]["value"]);
        dr.treeline = parseDanger(d["ratings"]["tln"]["rating"]["value"]);
        dr.belowTreeline = parseDanger(d["ratings"]["btl"]["rating"]["value"]);
    }
    for (; day < kDays; ++day) out.days[day] = DayRating{};

    out.problems.clear();
    for (JsonObjectConst p : report["problems"].as<JsonArrayConst>()) {
        Problem prob;
        prob.type = p["type"]["display"] | "";
        prob.comment = htmlToText(p["comment"]);
        JsonObjectConst data = p["data"];
        for (JsonObjectConst e : data["elevations"].as<JsonArrayConst>()) {
            if (prob.elevations.length()) prob.elevations += ", ";
            prob.elevations += e["display"] | "";
        }
        JsonArrayConst aspects = data["aspects"];
        if (aspects.size() >= 8) {
            prob.aspects = "All aspects";
        } else {
            for (JsonObjectConst a : aspects) {
                String v = a["value"] | "";
                v.toUpperCase();
                if (prob.aspects.length()) prob.aspects += ", ";
                prob.aspects += v;
            }
        }
        parseLikelihood(data["likelihood"]["value"], prob);
        prob.size = formatSize(data["expectedSize"]);
        prob.sizeMin = jsonNumber(data["expectedSize"]["min"]);
        prob.sizeMax = jsonNumber(data["expectedSize"]["max"]);
        if (prob.sizeMin <= 0) prob.sizeMin = prob.sizeMax;

        // Affected aspect/elevation cells, e.g. "nw_alp". Fall back to every
        // combination of the listed aspects and elevations if absent.
        for (JsonVariantConst ae : data["aspectElevations"].as<JsonArrayConst>()) {
            const String s = ae.as<const char *>() ? ae.as<const char *>() : "";
            const int sep = s.indexOf('_');
            const int a = aspectIndex(s.substring(0, sep));
            const int b = bandIndex(s.substring(sep + 1));
            if (sep > 0 && a >= 0 && b >= 0) prob.aspectMask[b] |= 1 << a;
        }
        if (!prob.aspectMask[0] && !prob.aspectMask[1] && !prob.aspectMask[2]) {
            uint8_t aspectBits = 0;
            for (JsonObjectConst a : aspects) {
                const int i = aspectIndex(a["value"] | "");
                if (i >= 0) aspectBits |= 1 << i;
            }
            for (JsonObjectConst e : data["elevations"].as<JsonArrayConst>()) {
                const int b = bandIndex(e["value"] | "");
                if (b >= 0) prob.aspectMask[b] = aspectBits;
            }
        }
        out.problems.push_back(prob);
    }

    out.valid = true;
    return true;
}

bool fetch(double lat, double lon, const char *lang, Forecast &out, String &error, String *rawBody) {
    char url[160];
    snprintf(url, sizeof url, "https://api.avalanche.ca/forecasts/%s/products/point?lat=%.4f&long=%.4f", lang, lat,
             lon);

    NetworkClientSecure client;
    client.setCACert(net::kRootCAs);
    client.setTimeout(kHttpTimeoutMs);  // milliseconds (Stream::setTimeout)

    HTTPClient http;
    http.useHTTP10(true);  // keep the response simple (no chunked encoding)
    http.setTimeout(kHttpTimeoutMs);
    http.setUserAgent("esp32-avalanche-display/1.0");
    if (!http.begin(client, url)) {
        error = "HTTP begin failed";
        return false;
    }
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        error = code < 0 ? String("HTTP: ") + HTTPClient::errorToString(code) : String("HTTP ") + code;
        http.end();
        return false;
    }
    // The response (~8 KB) has no Content-Length; the server just closes the
    // connection. getString() reads until close, which is more reliable than
    // streaming from the TLS client into the parser.
    const String body = http.getString();
    http.end();
    if (body.isEmpty()) {
        error = "Empty response";
        return false;
    }
    if (!parse(body, out, error)) return false;
    if (rawBody != nullptr) *rawBody = body;
    return true;
}

}  // namespace forecast
