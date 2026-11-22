#ifndef FACE_TABLE_HPP
#define FACE_TABLE_HPP

#include "gui/faces/FaceAssets.hpp"

struct FaceEntry {
    const FaceAssets* assets;
    int32_t (*needle0)(uint16_t value);   // for a span 0 needle: angle in 0.1 degree for channel 0; nullptr = none
};

// Indexed by CustomMessage::Faces.
extern const FaceEntry kFaces[];

#endif // FACE_TABLE_HPP
