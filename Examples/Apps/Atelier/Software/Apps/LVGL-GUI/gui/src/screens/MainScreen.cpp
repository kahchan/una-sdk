#include "gui/screens/MainScreen.hpp"

#include <algorithm>

#include "gui/faces/DialPainter.hpp"

#include "Commands.hpp"

namespace {

void placeImage(lv_obj_t* img, lv_image_dsc_t& dsc, const HandImage& h, int32_t cx, int32_t cy)
{
    dsc.header.magic  = LV_IMAGE_HEADER_MAGIC;
    dsc.header.cf     = LV_COLOR_FORMAT_RGB565A8;
    dsc.header.w      = h.w;
    dsc.header.h      = h.h;
    dsc.header.stride = h.w * 2;
    dsc.data_size     = h.w * h.h * 3;
    dsc.data          = h.px;

    lv_image_set_src(img, &dsc);
    lv_obj_set_pos(img, cx - h.pivotX, cy - h.pivotY);
    lv_image_set_pivot(img, h.pivotX, h.pivotY);
    lv_image_set_antialias(img, false);
}

lv_obj_t* makeImage(lv_obj_t* parent)
{
    lv_obj_t* img = lv_image_create(parent);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    return img;
}

constexpr int32_t kCentre = 120;

} // namespace

MainScreen::MainScreen(Model& model)
{
    mRoot = lv_obj_create(nullptr);
    lv_obj_remove_flag(mRoot, LV_OBJ_FLAG_SCROLLABLE);

    // z-order: dial, needles, hour, minute, hub
    dialPainterCreate(mRoot, *kFaces[0].assets);
    for (auto& n : mNeedle) {
        n = makeImage(mRoot);
    }
    mHour   = makeImage(mRoot);
    mMinute = makeImage(mRoot);
    mHub    = makeImage(mRoot);

    onFace(0);
    model.bind(this);
}

void MainScreen::onFace(uint8_t index)
{
    if (index >= CustomMessage::FACE_COUNT) {
        return;
    }
    mFace = index;
    std::fill(mValues, mValues + 3, CustomMessage::kNoData);   // until the service re-sends them
    const FaceAssets& a = *kFaces[index].assets;

    dialPainterSetFace(a);
    placeImage(mHour, mHourDsc, *a.hour, kCentre, kCentre);
    placeImage(mMinute, mMinuteDsc, *a.minute, kCentre, kCentre);
    placeImage(mHub, mHubDsc, *a.hub, kCentre, kCentre);
    for (int i = 0; i < kMaxNeedles; ++i) {
        lv_obj_add_flag(mNeedle[i], LV_OBJ_FLAG_HIDDEN);   // applyNeedles is the only code that shows one
        lv_image_set_rotation(mNeedle[i], 0);
        if (i < a.needleCount) {
            placeImage(mNeedle[i], mNeedleDsc[i], *a.needles[i].img, a.needles[i].cx, a.needles[i].cy);
        }
    }
    applyTime();
    applyNeedles();
}

void MainScreen::applyTime()
{
    // Rotation is in 0.1 degree units; the dial's tilt turns the hands with it.
    const int32_t tilt = kFaces[mFace].assets->tilt * 10;
    lv_image_set_rotation(mHour, (mHourVal % 12 * 60 + mMinuteVal) * 5 + tilt);
    lv_image_set_rotation(mMinute, mMinuteVal * 60 + tilt);
}

void MainScreen::applyNeedles()
{
    const FaceEntry& f = kFaces[mFace];
    for (int i = 0; i < f.assets->needleCount; ++i) {
        const Needle& n = f.assets->needles[i];
        const uint16_t v = mValues[n.channel];
        if (v == CustomMessage::kNoData) {
            lv_obj_add_flag(mNeedle[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(mNeedle[i], LV_OBJ_FLAG_HIDDEN);
        // span 0: the face maps the angle itself (Doctor)
        lv_image_set_rotation(mNeedle[i], n.span ? n.base * 10 + n.span * v / 100 : f.needle0(v));
    }
}

void MainScreen::onTime(uint8_t hour, uint8_t minute, uint8_t mday)
{
    mHourVal = hour;
    mMinuteVal = minute;
    dialPainterSetDay(mday);
    applyTime();
}

void MainScreen::onValues(const uint16_t* value)
{
    std::copy(value, value + 3, mValues);
    dialPainterSetValues(value);
    applyNeedles();
}
