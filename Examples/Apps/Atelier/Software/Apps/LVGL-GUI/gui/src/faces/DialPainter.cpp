#include "gui/faces/DialPainter.hpp"

#include "Commands.hpp"

#include <algorithm>
#include <cmath>

#define LOG_MODULE_PRX      "Painter"
#define LOG_MODULE_LEVEL    LOG_LEVEL_INFO
#include "SDK/UnaLogger/Logger.h"

namespace {

constexpr int32_t kSize = 240;

struct State {
    const FaceAssets* assets;
    lv_obj_t*         obj;
    int32_t           lit[3];   // per channel; slot units lit: ticks, or degrees when continuous
    bool              past[3];
    uint8_t           slotMask, angleMask;   // channels some slot / some angle slot reads
    uint8_t           day;
    lv_color_t        palette[64];
} s;

void buildMasks()
{
    s.slotMask = s.angleMask = 0;
    for (int i = 0; i < s.assets->slotCount; ++i) {
        const DialSlot& d = s.assets->slots[i];
        s.slotMask |= 1 << d.channel;
        if (d.kind == 0) {
            s.angleMask |= 1 << d.channel;
        }
    }
}

void buildPalette()
{
    // ABGR2222 code (b<<4)|(g<<2)|r, each level 0..3 -> 0,85,170,255
    static const uint8_t lvl[4] = { 0, 85, 170, 255 };
    for (int c = 0; c < 64; ++c) {
        s.palette[c] = lv_color_make(lvl[c & 3], lvl[(c >> 2) & 3], lvl[(c >> 4) & 3]);
    }
}

bool isOn(int32_t x, int32_t y, float seam, float splitDeg)
{
    // Centre is the middle of pixel 120; with ticks the seam sits half a tick before 12 so no tick is split.
    float th = std::atan2(static_cast<float>(x - 120), static_cast<float>(120 - y)) * (180.0f / 3.14159265f);
    if (th < seam) {
        th += 360.0f;
    }
    return th < splitDeg;
}

void fill(lv_layer_t* layer, lv_draw_fill_dsc_t& dsc, lv_color_t color, int32_t x1, int32_t x2, int32_t y)
{
    dsc.color = color;
    const lv_area_t a = { x1, y, x2, y };
    lv_draw_fill(layer, &dsc, &a);
}

// Paints one run list laid out row-major in a w-wide rectangle at (ox, oy), for the rows the clip meets.
void paintRuns(lv_layer_t* layer, const lv_area_t& clip, const uint8_t* runs, uint32_t runCount,
               int32_t ox, int32_t oy, int32_t w)
{
    const FaceAssets& fa = *s.assets;

    const float units = fa.ticks ? fa.ticks : 360;
    const float seam = fa.ticks ? -180.0f / fa.ticks : 0.0f;

    lv_draw_fill_dsc_t dsc;
    lv_draw_fill_dsc_init(&dsc);

    uint32_t pos = 0;
    for (uint32_t i = 0; i < runCount; ++i) {
        const uint8_t code = runs[2 * i];
        uint32_t len = runs[2 * i + 1];
        if (oy + static_cast<int32_t>((pos + len - 1) / w) < clip.y1) {
            pos += len;
            continue;
        }
        if (oy + static_cast<int32_t>(pos / w) > clip.y2) {
            break;
        }

        const DialSlot* slot = code >= 64 ? &fa.slots[code - 64] : nullptr;
        lv_color_t on {}, off {};
        bool past = false;
        float split = 0;
        if (slot) {
            past = s.past[slot->channel];
            on = s.palette[past ? slot->onPastGoal : slot->on];
            off = s.palette[slot->off];
            split = s.lit[slot->channel] * 360.0f / units + seam;
        }
        // One angle-slot segment: both ends agree (one fill) or fall back to per pixel.
        auto angleSeg = [&](int32_t y, int32_t sx1, int32_t sx2) {
            const bool a = past || isOn(sx1, y, seam, split);
            const bool b = past || isOn(sx2, y, seam, split);
            if (a == b) {
                fill(layer, dsc, a ? on : off, sx1, sx2, y);
                return;
            }
            for (int32_t px = sx1; px <= sx2; ++px) {
                fill(layer, dsc, (past || isOn(px, y, seam, split)) ? on : off, px, px, y);
            }
        };

        while (len > 0) {
            const int32_t y = oy + static_cast<int32_t>(pos / w);
            const int32_t x = ox + static_cast<int32_t>(pos % w);
            const uint32_t n = std::min<uint32_t>(len, w - (x - ox));
            pos += n;
            len -= n;
            const int32_t x1 = std::max(x, clip.x1);
            const int32_t x2 = std::min<int32_t>(x + n - 1, clip.x2);
            if (y < clip.y1 || y > clip.y2 || x1 > x2) {
                continue;
            }
            if (!slot) {
                fill(layer, dsc, s.palette[code], x1, x2, y);
                continue;
            }
            if (slot->kind == 1) {
                fill(layer, dsc, on, x1, x2, y);
                continue;
            }
            // A continuous slot's top rows can cross both the 12 o'clock seam and the split ray.
            if (!fa.ticks && y < 120 && x1 < 120 && x2 >= 120) {
                angleSeg(y, x1, 119);
                angleSeg(y, 120, x2);
            } else {
                angleSeg(y, x1, x2);
            }
        }
    }
}

void drawCb(lv_event_t* e)
{
    lv_layer_t* layer = lv_event_get_layer(e);
    const lv_area_t clip = layer->_clip_area;
    const FaceAssets& fa = *s.assets;

    paintRuns(layer, clip, fa.runs, fa.runCount, 0, 0, kSize);
    if (fa.date && s.day >= 1 && s.day <= 31) {
        const DateWindow& d = *fa.date;
        paintRuns(layer, clip, d.runs + 2 * d.start[s.day - 1], d.start[s.day] - d.start[s.day - 1], d.x, d.y, d.w);
    }
}

#ifdef ATELIER_MEASURE
void refreshReadyCb(lv_event_t*)
{
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    LOG_INFO("lv mem max_used %lu B, used_pct %u%%\n",
             static_cast<unsigned long>(mon.max_used), static_cast<unsigned>(mon.used_pct));
}
#endif

} // namespace

lv_obj_t* dialPainterCreate(lv_obj_t* parent, const FaceAssets& assets)
{
    s.assets = &assets;
    buildMasks();
    buildPalette();

    lv_obj_t* obj = s.obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, kSize, kSize);
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_remove_flag(obj, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_add_event_cb(obj, drawCb, LV_EVENT_DRAW_MAIN, nullptr);

#ifdef ATELIER_MEASURE
    lv_display_add_event_cb(lv_display_get_default(), refreshReadyCb, LV_EVENT_REFR_READY, nullptr);
#endif
    return obj;
}

void dialPainterSetValues(const uint16_t value[3])
{
    const int32_t units = s.assets->ticks ? s.assets->ticks : 360;
    bool changed = false;
    for (int c = 0; c < 3; ++c) {
        const bool known = value[c] != CustomMessage::kNoData;
        const int32_t lit = known ? (std::min<int32_t>(value[c], 1000) * units + 500) / 1000 : 0;
        const bool past = known && value[c] >= 1000;
        // Needles invalidate themselves; the dial only when a slot reads what changed.
        changed |= ((s.angleMask >> c) & 1 && lit != s.lit[c]) || ((s.slotMask >> c) & 1 && past != s.past[c]);
        s.lit[c] = lit;
        s.past[c] = past;
    }
    if (changed) {
        lv_obj_invalidate(s.obj);
    }
}

void dialPainterSetDay(uint8_t mday)
{
    if (mday == s.day) {
        return;
    }
    s.day = mday;
    if (const DateWindow* d = s.assets->date) {
        const lv_area_t a = { d->x, d->y, d->x + d->w - 1, d->y + d->h - 1 };
        lv_obj_invalidate_area(s.obj, &a);
    }
}

void dialPainterSetFace(const FaceAssets& assets)
{
    s.assets = &assets;
    buildMasks();
    std::fill(s.lit, s.lit + 3, 0);
    std::fill(s.past, s.past + 3, false);
    s.day = 0;
    lv_obj_invalidate(s.obj);
}
