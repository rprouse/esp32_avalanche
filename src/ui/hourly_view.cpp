#include "hourly_view.h"

#include <math.h>
#include <time.h>

namespace ui {
namespace {

// Layout (content area of the tab, absolute positions in px)
constexpr int32_t kDayY = 0;
constexpr int32_t kTimeY = 16;
constexpr int32_t kIconY = 36;
constexpr int32_t kTempTop = 100;     // highest temperature point
constexpr int32_t kTempBottom = 186;  // lowest temperature point
constexpr int32_t kBarBottom = 300;   // baseline of the precipitation bars
constexpr int32_t kBarMaxH = 80;
constexpr int32_t kPopY = 306;
constexpr int32_t kLegendY = 326;

constexpr uint32_t kTextColor = 0xD8E0E8;
constexpr uint32_t kMutedColor = 0x9AA8B8;
constexpr uint32_t kDayColor = 0x4FA3E0;
constexpr uint32_t kTempColor = 0xFFB347;
constexpr uint32_t kFreezeColor = 0x7FD1F5;
constexpr uint32_t kSnowColor = 0xE6F0FF;
constexpr uint32_t kRainColor = 0x3D9BE0;
constexpr uint32_t kDividerColor = 0x2E3B4B;

lv_obj_t *s_root = nullptr;
lv_obj_t *s_message = nullptr;
// lv_line keeps a pointer to its points, so they must outlive the widget.
lv_point_precise_t s_tempPts[weather::kHourSlots];
lv_point_precise_t s_freezePts[2];

lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    return l;
}

/** Label centred horizontally on x, top at y. */
void centredLabel(lv_obj_t *parent, const char *text, int32_t x, int32_t y, const lv_font_t *font,
                  uint32_t color) {
    lv_obj_t *l = label(parent, text, font, color);
    lv_obj_update_layout(l);
    lv_obj_set_pos(l, x - lv_obj_get_width(l) / 2, y);
}

lv_obj_t *box(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color, int32_t radius) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

String fmtAmount(float v, bool metric) {
    char buf[12];
    snprintf(buf, sizeof buf, metric ? "%.1f" : "%.2f", v);
    return buf;
}

int localDay(time_t t) {
    struct tm tm;
    localtime_r(&t, &tm);
    return tm.tm_yday;
}

}  // namespace

void buildHourlyTab(lv_obj_t *tab) {
    lv_obj_set_style_pad_hor(tab, 10, 0);
    lv_obj_set_style_pad_ver(tab, 8, 0);
    lv_obj_remove_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

    s_root = lv_obj_create(tab);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    s_message = label(tab, "Waiting for weather...", &lv_font_montserrat_24, kMutedColor);
    lv_obj_center(s_message);
}

void setHourlyMessage(const char *text) {
    if (s_message != nullptr && !lv_obj_has_flag(s_message, LV_OBJ_FLAG_HIDDEN)) lv_label_set_text(s_message, text);
}

void showHourly(const weather::Weather &w) {
    lv_obj_clean(s_root);
    const auto &hours = w.hours;
    if (hours.empty()) {
        lv_obj_remove_flag(s_message, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_message, "No hourly forecast available.");
        return;
    }
    lv_obj_add_flag(s_message, LV_OBJ_FLAG_HIDDEN);

    lv_obj_update_layout(s_root);
    const int32_t width = lv_obj_get_width(s_root);
    const int n = hours.size();
    const int32_t colW = width / weather::kHourSlots;
    auto colX = [&](int i) { return colW * i + colW / 2; };

    // ---- temperature scale
    float tMin = hours[0].temp, tMax = hours[0].temp;
    for (const auto &h : hours) {
        tMin = fminf(tMin, h.temp);
        tMax = fmaxf(tMax, h.temp);
    }
    const float span = fmaxf(tMax - tMin, 6.0f);  // keep small swings from looking dramatic
    const float mid = (tMax + tMin) / 2;
    const float top = mid + span / 2, bottom = mid - span / 2;
    auto tempY = [&](float t) { return kTempTop + (int32_t)lroundf((top - t) / span * (kTempBottom - kTempTop)); };

    // ---- precipitation scale (at least 5 mm / 0.2 in per 3 h so drizzle stays small)
    float pMax = w.metric ? 5.0f : 0.2f;
    for (const auto &h : hours) pMax = fmaxf(pMax, h.snow + h.rain);

    // ---- day dividers and weekday labels
    for (int i = 0; i < n; ++i) {
        const bool newDay = i == 0 || localDay(hours[i].time) != localDay(hours[i - 1].time);
        if (!newDay) continue;
        if (i > 0) box(s_root, colW * i - 1, kDayY, 2, kLegendY - 8, kDividerColor, 0);
        struct tm tm;
        localtime_r(&hours[i].time, &tm);
        char day[8];
        strftime(day, sizeof day, "%a", &tm);
        lv_obj_t *l = label(s_root, day, &lv_font_montserrat_14, kDayColor);
        lv_obj_set_pos(l, colW * i + 4, kDayY);
    }

    // ---- freezing line (0 °C / 32 °F) if it's within the plotted range
    const float freezing = w.metric ? 0.0f : 32.0f;
    if (freezing <= top && freezing >= bottom) {
        const int32_t y = tempY(freezing);
        s_freezePts[0] = {0, (lv_value_precise_t)y};
        s_freezePts[1] = {(lv_value_precise_t)width, (lv_value_precise_t)y};
        lv_obj_t *line = lv_line_create(s_root);
        lv_line_set_points(line, s_freezePts, 2);
        lv_obj_set_style_line_color(line, lv_color_hex(kFreezeColor), 0);
        lv_obj_set_style_line_width(line, 1, 0);
        lv_obj_set_style_line_dash_width(line, 6, 0);
        lv_obj_set_style_line_dash_gap(line, 6, 0);
        lv_obj_set_style_line_opa(line, LV_OPA_70, 0);
        lv_obj_t *l = label(s_root, w.metric ? "0\xC2\xB0" : "32\xC2\xB0", &lv_font_montserrat_14, kFreezeColor);
        lv_obj_set_pos(l, width - 28, y - 18);
    }

    // ---- temperature line
    for (int i = 0; i < n; ++i) {
        s_tempPts[i] = {(lv_value_precise_t)colX(i), (lv_value_precise_t)tempY(hours[i].temp)};
    }
    lv_obj_t *line = lv_line_create(s_root);
    lv_line_set_points(line, s_tempPts, n);
    lv_obj_set_style_line_color(line, lv_color_hex(kTempColor), 0);
    lv_obj_set_style_line_width(line, 3, 0);
    lv_obj_set_style_line_rounded(line, true, 0);

    // ---- per-slot column
    for (int i = 0; i < n; ++i) {
        const auto &h = hours[i];
        const int32_t x = colX(i);

        struct tm tm;
        localtime_r(&h.time, &tm);
        char hh[8];
        strftime(hh, sizeof hh, "%H:%M", &tm);
        centredLabel(s_root, hh, x, kTimeY, &lv_font_montserrat_14, kMutedColor);

        lv_obj_t *icon = lv_image_create(s_root);
        lv_image_set_src(icon, weather::tinyIcon(h.conditionId, h.night));
        lv_obj_set_pos(icon, x - 18, kIconY);

        const int32_t y = tempY(h.temp);
        box(s_root, x - 4, y - 4, 8, 8, kTempColor, LV_RADIUS_CIRCLE);
        char t[12];
        snprintf(t, sizeof t, "%ld\xC2\xB0", lroundf(h.temp));
        centredLabel(s_root, t, x, y - 24, &lv_font_montserrat_14, kTextColor);

        // Precipitation bars: snow at the bottom, rain stacked on top.
        const int32_t barW = colW - 18;
        const int32_t snowH = (int32_t)lroundf(h.snow / pMax * kBarMaxH);
        const int32_t rainH = (int32_t)lroundf(h.rain / pMax * kBarMaxH);
        if (snowH > 0) box(s_root, x - barW / 2, kBarBottom - snowH, barW, snowH, kSnowColor, 3);
        if (rainH > 0) box(s_root, x - barW / 2, kBarBottom - snowH - rainH, barW, rainH, kRainColor, 3);
        const float total = h.snow + h.rain;
        if (total >= (w.metric ? 0.1f : 0.01f)) {
            centredLabel(s_root, fmtAmount(total, w.metric).c_str(), x, kBarBottom - snowH - rainH - 18,
                         &lv_font_montserrat_14, kTextColor);
        }
        if (h.pop > 0) {
            char pop[8];
            snprintf(pop, sizeof pop, "%d%%", h.pop);
            centredLabel(s_root, pop, x, kPopY, &lv_font_montserrat_14, kMutedColor);
        }
    }

    // ---- baseline and legend
    box(s_root, 0, kBarBottom, width, 1, kDividerColor, 0);
    int32_t lx = 0;
    box(s_root, lx, kLegendY + 3, 12, 12, kSnowColor, 2);
    lv_obj_set_pos(label(s_root, "Snow", &lv_font_montserrat_14, kMutedColor), lx + 18, kLegendY);
    lx += 80;
    box(s_root, lx, kLegendY + 3, 12, 12, kRainColor, 2);
    lv_obj_set_pos(label(s_root, "Rain", &lv_font_montserrat_14, kMutedColor), lx + 18, kLegendY);
    lx += 74;
    lv_obj_set_pos(label(s_root, w.metric ? "mm per 3 h    % = chance of precipitation"
                                          : "in per 3 h    % = chance of precipitation",
                         &lv_font_montserrat_14, kMutedColor),
                   lx, kLegendY);
}

}  // namespace ui
