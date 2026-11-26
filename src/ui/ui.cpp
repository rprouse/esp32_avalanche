#include "ui.h"

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

forecast::Forecast s_fc;

lv_obj_t *s_regionLabel = nullptr;
lv_obj_t *s_issuedLabel = nullptr;
lv_obj_t *s_dangerTab = nullptr;
lv_obj_t *s_problemsTab = nullptr;
lv_obj_t *s_detailsTab = nullptr;

// ---------------------------------------------------------------- helpers

lv_obj_t *makeLabel(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color = 0xFFFFFF) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    return l;
}

lv_obj_t *makePanel(lv_obj_t *parent) {
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_set_style_bg_color(p, lv_color_hex(kPanelColor), 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_radius(p, 10, 0);
    lv_obj_set_style_pad_all(p, 12, 0);
    return p;
}

void styleDangerCell(lv_obj_t *cell, Danger d) {
    lv_obj_set_style_bg_color(cell, lv_color_hex(forecast::dangerColor(d)), 0);
    lv_obj_t *label = lv_obj_get_child(cell, 0);
    lv_label_set_text(label, forecast::dangerName(d));
    lv_obj_set_style_text_color(label, lv_color_hex(forecast::dangerTextColor(d)), 0);
}

Danger &ratingRef(int day, int band) {
    auto &r = s_fc.days[day];
    return band == 0 ? r.alpine : (band == 1 ? r.treeline : r.belowTreeline);
}

// Touch demo: tapping a danger cell cycles its rating.
void onDangerCellClicked(lv_event_t *e) {
    lv_obj_t *cell = static_cast<lv_obj_t *>(lv_event_get_current_target(e));
    const intptr_t idx = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
    Danger &d = ratingRef(idx / 3, idx % 3);
    d = forecast::nextDanger(d);
    styleDangerCell(cell, d);
}

// ---------------------------------------------------------------- Danger tab

void buildDangerTab() {
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
        lv_obj_t *h = makeLabel(grid, s_fc.days[day].label.c_str(), &lv_font_montserrat_24);
        lv_obj_set_grid_cell(h, LV_GRID_ALIGN_CENTER, day + 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    }

    static const char *bands[] = {"Alpine", "Treeline", "Below Treeline"};
    for (int band = 0; band < 3; ++band) {
        lv_obj_t *bl = makeLabel(grid, bands[band], &lv_font_montserrat_24);
        lv_obj_set_grid_cell(bl, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, band + 1, 1);

        for (int day = 0; day < forecast::kDays; ++day) {
            lv_obj_t *cell = lv_obj_create(grid);
            lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_STRETCH, day + 1, 1, LV_GRID_ALIGN_STRETCH, band + 1, 1);
            lv_obj_set_style_border_width(cell, 0, 0);
            lv_obj_set_style_radius(cell, 8, 0);
            lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
            // Visual press feedback
            lv_obj_set_style_transform_scale(cell, 240, LV_STATE_PRESSED);

            lv_obj_t *l = makeLabel(cell, "", &lv_font_montserrat_20);
            lv_obj_center(l);
            styleDangerCell(cell, ratingRef(day, band));
            lv_obj_add_event_cb(cell, onDangerCellClicked, LV_EVENT_CLICKED,
                                reinterpret_cast<void *>(static_cast<intptr_t>(day * 3 + band)));
        }
    }

    lv_obj_t *hint = makeLabel(grid, "Tap a rating to cycle it (touch demo).  Swipe or use the tabs to change screens.",
                               &lv_font_montserrat_14, 0x9AA8B8);
    lv_obj_set_grid_cell(hint, LV_GRID_ALIGN_CENTER, 0, 4, LV_GRID_ALIGN_CENTER, 4, 1);
}

// ---------------------------------------------------------------- Problems tab

void buildProblemsTab() {
    lv_obj_clean(s_problemsTab);
    lv_obj_set_flex_flow(s_problemsTab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_problemsTab, 12, 0);
    lv_obj_set_style_pad_row(s_problemsTab, 12, 0);

    if (s_fc.problems.empty()) {
        makeLabel(s_problemsTab, "No avalanche problems listed.", &lv_font_montserrat_28);
        return;
    }
    for (const auto &p : s_fc.problems) {
        lv_obj_t *card = makePanel(s_problemsTab);
        lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(card, 6, 0);

        makeLabel(card, p.type.c_str(), &lv_font_montserrat_28, kAccentColor);
        String facts = "Elevation: " + p.elevations + "    Aspects: " + p.aspects + "\nLikelihood: " +
                       p.likelihood + "    Size: " + p.size;
        makeLabel(card, facts.c_str(), &lv_font_montserrat_20);
        lv_obj_t *c = makeLabel(card, p.comment.c_str(), &lv_font_montserrat_20, 0xC8D2DC);
        lv_obj_set_width(c, LV_PCT(100));
        lv_label_set_long_mode(c, LV_LABEL_LONG_WRAP);
    }
}

// ---------------------------------------------------------------- Details tab

void buildDetailsTab() {
    lv_obj_clean(s_detailsTab);
    lv_obj_set_flex_flow(s_detailsTab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_detailsTab, 12, 0);
    lv_obj_set_style_pad_row(s_detailsTab, 12, 0);

    lv_obj_t *card = makePanel(s_detailsTab);
    lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    makeLabel(card, "Highlights", &lv_font_montserrat_28, kAccentColor);
    lv_obj_t *h = makeLabel(card, s_fc.highlights.c_str(), &lv_font_montserrat_20);
    lv_obj_set_width(h, LV_PCT(100));
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);

    lv_obj_t *conf = makePanel(s_detailsTab);
    lv_obj_set_size(conf, LV_PCT(100), LV_SIZE_CONTENT);
    String txt = "Confidence: " + s_fc.confidence;
    makeLabel(conf, txt.c_str(), &lv_font_montserrat_20);
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

    // Offset from nearest target
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
    s_fc = fc;

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
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = makeLabel(header, "Avalanche Forecast", &lv_font_montserrat_28);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);
    s_regionLabel = makeLabel(header, "", &lv_font_montserrat_20, kAccentColor);
    lv_obj_align(s_regionLabel, LV_ALIGN_CENTER, 90, 0);
    s_issuedLabel = makeLabel(header, "", &lv_font_montserrat_14, 0x9AA8B8);
    lv_obj_align(s_issuedLabel, LV_ALIGN_RIGHT_MID, 0, 0);

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
    lv_obj_set_style_text_color(bar, lv_color_hex(0x9AA8B8), 0);

    s_dangerTab = lv_tabview_add_tab(tv, "Danger");
    s_problemsTab = lv_tabview_add_tab(tv, "Problems");
    s_detailsTab = lv_tabview_add_tab(tv, "Details");
    lv_obj_t *touchTab = lv_tabview_add_tab(tv, "Touch Test");

    // Selected tab button styling
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
    s_fc = fc;
    lv_label_set_text(s_regionLabel, s_fc.region.c_str());
    lv_label_set_text(s_issuedLabel, s_fc.issued.c_str());
    buildDangerTab();
    buildProblemsTab();
    buildDetailsTab();
}

}  // namespace ui
