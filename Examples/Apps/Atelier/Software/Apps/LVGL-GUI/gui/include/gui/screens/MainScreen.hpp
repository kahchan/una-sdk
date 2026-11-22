#ifndef MAIN_SCREEN_HPP
#define MAIN_SCREEN_HPP

#include "lvgl.h"

#include "gui/faces/FaceTable.hpp"
#include "gui/model/Model.hpp"
#include "gui/model/ModelListener.hpp"

class MainScreen : public ModelListener
{
public:
    explicit MainScreen(Model& model);

    lv_obj_t* root() const { return mRoot; }

    void onTime(uint8_t hour, uint8_t minute, uint8_t mday) override;
    void onValues(const uint16_t* value) override;
    void onFace(uint8_t index) override;

private:
    static constexpr int kMaxNeedles = 3;

    void applyTime();
    void applyNeedles();

    lv_obj_t*      mRoot = nullptr;
    lv_obj_t*      mNeedle[kMaxNeedles] = {};
    lv_obj_t*      mHour = nullptr;
    lv_obj_t*      mMinute = nullptr;
    lv_obj_t*      mHub = nullptr;
    lv_image_dsc_t mNeedleDsc[kMaxNeedles] {};
    lv_image_dsc_t mHourDsc {}, mMinuteDsc {}, mHubDsc {};

    uint8_t  mFace = 0;
    uint8_t  mHourVal = 0, mMinuteVal = 0;
    uint16_t mValues[3] = {};
};

#endif // MAIN_SCREEN_HPP
