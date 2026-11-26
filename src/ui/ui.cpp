#include "ui.h"

#include <climits>
#include <lvgl.h>

#include "display/display_port.h"

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
    lv_obj_clean(s_dangerTab);
    lv_obj_set_style_pad_all(s_dangerTab, 12, 0);

    static const int32_t cols[] = {200, LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const int32_t rows[] = {40, LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), 28, LV_GRID_TEMPLATE_LAST};

    lv_obj_t *grid = lv_obj_create(s_dangerTab);
    lv_obj_set_size(grid, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_style_pad_gap(grid, 8, 0);
    lv_obj_set_grid_dsc_array(grid, cols, rows);
    lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

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

    String foot = fc.validUntil.length() ? "Valid until " + fc.validUntil : String("");
    lv_obj_t *hint = makeLabel(grid, foot.c_str(), &lv_font_montserrat_14, kMutedColor);
    lv_obj_set_grid_cell(hint, LV_GRID_ALIGN_CENTER, 0, 4, LV_GRID_ALIGN_CENTER, 4, 1);
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

// ---------------------------------------------------------------- Touch test tab
//
// The GT911 is a capacitive controller with factory calibration, so no runtime
// calibration is needed. This screen lets you verify alignment: tap the targets
// and check the reported offset is only a few pixels.

struct TouchTest {
    lv_obj_t *pad;
    lv_obj_t *hline;
    lv_obj_t *vline;
    lv_obj_t *dot;
    lv_obj_t *readout;
    lv_point_t targets[5];  // pad-local coordinates (the tab scrolls, so screen coords move)
    uint32_t touches;
} s_tt;

void onTouchPad(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_active();
    if (indev == nullptr) return;

    lv_point_t p;
    lv_indev_get_point(indev, &p);  // screen coordinates
    lv_area_t a;
    lv_obj_get_coords(s_tt.pad, &a);
    const int32_t lx = p.x - a.x1;
    const int32_t ly = p.y - a.y1;

    if (code == LV_EVENT_PRESSED) s_tt.touches++;

    lv_obj_remove_flag(s_tt.hline, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_tt.vline, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_tt.dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(s_tt.hline, ly);
    lv_obj_set_x(s_tt.vline, lx);
    lv_obj_set_pos(s_tt.dot, lx - 12, ly - 12);

    int best = 0;
    int32_t bestD = INT32_MAX;
    for (int i = 0; i < 5; ++i) {
        const int32_t dx = lx - s_tt.targets[i].x, dy = ly - s_tt.targets[i].y;
        const int32_t d = dx * dx + dy * dy;
        if (d < bestD) { bestD = d; best = i; }
    }
    lv_label_set_text_fmt(s_tt.readout, "x=%d  y=%d   nearest target off by dx=%+d dy=%+d   touches=%u",
                          (int)p.x, (int)p.y, (int)(lx - s_tt.targets[best].x),
                          (int)(ly - s_tt.targets[best].y), (unsigned)s_tt.touches);
}

void buildTouchTab(lv_obj_t *tab) {
    lv_obj_set_style_pad_all(tab, 0, 0);
    lv_obj_remove_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

    s_tt.pad = lv_obj_create(tab);
    lv_obj_set_size(s_tt.pad, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_tt.pad, lv_color_hex(0x0A0E14), 0);
    lv_obj_set_style_border_width(s_tt.pad, 0, 0);
    lv_obj_set_style_radius(s_tt.pad, 0, 0);
    lv_obj_set_style_pad_all(s_tt.pad, 0, 0);
    lv_obj_remove_flag(s_tt.pad, LV_OBJ_FLAG_SCROLLABLE);
    // Don't hand drags to the tabview, otherwise drawing here would swipe tabs.
    lv_obj_remove_flag(s_tt.pad, LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    lv_obj_remove_flag(s_tt.pad, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
    lv_obj_add_event_cb(s_tt.pad, onTouchPad, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(s_tt.pad, onTouchPad, LV_EVENT_PRESSING, nullptr);

    // Alignment targets: 4 corners (inset 40 px) + centre, in pad-local coords.
    lv_obj_update_layout(s_tt.pad);
    const int32_t w = lv_obj_get_width(s_tt.pad), h = lv_obj_get_height(s_tt.pad);
    const lv_point_t local[5] = {{40, 40}, {w - 40, 40}, {40, h - 40}, {w - 40, h - 40}, {w / 2, h / 2}};
    for (int i = 0; i < 5; ++i) {
        lv_obj_t *t = lv_obj_create(s_tt.pad);
        lv_obj_set_size(t, 30, 30);
        lv_obj_set_style_radius(t, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(t, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(t, lv_color_hex(kAccentColor), 0);
        lv_obj_set_style_border_width(t, 2, 0);
        lv_obj_remove_flag(t, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(t, local[i].x - 15, local[i].y - 15);
        s_tt.targets[i] = local[i];
    }

    s_tt.hline = lv_obj_create(s_tt.pad);
    lv_obj_set_size(s_tt.hline, LV_PCT(100), 1);
    s_tt.vline = lv_obj_create(s_tt.pad);
    lv_obj_set_size(s_tt.vline, 1, LV_PCT(100));
    for (lv_obj_t *o : {s_tt.hline, s_tt.vline}) {
        lv_obj_set_style_bg_color(o, lv_color_hex(0x00FF88), 0);
        lv_obj_set_style_border_width(o, 0, 0);
        lv_obj_set_style_radius(o, 0, 0);
        lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    }
    s_tt.dot = lv_obj_create(s_tt.pad);
    lv_obj_set_size(s_tt.dot, 24, 24);
    lv_obj_set_style_radius(s_tt.dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_tt.dot, lv_color_hex(0xFF3366), 0);
    lv_obj_set_style_border_width(s_tt.dot, 0, 0);
    lv_obj_remove_flag(s_tt.dot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_tt.dot, LV_OBJ_FLAG_HIDDEN);

    s_tt.readout = makeLabel(s_tt.pad, "Touch anywhere. Tap the circles to check alignment.", &lv_font_montserrat_20);
    lv_obj_align(s_tt.readout, LV_ALIGN_TOP_MID, 0, 8);
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

    lv_obj_t *title = makeLabel(header, "Avalanche Forecast", &lv_font_montserrat_28);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);
    s_regionLabel = makeLabel(header, "", &lv_font_montserrat_24, kAccentColor);
    lv_obj_align(s_regionLabel, LV_ALIGN_LEFT_MID, 300, 0);
    s_issuedLabel = makeLabel(header, "", &lv_font_montserrat_14, kBodyColor);
    lv_obj_align(s_issuedLabel, LV_ALIGN_TOP_RIGHT, 0, 2);
    s_statusLabel = makeLabel(header, "", &lv_font_montserrat_14, kMutedColor);
    lv_obj_align(s_statusLabel, LV_ALIGN_BOTTOM_RIGHT, 0, -2);

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

    s_dangerTab = lv_tabview_add_tab(tv, "Danger");
    s_problemsTab = lv_tabview_add_tab(tv, "Problems");
    s_conditionsTab = lv_tabview_add_tab(tv, "Conditions");
    s_detailsTab = lv_tabview_add_tab(tv, "Details");
    lv_obj_t *touchTab = lv_tabview_add_tab(tv, "Touch");

    const uint32_t n = lv_obj_get_child_count(bar);
    for (uint32_t i = 0; i < n; ++i) {
        lv_obj_t *btn = lv_obj_get_child(bar, i);
        lv_obj_set_style_text_color(btn, lv_color_white(), LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2A3A4D), LV_STATE_CHECKED);
        lv_obj_set_style_border_color(btn, lv_color_hex(kAccentColor), LV_STATE_CHECKED);
    }

    buildTouchTab(touchTab);
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

void setStatus(const char *text, StatusLevel level) {
    if (s_statusLabel == nullptr) return;
    uint32_t color = kMutedColor;
    if (level == StatusLevel::Ok) color = 0x7CC47F;
    if (level == StatusLevel::Error) color = 0xF7941E;
    lv_obj_set_style_text_color(s_statusLabel, lv_color_hex(color), 0);
    lv_label_set_text(s_statusLabel, text);
}

}  // namespace ui
