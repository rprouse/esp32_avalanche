#include "ui.h"

#include <cmath>
#include <lvgl.h>
#include <time.h>

#include "display/display_port.h"
#include "ui/logo_icon.h"
#include "weather/weather.h"

using forecast::Danger;

namespace ui {
namespace {

constexpr int32_t kHeaderH = 56;
constexpr int32_t kTabBarH = 64;
constexpr uint32_t kBgColor = 0x10161F;
constexpr uint32_t kPanelColor = 0x1C2633;
constexpr uint32_t kAccentColor = 0x4FA3E0;
constexpr uint32_t kMutedColor = 0x9AA8B8;
constexpr uint32_t kBodyColor = 0xD8E0E8;

lv_obj_t *s_regionLabel = nullptr;
lv_obj_t *s_issuedLabel = nullptr;
lv_obj_t *s_statusLabel = nullptr;
lv_obj_t *s_dangerTab = nullptr;
lv_obj_t *s_dangerGrid = nullptr;  // rebuilt on each forecast; the footer below it is persistent
lv_obj_t *s_refreshBtn = nullptr;
void (*s_refreshCb)() = nullptr;
lv_obj_t *s_problemsTab = nullptr;
lv_obj_t *s_conditionsTab = nullptr;
lv_obj_t *s_detailsTab = nullptr;

// ---------------------------------------------------------------- helpers

lv_obj_t *makeLabel(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color = 0xFFFFFF) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    return l;
}

lv_obj_t *makeWrappedLabel(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color) {
    lv_obj_t *l = makeLabel(parent, text, font, color);
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    return l;
}

lv_obj_t *makePanel(lv_obj_t *parent) {
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_set_style_bg_color(p, lv_color_hex(kPanelColor), 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_radius(p, 10, 0);
    lv_obj_set_style_pad_all(p, 12, 0);
    lv_obj_set_size(p, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(p, 6, 0);
    lv_obj_remove_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

/** Card with a heading and wrapped body text. Skipped if body is empty. */
void addTextCard(lv_obj_t *parent, const char *heading, const String &body) {
    if (body.isEmpty()) return;
    lv_obj_t *card = makePanel(parent);
    makeLabel(card, heading, &lv_font_montserrat_24, kAccentColor);
    makeWrappedLabel(card, body.c_str(), &lv_font_montserrat_20, kBodyColor);
}

void prepareScrollTab(lv_obj_t *tab) {
    lv_obj_clean(tab);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab, 12, 0);
    lv_obj_set_style_pad_row(tab, 12, 0);
    lv_obj_scroll_to_y(tab, 0, LV_ANIM_OFF);
}

void styleDangerCell(lv_obj_t *cell, Danger d) {
    lv_obj_set_style_bg_color(cell, lv_color_hex(forecast::dangerColor(d)), 0);
    lv_obj_t *label = lv_obj_get_child(cell, 0);
    lv_label_set_text(label, forecast::dangerName(d));
    lv_obj_set_style_text_color(label, lv_color_hex(forecast::dangerTextColor(d)), 0);
}

// ---------------------------------------------------------------- Danger tab

void buildDangerTab(const forecast::Forecast &fc) {
    static const int32_t cols[] = {200, LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const int32_t rows[] = {40, LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

    lv_obj_t *grid = s_dangerGrid;
    lv_obj_clean(grid);
    lv_obj_set_grid_dsc_array(grid, cols, rows);

    for (int day = 0; day < forecast::kDays; ++day) {
        lv_obj_t *h = makeLabel(grid, fc.days[day].label.c_str(), &lv_font_montserrat_24);
        lv_obj_set_grid_cell(h, LV_GRID_ALIGN_CENTER, day + 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    }

    static const char *bands[] = {"Alpine", "Treeline", "Below Treeline"};
    for (int band = 0; band < 3; ++band) {
        lv_obj_t *bl = makeLabel(grid, bands[band], &lv_font_montserrat_24);
        lv_obj_set_grid_cell(bl, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, band + 1, 1);

        for (int day = 0; day < forecast::kDays; ++day) {
            const auto &r = fc.days[day];
            const Danger d = band == 0 ? r.alpine : (band == 1 ? r.treeline : r.belowTreeline);
            lv_obj_t *cell = lv_obj_create(grid);
            lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_STRETCH, day + 1, 1, LV_GRID_ALIGN_STRETCH, band + 1, 1);
            lv_obj_set_style_border_width(cell, 0, 0);
            lv_obj_set_style_radius(cell, 8, 0);
            lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_remove_flag(cell, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_t *l = makeLabel(cell, "", &lv_font_montserrat_20);
            lv_obj_center(l);
            styleDangerCell(cell, d);
        }
    }
}

/** Danger tab skeleton: grid (filled by buildDangerTab) + footer with issued time and status. */
void buildDangerSkeleton() {
    lv_obj_set_style_pad_all(s_dangerTab, 12, 0);
    lv_obj_set_style_pad_row(s_dangerTab, 8, 0);
    lv_obj_set_flex_flow(s_dangerTab, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(s_dangerTab, LV_OBJ_FLAG_SCROLLABLE);

    s_dangerGrid = lv_obj_create(s_dangerTab);
    lv_obj_set_width(s_dangerGrid, LV_PCT(100));
    lv_obj_set_flex_grow(s_dangerGrid, 1);
    lv_obj_set_style_bg_opa(s_dangerGrid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_dangerGrid, 0, 0);
    lv_obj_set_style_pad_all(s_dangerGrid, 0, 0);
    lv_obj_set_style_pad_gap(s_dangerGrid, 8, 0);
    lv_obj_remove_flag(s_dangerGrid, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *footer = lv_obj_create(s_dangerTab);
    lv_obj_set_size(footer, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_set_style_pad_all(footer, 0, 0);
    lv_obj_remove_flag(footer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    s_issuedLabel = makeLabel(footer, "", &lv_font_montserrat_14, kBodyColor);
    s_statusLabel = makeLabel(footer, "", &lv_font_montserrat_14, kMutedColor);
}

// ---------------------------------------------------------------- Problems tab

void buildProblemsTab(const forecast::Forecast &fc) {
    prepareScrollTab(s_problemsTab);

    if (fc.problems.empty()) {
        makeLabel(s_problemsTab, fc.valid ? "No avalanche problems listed." : "", &lv_font_montserrat_24);
        return;
    }
    for (const auto &p : fc.problems) {
        lv_obj_t *card = makePanel(s_problemsTab);
        makeLabel(card, p.type.c_str(), &lv_font_montserrat_28, kAccentColor);
        String facts = "Elevation: " + p.elevations + "\nAspects: " + p.aspects + "\nLikelihood: " + p.likelihood +
                       "\nExpected size: " + p.size;
        makeLabel(card, facts.c_str(), &lv_font_montserrat_20);
        makeWrappedLabel(card, p.comment.c_str(), &lv_font_montserrat_20, kBodyColor);
    }
}

// ---------------------------------------------------------------- Conditions tab

void buildConditionsTab(const forecast::Forecast &fc) {
    prepareScrollTab(s_conditionsTab);
    addTextCard(s_conditionsTab, "Avalanche Summary", fc.avalancheSummary);
    addTextCard(s_conditionsTab, "Snowpack Summary", fc.snowpackSummary);
    addTextCard(s_conditionsTab, "Weather Summary", fc.weatherSummary);
}

// ---------------------------------------------------------------- Details tab

void buildDetailsTab(const forecast::Forecast &fc) {
    prepareScrollTab(s_detailsTab);
    addTextCard(s_detailsTab, "Highlights", fc.highlights);

    String advice;
    for (const auto &a : fc.travelAdvice) {
        if (advice.length()) advice += "\n";
        advice += "\xE2\x80\xA2 " + a;
    }
    addTextCard(s_detailsTab, "Terrain and Travel Advice", advice);
    addTextCard(s_detailsTab, "Confidence", fc.confidence);
    addTextCard(s_detailsTab, "Region", fc.title);
}

// ---------------------------------------------------------------- Weather tab
//
// Left card: current conditions. Right card: daily forecast rows.
// Built once; showWeather() updates labels/images and rebuilds the day rows.

constexpr uint32_t kLowTempColor = 0x7FD1F5;

struct WeatherUi {
    lv_obj_t *message;  // shown until the first successful download
    lv_obj_t *content;
    lv_obj_t *icon;
    lv_obj_t *temp;
    lv_obj_t *desc;
    lv_obj_t *feels;
    lv_obj_t *updated;
    lv_obj_t *windIcon;
    lv_obj_t *wind;
    lv_obj_t *gust;
    lv_obj_t *stats;
    lv_obj_t *moonIcon;
    lv_obj_t *moon;
    lv_obj_t *sun;
    lv_obj_t *days;
    bool valid;
} s_wx;

lv_obj_t *makeBox(lv_obj_t *parent, lv_flex_flow_t flow) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_flex_flow(o, flow);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(o, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    return o;
}

String fmtTemp(float t, bool unit, bool metric) {
    long v = lroundf(t);
    String s = String(v) + "\xC2\xB0";
    if (unit) s += metric ? "C" : "F";
    return s;
}

String fmtClock(time_t t) {
    struct tm tm;
    localtime_r(&t, &tm);
    char buf[8];
    strftime(buf, sizeof buf, "%H:%M", &tm);
    return buf;
}

String fmtPrecip(float v, bool metric) {
    char buf[16];
    if (metric) snprintf(buf, sizeof buf, v < 10 ? "%.1f mm" : "%.0f mm", v);
    else snprintf(buf, sizeof buf, "%.2f in", v);
    return buf;
}

void buildWeatherTab(lv_obj_t *tab) {
    lv_obj_set_style_pad_all(tab, 12, 0);
    lv_obj_remove_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

    s_wx.message = makeWrappedLabel(tab, "Waiting for weather...", &lv_font_montserrat_24, kMutedColor);
    lv_obj_set_width(s_wx.message, LV_PCT(80));
    lv_obj_set_style_text_align(s_wx.message, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_wx.message);

    s_wx.content = makeBox(tab, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(s_wx.content, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_column(s_wx.content, 12, 0);
    lv_obj_add_flag(s_wx.content, LV_OBJ_FLAG_HIDDEN);

    // ---- current conditions card
    lv_obj_t *now = makePanel(s_wx.content);
    lv_obj_set_size(now, 340, LV_PCT(100));
    lv_obj_set_style_pad_row(now, 10, 0);

    lv_obj_t *top = makeBox(now, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(top, 10, 0);
    s_wx.icon = lv_image_create(top);
    lv_obj_t *col = makeBox(top, LV_FLEX_FLOW_COLUMN);
    s_wx.temp = makeLabel(col, "", &lv_font_montserrat_40);
    s_wx.desc = makeLabel(col, "", &lv_font_montserrat_20, kAccentColor);
    s_wx.feels = makeLabel(col, "", &lv_font_montserrat_14, kBodyColor);
    s_wx.updated = makeLabel(col, "", &lv_font_montserrat_14, kMutedColor);

    lv_obj_t *windRow = makeBox(now, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(windRow, 10, 0);
    lv_obj_set_flex_align(windRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    s_wx.windIcon = lv_image_create(windRow);
    lv_obj_t *windCol = makeBox(windRow, LV_FLEX_FLOW_COLUMN);
    s_wx.wind = makeLabel(windCol, "", &lv_font_montserrat_20);
    s_wx.gust = makeLabel(windCol, "", &lv_font_montserrat_14, kBodyColor);

    s_wx.stats = makeLabel(now, "", &lv_font_montserrat_14, kBodyColor);
    lv_obj_set_style_text_line_space(s_wx.stats, 4, 0);

    lv_obj_t *skyRow = makeBox(now, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(skyRow, 10, 0);
    lv_obj_set_flex_align(skyRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    s_wx.moonIcon = lv_image_create(skyRow);
    s_wx.moon = makeLabel(skyRow, "", &lv_font_montserrat_14, kBodyColor);
    s_wx.sun = makeLabel(skyRow, "", &lv_font_montserrat_14, kBodyColor);
    lv_obj_set_style_pad_left(s_wx.sun, 8, 0);

    // ---- daily forecast card
    s_wx.days = makePanel(s_wx.content);
    lv_obj_set_height(s_wx.days, LV_PCT(100));
    lv_obj_set_flex_grow(s_wx.days, 1);
    lv_obj_set_style_pad_row(s_wx.days, 4, 0);
    lv_obj_set_style_pad_ver(s_wx.days, 6, 0);
}

void addDayRow(const weather::Day &d, bool metric) {
    lv_obj_t *row = makeBox(s_wx.days, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_flex_grow(row, 1);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);

    lv_obj_t *label = makeLabel(row, d.label.c_str(), &lv_font_montserrat_20);
    lv_obj_set_width(label, 66);
    lv_obj_t *icon = lv_image_create(row);
    lv_image_set_src(icon, weather::smallIcon(d.conditionId));
    lv_obj_t *hi = makeLabel(row, fmtTemp(d.tempMax, false, metric).c_str(), &lv_font_montserrat_20);
    lv_obj_set_width(hi, 48);
    lv_obj_set_style_text_align(hi, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t *lo = makeLabel(row, fmtTemp(d.tempMin, false, metric).c_str(), &lv_font_montserrat_20, kLowTempColor);
    lv_obj_set_width(lo, 48);
    lv_obj_set_style_text_align(lo, LV_TEXT_ALIGN_RIGHT, 0);

    String detail = d.description;
    String precip;
    if (d.snow >= 0.1f) precip += "Snow " + fmtPrecip(d.snow, metric);
    if (d.rain >= 0.1f) precip += (precip.length() ? "  Rain " : "Rain ") + fmtPrecip(d.rain, metric);
    if (d.pop > 0) precip += (precip.length() ? "  (" : "(") + String(d.pop) + "%)";
    if (precip.length()) detail += "\n" + precip;
    lv_obj_t *det = makeLabel(row, detail.c_str(), &lv_font_montserrat_14, kBodyColor);
    lv_obj_set_flex_grow(det, 1);
    lv_obj_set_style_pad_left(det, 8, 0);
    lv_label_set_long_mode(det, LV_LABEL_LONG_DOT);
}

}  // namespace

// ---------------------------------------------------------------- public

void build(const forecast::Forecast &fc) {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(kBgColor), 0);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Header
    lv_obj_t *header = lv_obj_create(scr);
    lv_obj_set_size(header, display::kWidth, kHeaderH);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(kPanelColor), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_hor(header, 16, 0);
    lv_obj_set_style_pad_ver(header, 4, 0);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    // Logo + title
    lv_obj_t *brand = makeBox(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(brand, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(brand, 12, 0);
    lv_obj_align(brand, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *logo = lv_image_create(brand);
    lv_image_set_src(logo, &logo_icon);  // A8 image: drawn in the recolor colour
    lv_obj_set_style_image_recolor(logo, lv_color_white(), 0);
    lv_obj_set_style_image_recolor_opa(logo, LV_OPA_COVER, 0);
    makeLabel(brand, "Backcountry Forecast", &lv_font_montserrat_28);

    // Refresh button (far right), location to its left.
    constexpr int32_t kRefreshSize = 48;
    s_refreshBtn = lv_button_create(header);
    lv_obj_set_size(s_refreshBtn, kRefreshSize, kRefreshSize);
    lv_obj_align(s_refreshBtn, LV_ALIGN_RIGHT_MID, 8, 0);
    lv_obj_set_style_bg_opa(s_refreshBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(s_refreshBtn, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(s_refreshBtn, lv_color_hex(0x2A3A4D), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(s_refreshBtn, 0, 0);
    lv_obj_set_style_radius(s_refreshBtn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_text_color(s_refreshBtn, lv_color_white(), 0);
    lv_obj_set_style_text_color(s_refreshBtn, lv_color_hex(0x55606C), LV_STATE_DISABLED);
    lv_obj_set_style_text_font(s_refreshBtn, &lv_font_montserrat_24, 0);
    lv_obj_t *icon = lv_label_create(s_refreshBtn);
    lv_label_set_text(icon, LV_SYMBOL_REFRESH);
    lv_obj_center(icon);
    lv_obj_add_event_cb(
        s_refreshBtn,
        [](lv_event_t *) {
            if (s_refreshCb != nullptr) s_refreshCb();
        },
        LV_EVENT_CLICKED, nullptr);

    s_regionLabel = makeLabel(header, "", &lv_font_montserrat_24, kAccentColor);
    lv_obj_align(s_regionLabel, LV_ALIGN_RIGHT_MID, -(kRefreshSize + 4), 0);

    // Tabs
    lv_obj_t *tv = lv_tabview_create(scr);
    lv_obj_set_size(tv, display::kWidth, display::kHeight - kHeaderH);
    lv_obj_align(tv, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(tv, lv_color_hex(kBgColor), 0);
    lv_tabview_set_tab_bar_position(tv, LV_DIR_BOTTOM);
    lv_tabview_set_tab_bar_size(tv, kTabBarH);

    lv_obj_t *bar = lv_tabview_get_tab_bar(tv);
    lv_obj_set_style_bg_color(bar, lv_color_hex(kPanelColor), 0);
    lv_obj_set_style_text_font(bar, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(bar, lv_color_hex(kMutedColor), 0);

    lv_obj_t *weatherTab = lv_tabview_add_tab(tv, "Weather");  // first = shown at boot
    s_dangerTab = lv_tabview_add_tab(tv, "Danger");
    s_problemsTab = lv_tabview_add_tab(tv, "Problems");
    s_conditionsTab = lv_tabview_add_tab(tv, "Conditions");
    s_detailsTab = lv_tabview_add_tab(tv, "Details");

    const uint32_t n = lv_obj_get_child_count(bar);
    for (uint32_t i = 0; i < n; ++i) {
        lv_obj_t *btn = lv_obj_get_child(bar, i);
        lv_obj_set_style_text_color(btn, lv_color_white(), LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2A3A4D), LV_STATE_CHECKED);
        lv_obj_set_style_border_color(btn, lv_color_hex(kAccentColor), LV_STATE_CHECKED);
    }

    buildWeatherTab(weatherTab);
    buildDangerSkeleton();
    showForecast(fc);
}

void showForecast(const forecast::Forecast &fc) {
    lv_label_set_text(s_regionLabel, fc.region.c_str());
    String issued = fc.valid ? "Issued " + fc.issued : fc.issued;
    lv_label_set_text(s_issuedLabel, issued.c_str());
    buildDangerTab(fc);
    buildProblemsTab(fc);
    buildConditionsTab(fc);
    buildDetailsTab(fc);
}

void setRefreshCallback(void (*cb)()) { s_refreshCb = cb; }

void setRefreshing(bool busy) {
    if (s_refreshBtn == nullptr) return;
    if (busy) lv_obj_add_state(s_refreshBtn, LV_STATE_DISABLED);
    else lv_obj_remove_state(s_refreshBtn, LV_STATE_DISABLED);
}

void setStatus(const char *text, StatusLevel level) {
    if (s_statusLabel == nullptr) return;
    uint32_t color = kMutedColor;
    if (level == StatusLevel::Ok) color = 0x7CC47F;
    if (level == StatusLevel::Error) color = 0xF7941E;
    lv_obj_set_style_text_color(s_statusLabel, lv_color_hex(color), 0);
    lv_label_set_text(s_statusLabel, text);
}

void showWeather(const weather::Weather &w) {
    const auto &c = w.now;
    // Before NTP sync (e.g. offline after a power loss) use the download time instead.
    const time_t now = time(nullptr) > 1700000000 ? time(nullptr) : w.fetched;
    const bool night = c.sunrise && c.sunset && (now < c.sunrise || now > c.sunset);
    const bool m = w.metric;
    const char *speed = m ? "km/h" : "mph";

    lv_image_set_src(s_wx.icon, weather::largeIcon(c.conditionId, night));
    lv_label_set_text(s_wx.temp, fmtTemp(c.temp, true, m).c_str());
    lv_label_set_text(s_wx.desc, c.description.c_str());
    lv_label_set_text(s_wx.feels, ("Feels like " + fmtTemp(c.feelsLike, false, m)).c_str());
    if (w.fromCache) {
        struct tm tm;
        localtime_r(&w.fetched, &tm);
        char buf[32];
        strftime(buf, sizeof buf, "Saved %a %H:%M", &tm);
        lv_label_set_text(s_wx.updated, buf);
    } else {
        lv_label_set_text(s_wx.updated, ("Updated " + fmtClock(w.fetched)).c_str());
    }
    lv_obj_set_style_text_color(s_wx.updated, lv_color_hex(kMutedColor), 0);

    lv_image_set_src(s_wx.windIcon, weather::windIcon(c.windDeg));
    lv_label_set_text_fmt(s_wx.wind, "%ld %s %s", lroundf(c.windSpeed), speed, weather::windDirName(c.windDeg));
    if (c.windGust > c.windSpeed) lv_label_set_text_fmt(s_wx.gust, "Gusts %ld %s", lroundf(c.windGust), speed);
    else lv_label_set_text(s_wx.gust, "");

    lv_label_set_text_fmt(s_wx.stats, "Humidity %d%%    Clouds %d%%\nPressure %d hPa    Visibility %.0f %s",
                          c.humidity, c.clouds, c.pressure, c.visibility, m ? "km" : "mi");

    lv_image_set_src(s_wx.moonIcon, weather::moonIcon(now));
    lv_label_set_text_fmt(s_wx.moon, "%s\n%d%% lit", weather::moonPhaseName(now), weather::moonIllumination(now));
    lv_label_set_text(s_wx.sun, ("Sunrise " + fmtClock(c.sunrise) + "\nSunset  " + fmtClock(c.sunset)).c_str());

    lv_obj_clean(s_wx.days);
    for (const auto &d : w.days) addDayRow(d, m);

    s_wx.valid = true;
    lv_obj_add_flag(s_wx.message, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_wx.content, LV_OBJ_FLAG_HIDDEN);
}

void setWeatherError(const char *text) {
    if (s_wx.valid) {
        // Keep showing the last good data; flag the problem in the "Updated" line.
        lv_label_set_text(s_wx.updated, text);
        lv_obj_set_style_text_color(s_wx.updated, lv_color_hex(0xF7941E), 0);
    } else {
        lv_label_set_text(s_wx.message, text);
    }
}

}  // namespace ui
