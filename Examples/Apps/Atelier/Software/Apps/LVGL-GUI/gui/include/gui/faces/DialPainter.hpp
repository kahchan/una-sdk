#ifndef DIAL_PAINTER_HPP
#define DIAL_PAINTER_HPP

#include "lvgl.h"

#include "gui/faces/FaceAssets.hpp"

// One painter at a time; its state is file-static.
lv_obj_t* dialPainterCreate(lv_obj_t* parent, const FaceAssets& assets);
void dialPainterSetValues(const uint16_t value[3]);
void dialPainterSetFace(const FaceAssets& assets);   // resets progress and day; the caller re-sends them
void dialPainterSetDay(uint8_t mday);

#endif // DIAL_PAINTER_HPP
