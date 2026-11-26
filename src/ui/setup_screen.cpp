#include "setup_screen.h"

#include <lvgl.h>

#include "display/display_port.h"
#include "ui/logo_icon.h"

namespace setup_screen {
namespace {

// Same palette as ui.cpp
constexpr int32_t kHeaderH = 56;
constexpr uint32_t kBgColor = 0x10161F;
constexpr uint32_t kPanelColor = 0x1C2633;
constexpr uint32_t kAccentColor = 0x4FA3E0;
constexpr uint32_t kClockColor = 0xFFC857;
constexpr uint32_t kMutedColor = 0x9AA8B8;
constexpr uint32_t kBodyColor = 0xD8E0E8;
constexpr int32_t kQrSize = 180;

lv_obj_t *s_status = nullptr;
void (*s_onCancel)() = nullptr;

lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    return l;
}

/** Escape \ ; , : " for the WIFI: QR payload. */
String qrEscape(const char *s) {
    String out;
    for (; *s; ++s) {
        if (strchr("\\;,:\"", *s)) out += '\\';
        out += *s;
    }
    return out;
}

/** One step card: number + heading, QR code, then detail lines. Returns the content box. */
lv_obj_t *card(lv_obj_t *parent, const char *step, const char *heading, const String &qrData) {
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_size(c, 372, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(c, lv_color_hex(kPanelColor), 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_radius(c, 12, 0);
    lv_obj_set_style_pad_all(c, 16, 0);
    lv_obj_set_style_pad_row(c, 10, 0);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = label(c, "", &lv_font_montserrat_24, 0xFFFFFF);
    lv_label_set_text_fmt(title, "%s  %s", step, heading);

    lv_obj_t *qr = lv_qrcode_create(c);
    lv_qrcode_set_size(qr, kQrSize);
    lv_qrcode_set_dark_color(qr, lv_color_black());
    lv_qrcode_set_light_color(qr, lv_color_white());
    lv_qrcode_set_quiet_zone(qr, true);
    lv_qrcode_update(qr, qrData.c_str(), qrData.length());
    return c;
}

}  // namespace

void build(const char *apSsid, const char *apPass, const char *url, void (*onCancel)()) {
    s_onCancel = onCancel;
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(kBgColor), 0);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Header: logo + title, "Wi-Fi setup" on the right.
    lv_obj_t *header = lv_obj_create(scr);
    lv_obj_set_size(header, display::kWidth, kHeaderH);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(kPanelColor), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_hor(header, 16, 0);
    lv_obj_set_style_pad_column(header, 12, 0);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *logo = lv_image_create(header);
    lv_image_set_src(logo, &logo_icon);
    lv_obj_set_style_image_recolor(logo, lv_color_white(), 0);
    lv_obj_set_style_image_recolor_opa(logo, LV_OPA_COVER, 0);
    label(header, "Backcountry", &lv_font_montserrat_28, 0xFFFFFF);
    lv_obj_t *spacer = lv_obj_create(header);
    lv_obj_set_size(spacer, 1, 1);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    label(header, LV_SYMBOL_WIFI "  Wi-Fi setup", &lv_font_montserrat_28, kClockColor);

    // Two step cards side by side.
    lv_obj_t *row = lv_obj_create(scr);
    lv_obj_set_size(row, display::kWidth, LV_SIZE_CONTENT);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, kHeaderH + 12);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    const String wifiQr = "WIFI:T:WPA;S:" + qrEscape(apSsid) + ";P:" + qrEscape(apPass) + ";;";
    lv_obj_t *c1 = card(row, "1", "Join this Wi-Fi network", wifiQr);
    lv_obj_t *net = label(c1, "", &lv_font_montserrat_24, kAccentColor);
    lv_label_set_text_fmt(net, "%s", apSsid);
    lv_obj_t *pw = label(c1, "", &lv_font_montserrat_24, kBodyColor);
    lv_label_set_text_fmt(pw, "Password: %s", apPass);

    lv_obj_t *c2 = card(row, "2", "Open the setup page", url);
    lv_obj_t *hint = label(c2, "", &lv_font_montserrat_20, kBodyColor);
    lv_label_set_text_fmt(hint, "It usually opens by itself.\nIf not, go to %s", url);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);

    // Status line (+ Cancel) at the bottom.
    s_status = label(scr, "Waiting for a phone to connect...", &lv_font_montserrat_24, kMutedColor);
    lv_obj_set_width(s_status, onCancel ? 600 : 760);
    lv_label_set_long_mode(s_status, LV_LABEL_LONG_DOT);
    lv_obj_align(s_status, LV_ALIGN_BOTTOM_LEFT, 20, -22);

    if (onCancel != nullptr) {
        lv_obj_t *btn = lv_button_create(scr);
        lv_obj_set_size(btn, 140, 52);
        lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, -20, -12);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2A3A4D), 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_t *l = label(btn, "Cancel", &lv_font_montserrat_20, 0xFFFFFF);
        lv_obj_center(l);
        lv_obj_add_event_cb(
            btn,
            [](lv_event_t *) {
                if (s_onCancel != nullptr) s_onCancel();
            },
            LV_EVENT_CLICKED, nullptr);
    }
}

void setStatus(const char *text, ui::StatusLevel level) {
    if (s_status == nullptr) return;
    uint32_t color = kMutedColor;
    if (level == ui::StatusLevel::Ok) color = 0x7CC47F;
    if (level == ui::StatusLevel::Error) color = 0xF7941E;
    lv_obj_set_style_text_color(s_status, lv_color_hex(color), 0);
    lv_label_set_text(s_status, text);
}

}  // namespace setup_screen
