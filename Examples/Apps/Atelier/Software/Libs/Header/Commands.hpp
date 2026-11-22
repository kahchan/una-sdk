#ifndef COMMANDS_HPP
#define COMMANDS_HPP

#include "SDK/Messages/MessageBase.hpp"
#include "SDK/Messages/MessageTypes.hpp"

#include <cstdint>

#pragma pack(push, 4)

namespace CustomMessage {

// Service --> GUI
constexpr SDK::MessageType::Type TIME     = 0x00000001;
constexpr SDK::MessageType::Type VALUES   = 0x00000002;
constexpr SDK::MessageType::Type FACE     = 0x00000003;

// Index into the GUI's face table.
enum Faces : uint8_t { FACE_FIELD, FACE_DIVER, FACE_DOCTOR, FACE_TRACK, FACE_PANDA, FACE_TRICOMPAX, FACE_DRIVER, FACE_PILOT, FACE_COUNT };

// Where each Values channel of a face comes from, indexed by Faces. Matches the generated `channel` fields.
enum Source : uint8_t { SRC_NONE, SRC_STEPS, SRC_ACTIVE, SRC_FLOORS, SRC_RHR, SRC_BATTERY, SRC_STEPS_HOUR, SRC_ALT_MOD, SRC_CLIMB, SRC_COUNT };

// A Values channel whose source has no reading (no sensor, or none yet): needles hide, rims stay unlit.
// Zero is always a real value.
constexpr uint16_t kNoData = 0xFFFF;
constexpr Source kFaceSources[][3] = {
    { SRC_STEPS,  SRC_NONE,   SRC_NONE   },   // Field
    { SRC_STEPS,  SRC_NONE,   SRC_NONE   },   // Diver
    { SRC_RHR,    SRC_NONE,   SRC_NONE   },   // Doctor
    { SRC_ACTIVE, SRC_FLOORS, SRC_NONE   },   // Track
    { SRC_STEPS,  SRC_ACTIVE, SRC_NONE   },   // Panda
    { SRC_STEPS,  SRC_FLOORS, SRC_ACTIVE },   // Tricompax
    { SRC_BATTERY, SRC_STEPS_HOUR, SRC_NONE }, // Driver
    { SRC_ALT_MOD, SRC_CLIMB, SRC_NONE   },   // Pilot
};
static_assert(sizeof(kFaceSources) / sizeof(kFaceSources[0]) == FACE_COUNT, "kFaceSources out of step with Faces");

// GUI --> Service
constexpr SDK::MessageType::Type REFRESH  = 0x00000010;

struct Time : public SDK::MessageBase {
    uint8_t hour;
    uint8_t minute;
    uint8_t mday;

    Time() : SDK::MessageBase(TIME), hour(0), minute(0), mday(0) {}
    Time(uint8_t h, uint8_t m, uint8_t d) : Time() { hour = h; minute = m; mday = d; }
};

/// One value per channel (see kFaceSources): progress against the goal in permille, capped at 1000
/// (0 also when there is no goal), or raw bpm for Doctor. kNoData when the source has no reading.
struct Values : public SDK::MessageBase {
    uint16_t value[3];

    Values() : SDK::MessageBase(VALUES), value {} {}
    Values(uint16_t a, uint16_t b, uint16_t c) : Values() { value[0] = a; value[1] = b; value[2] = c; }
};

struct Face : public SDK::MessageBase {
    uint8_t index;

    Face() : SDK::MessageBase(FACE), index(0) {}
    explicit Face(uint8_t i) : Face() { index = i; }
};

/// Asks the service to re-send everything; sent on resume.
struct Refresh : public SDK::MessageBase {
    Refresh() : SDK::MessageBase(REFRESH) {}
};

} // namespace CustomMessage

#pragma pack(pop)

// The kernel drops anything over 256 bytes silently.
static_assert(sizeof(CustomMessage::Time) <= 256, "message pool limit");
static_assert(sizeof(CustomMessage::Values) <= 256, "message pool limit");
static_assert(sizeof(CustomMessage::Face) <= 256, "message pool limit");
static_assert(sizeof(CustomMessage::Refresh) <= 256, "message pool limit");

#endif // COMMANDS_HPP
