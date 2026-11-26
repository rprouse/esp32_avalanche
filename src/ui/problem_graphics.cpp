#include "problem_graphics.h"

#include <math.h>
#include <stdint.h>

namespace ui {
namespace {

constexpr uint32_t kOnColor = 0x3D9BE0;     // affected / in range
constexpr uint32_t kOffColor = 0x2E3B4B;    // not affected
constexpr uint32_t kGapColor = 0x1C2633;    // panel background (segment separators)
constexpr uint32_t kLetterColor = 0xD8E0E8;
constexpr uint32_t kLabelColor = 0x9AA8B8;
constexpr int32_t kLetterSpace = 18;        // room around the rose for N/E/S/W
constexpr int32_t kScaleBarH = 16;

// The drawing data is packed into the object's user_data pointer, so nothing
// needs to be allocated or freed per object.
//   rose:  bits 0-7 alpine, 8-15 treeline, 16-23 below treeline
//   scale: bits 0-7 lo*2, 8-15 hi*2

int normAngle(int a) { return ((a % 360) + 360) % 360; }

void drawRose(lv_event_t *e) {
    lv_obj_t *obj = static_cast<lv_obj_t *>(lv_event_get_current_target(e));
    lv_layer_t *layer = lv_event_get_layer(e);
    const uint32_t packed = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(obj));

    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    const int32_t radius = lv_area_get_width(&a) / 2;
    const int32_t ring = radius / 3;

    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.center.x = a.x1 + radius;
    dsc.center.y = a.y1 + radius;
    dsc.rounded = 0;

    for (int band = 0; band < 3; ++band) {
        const uint8_t bits = (packed >> (8 * band)) & 0xFF;
        // Centre (alpine) is a full pie; outer rings leave a 2 px gap inside them.
        dsc.radius = ring * (band + 1);
        dsc.width = band == 0 ? ring : ring - 2;
        for (int aspect = 0; aspect < 8; ++aspect) {
            // LVGL angles: 0 = east, clockwise. North (aspect 0) is at 270.
            const int centre = 270 + 45 * aspect;
            dsc.start_angle = normAngle(centre - 21);
            dsc.end_angle = normAngle(centre + 21);
            dsc.color = lv_color_hex((bits >> aspect) & 1 ? kOnColor : kOffColor);
            lv_draw_arc(layer, &dsc);
        }
    }
}

void drawScale(lv_event_t *e) {
    lv_obj_t *obj = static_cast<lv_obj_t *>(lv_event_get_current_target(e));
    lv_layer_t *layer = lv_event_get_layer(e);
    const uint32_t packed = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(obj));
    const float lo = (packed & 0xFF) / 2.0f;
    const float hi = ((packed >> 8) & 0xFF) / 2.0f;

    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    const int32_t w = lv_area_get_width(&a);

    lv_draw_rect_dsc_t rect;
    lv_draw_rect_dsc_init(&rect);
    rect.radius = 4;
    rect.bg_color = lv_color_hex(kOffColor);
    lv_draw_rect(layer, &rect, &a);

    if (lo >= 1 && hi >= lo) {
        // Step k (1..5) occupies [(k-1)/5, k/5] of the width.
        lv_area_t fill = a;
        fill.x1 = a.x1 + (int32_t)((lo - 1) / 5 * w);
        fill.x2 = a.x1 + (int32_t)(hi / 5 * w) - 1;
        rect.bg_color = lv_color_hex(kOnColor);
        lv_draw_rect(layer, &rect, &fill);
    }

    // Separators so it reads as five steps.
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = lv_color_hex(kGapColor);
    line.width = 3;
    for (int k = 1; k < 5; ++k) {
        const int32_t x = a.x1 + k * w / 5;
        line.p1.x = x;
        line.p1.y = a.y1;
        line.p2.x = x;
        line.p2.y = a.y2 + 1;
        lv_draw_line(layer, &line);
    }
}

lv_obj_t *plainObj(lv_obj_t *parent) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

lv_obj_t *smallLabel(lv_obj_t *parent, const char *text, uint32_t color) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    return l;
}

}  // namespace

lv_obj_t *createAspectRose(lv_obj_t *parent, const uint8_t mask[3], int32_t size) {
    lv_obj_t *box = plainObj(parent);
    lv_obj_set_size(box, size, size);

    lv_obj_t *rose = plainObj(box);
    lv_obj_set_size(rose, size - 2 * kLetterSpace, size - 2 * kLetterSpace);
    lv_obj_center(rose);
    const uintptr_t packed = mask[0] | (mask[1] << 8) | ((uint32_t)mask[2] << 16);
    lv_obj_set_user_data(rose, reinterpret_cast<void *>(packed));
    lv_obj_add_event_cb(rose, drawRose, LV_EVENT_DRAW_MAIN, nullptr);

    struct Letter { const char *text; lv_align_t align; };
    static const Letter kLetters[] = {
        {"N", LV_ALIGN_TOP_MID}, {"E", LV_ALIGN_RIGHT_MID}, {"S", LV_ALIGN_BOTTOM_MID}, {"W", LV_ALIGN_LEFT_MID}};
    for (const auto &l : kLetters) {
        lv_obj_t *label = smallLabel(box, l.text, kLetterColor);
        lv_obj_align(label, l.align, 0, 0);
    }
    return box;
}

lv_obj_t *createRangeScale(lv_obj_t *parent, float lo, float hi, int32_t width, const char *const labels[5]) {
    lv_obj_t *box = plainObj(parent);
    lv_obj_set_size(box, width, kScaleBarH + 20);

    lv_obj_t *bar = plainObj(box);
    lv_obj_set_size(bar, width, kScaleBarH);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);
    const uintptr_t packed = ((uint32_t)lroundf(lo * 2) & 0xFF) | (((uint32_t)lroundf(hi * 2) & 0xFF) << 8);
    lv_obj_set_user_data(bar, reinterpret_cast<void *>(packed));
    lv_obj_add_event_cb(bar, drawScale, LV_EVENT_DRAW_MAIN, nullptr);

    for (int k = 0; k < 5; ++k) {
        if (labels == nullptr || labels[k] == nullptr || labels[k][0] == '\0') continue;
        lv_obj_t *l = smallLabel(box, labels[k], kLabelColor);
        const int32_t centre = (2 * k + 1) * width / 10;
        if (k == 0) lv_obj_align(l, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        else if (k == 4) lv_obj_align(l, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        else lv_obj_align(l, LV_ALIGN_BOTTOM_MID, centre - width / 2, 0);
    }
    return box;
}

}  // namespace ui
