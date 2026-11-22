#ifndef FACE_ASSETS_HPP
#define FACE_ASSETS_HPP

#include <cstdint>

struct DialSlot { uint8_t on; uint8_t onPastGoal; uint8_t off; uint8_t kind; uint8_t channel; };   // palette codes 0-63; slot index = run code - 64; kind 0 = angle slot, 1 = goal slot; channel = index of the face live value
struct HandImage { uint16_t w; uint16_t h; int16_t pivotX; int16_t pivotY; const uint8_t* px; };  // RGB565 plane then A8 plane
struct DateWindow { uint8_t x; uint8_t y; uint8_t w; uint8_t h; const uint8_t* runs; const uint16_t* start; };   // date d: pairs start[d-1]..start[d]
struct Needle { const HandImage* img; uint8_t cx; uint8_t cy; uint8_t channel; uint16_t span; int16_t base; };   // pivot = (cx, cy) on screen; rotation = base + span * value / 1000 degrees; span 0 = angle mapped in code
struct FaceAssets {
    const uint8_t* runs; uint32_t runCount;
    const DialSlot* slots; uint8_t slotCount; uint8_t ticks;   // ticks 0 = continuous 360 degree slot
    const HandImage* hour; const HandImage* minute; const HandImage* hub;
    const DateWindow* date;   // nullptr when the face has no window
    const Needle* needles; uint8_t needleCount; int16_t tilt;   // nullptr when 0
};

extern const FaceAssets kFieldAssets;
extern const FaceAssets kDiverAssets;
extern const FaceAssets kDoctorAssets;
extern const FaceAssets kTrackAssets;
extern const FaceAssets kPandaAssets;
extern const FaceAssets kTricompaxAssets;
extern const FaceAssets kDriverAssets;
extern const FaceAssets kPilotAssets;

#endif // FACE_ASSETS_HPP
