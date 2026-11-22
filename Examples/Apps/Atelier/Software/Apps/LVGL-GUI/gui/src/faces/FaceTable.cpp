#include "gui/faces/FaceTable.hpp"

#include "Commands.hpp"

namespace {

// Snail scale: 6 * 1800 / bpm degrees.
int32_t doctorNeedle(uint16_t bpm)
{
    const int32_t hr = bpm < 30 ? 30 : (bpm > 200 ? 200 : bpm);
    return (108000 + hr / 2) / hr;
}

} // namespace

const FaceEntry kFaces[] = {
    { &kFieldAssets, nullptr },
    { &kDiverAssets, nullptr },
    { &kDoctorAssets, doctorNeedle },
    { &kTrackAssets, nullptr },
    { &kPandaAssets, nullptr },
    { &kTricompaxAssets, nullptr },
    { &kDriverAssets, nullptr },
    { &kPilotAssets, nullptr },
};
static_assert(sizeof(kFaces) / sizeof(kFaces[0]) == CustomMessage::FACE_COUNT, "face table out of step with Faces");
