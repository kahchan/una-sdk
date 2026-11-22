#include "Service.hpp"
#include "Commands.hpp"
#include "AppConfigFields.hpp"

#include "SDK/Messages/MessageGuard.hpp"
#include "SDK/Messages/CommandMessages.hpp"
#include "SDK/Messages/MessageTypes.hpp"
#include "SDK/Messages/SensorLayerMessages.hpp"
#include "SDK/SensorLayer/DataParsers/SensorDataParserActivity.hpp"
#include "SDK/SensorLayer/DataParsers/SensorDataParserAltimeter.hpp"
#include "SDK/SensorLayer/DataParsers/SensorDataParserBatteryLevel.hpp"
#include "SDK/SensorLayer/DataParsers/SensorDataParserFloorCounter.hpp"
#include "SDK/SensorLayer/DataParsers/SensorDataParserHeartRateMetrics.hpp"
#include "SDK/SensorLayer/DataParsers/SensorDataParserStepCounter.hpp"
#include "SDK/SensorLayer/SensorDataBatch.hpp"

#include <algorithm>
#include <cmath>
#ifdef ATELIER_SIM_SAMPLES
#include <cstdio>
#include <cstdlib>
#endif

#define LOG_MODULE_PRX      "Service"
#define LOG_MODULE_LEVEL    LOG_LEVEL_INFO
#include "SDK/UnaLogger/Logger.h"

namespace {

constexpr uint32_t kStartupGraceMs = 5000;
constexpr uint32_t kPollMs = 60000;

void readLocalTime(std::tm &out)
{
    std::time_t utc = time(nullptr);
#ifdef ATELIER_SIM_SAMPLES
    // ATELIER_SIM_TIME=HH:MM[:SS] sets the clock to that time at start, then it runs on.
    static int64_t offset = 0;
    static bool offsetKnown = false;
    if (!offsetKnown) {
        offsetKnown = true;
        int h, m, s = 0;
        const char *start = std::getenv("ATELIER_SIM_TIME");
        if (start && std::sscanf(start, "%d:%d:%d", &h, &m, &s) >= 2) {
            std::tm now {};
            localtime_r(&utc, &now);
            offset = (h * 3600 + m * 60 + s) - (now.tm_hour * 3600 + now.tm_min * 60 + now.tm_sec);
        }
    }
    utc += offset;
#endif
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&out, &utc);
#else
    localtime_r(&utc, &out);
#endif
#ifdef ATELIER_SIM_SAMPLES
    if (const char *day = std::getenv("ATELIER_SIM_DAY")) {
        out.tm_mday = std::atoi(day);
    }
#endif
}

// Days since 1970-01-01 for a civil date (Hinnant's algorithm).
constexpr int32_t daysFromCivil(int32_t y, int32_t m, int32_t d)
{
    y -= m <= 2;
    const int32_t era = (y >= 0 ? y : y - 399) / 400;
    const int32_t yoe = y - era * 400;
    const int32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

int32_t localDay(const std::tm &t)
{
    return daysFromCivil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
}

// Each local day gets the next face in table order.
uint8_t faceForDay(int32_t daysSinceEpoch, uint8_t count)
{
    return static_cast<uint8_t>(daysSinceEpoch % count);   // days since 1970 are positive
}

static_assert(daysFromCivil(1970, 1, 1) == 0, "epoch");
static_assert(daysFromCivil(2028, 3, 1) - daysFromCivil(2028, 2, 28) == 2, "leap day");
static_assert(daysFromCivil(2026, 12, 1) - daysFromCivil(2026, 11, 30) == 1, "month edge");

bool usesSource(uint8_t face, CustomMessage::Source src)
{
    for (CustomMessage::Source s : CustomMessage::kFaceSources[face]) {
        if (s == src) {
            return true;
        }
    }
    return false;
}

uint16_t permille(uint32_t value, uint32_t goal)
{
    return goal > 0 ? static_cast<uint16_t>(std::min<uint64_t>(1000, uint64_t(value) * 1000 / goal)) : 0;
}

// face: 0 = rotate, 1..N = fixed face in table order. No good read yet, or a face number not built yet, rotates.
uint8_t chooseFace(SDK::Kernel &kernel, const std::tm &local, int32_t &fixed)
{
#ifdef ATELIER_SIM_SAMPLES
    if (const char *index = std::getenv("ATELIER_SIM_FACE")) {
        return static_cast<uint8_t>(std::atoi(index) % CustomMessage::FACE_COUNT);
    }
#endif
    // A fresh read each time: a running app otherwise never sees a change made on the phone.
    SDK::AppConfig config(kernel, AtelierConfig::kFileName, AtelierConfig::kFields, AtelierConfig::kFieldCount);
    // Keep the last good value: a read that lands in the middle of a phone write must not drop to rotation.
    if (config.isLoaded()) {
        fixed = config.getInt("face");
    }
    if (fixed >= 1 && fixed <= CustomMessage::FACE_COUNT) {
        return static_cast<uint8_t>(fixed - 1);
    }
    return faceForDay(localDay(local), CustomMessage::FACE_COUNT);
}

} // namespace

void Service::run()
{
    LOG_INFO("Started\n");

    {
        std::tm local {};
        readLocalTime(local);
        updateFace(local);
    }

    bool guiStarted = false;
    const uint32_t startTime = mKernel.sys.getTimeMs();

    while (true) {
        std::tm local {};
        readLocalTime(local);
        const uint32_t now = mKernel.sys.getTimeMs();
        if (localDay(local) != mDay || now - mFaceAt >= kPollMs) {
            updateFace(local);
        }
        if (now - mGoalAt >= kPollMs) {
            refreshGoals();
        }
        const int32_t histKey = mDay * 1440 + local.tm_hour * 60 + local.tm_min;
        if (histKey != mHistKey) {
            mHistKey = histKey;
            if (mStepHistN && usesSource(mFace, CustomMessage::SRC_STEPS_HOUR)) {   // the first sample comes from onSteps
                pushStepSample();
            }
        }
        publishFace();
        publishTime(local);
        publishValues();

        // Always lands in the first second of the next minute, never zero.
        uint32_t wait = (60u - static_cast<uint32_t>(local.tm_sec) % 60u) * 1000u;

        if (!guiStarted) {
            const uint32_t elapsed = mKernel.sys.getTimeMs() - startTime;
            if (elapsed >= kStartupGraceMs) {
                LOG_INFO("GUI never started, exiting service\n");
                break;
            }
            if ((kStartupGraceMs - elapsed) < wait) {
                wait = kStartupGraceMs - elapsed;
            }
        }

        SDK::MessageBase *msg;
        if (!mKernel.comm.getMessage(msg, wait)) {
            continue;
        }

        bool done = false;
        switch (msg->getType()) {
            case SDK::MessageType::COMMAND_APP_NOTIF_GUI_RUN:
                guiStarted = true;
                refreshGoals();
                break;

            case CustomMessage::REFRESH:
                // The GUI's queue evicts old messages while suspended; the loop top re-sends.
                mFaceSent = mTimeSent = mValuesSent = false;
                {
                    std::tm local {};
                    readLocalTime(local);
                    if (!updateFace(local)) {   // setFace reads the goals itself
                        refreshGoals();
                    }
                }
                break;

            case SDK::MessageType::EVENT_SENSOR_LAYER_DATA: {
                auto *event = static_cast<SDK::Message::Sensor::EventData *>(msg);
                SDK::Sensor::DataBatch batch(event->data, event->count, event->stride);
                if (batch.size() == 0) {
                    break;
                }
                if (mStepSensor.matchesDriver(event->handle)) {
                    SDK::SensorDataParser::StepCounter parser(batch[batch.size() - 1]);
                    if (parser.isDataValid()) {
                        onSteps(parser.getStepCount());
                    }
                } else if (mActiveSensor.matchesDriver(event->handle)) {
                    SDK::SensorDataParser::Activity parser(batch[batch.size() - 1]);
                    if (parser.isDataValid()) {
                        mActive = parser.getDuration();
                    }
                } else if (mFloorSensor.matchesDriver(event->handle)) {
                    SDK::SensorDataParser::FloorCounter parser(batch[batch.size() - 1]);
                    if (parser.isDataValid()) {
                        mFloors = static_cast<uint32_t>(std::max<int32_t>(0, parser.getFloorsUp()));
                    }
                } else if (mHrSensor.matchesDriver(event->handle)) {
                    SDK::SensorDataParser::HeartRateMetrics parser(batch[batch.size() - 1]);
                    if (parser.isDataValid()) {
                        mRestingHr = static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, std::round(parser.getRhr()))));
                    }
                } else if (mBatterySensor.matchesDriver(event->handle)) {
                    SDK::SensorDataParser::BatteryLevel parser(batch[batch.size() - 1]);
                    if (parser.isDataValid()) {
                        mBattery = static_cast<uint32_t>(std::max(0.0f, std::round(parser.getCharge())));
                        mHaveBattery = true;
                    }
                } else if (mAltSensor.matchesDriver(event->handle)) {
                    SDK::SensorDataParser::Altimeter parser(batch[batch.size() - 1]);
                    if (parser.isDataValid()) {
                        onAltitude(parser.getAltitude());
                    }
                }
            } break;

            case SDK::MessageType::COMMAND_APP_STOP:
            case SDK::MessageType::COMMAND_APP_NOTIF_GUI_STOP:
                LOG_INFO("Exiting\n");
                done = true;
                break;

            default:
                break;
        }
        mKernel.comm.releaseMessage(msg);
        if (done) {
            break;
        }
    }

    disconnectAll();
}

void Service::disconnectAll()
{
    mStepSensor.disconnect();
    mActiveSensor.disconnect();
    mFloorSensor.disconnect();
    mHrSensor.disconnect();
    mBatterySensor.disconnect();
    mAltSensor.disconnect();
}

bool Service::updateFace(const std::tm &local)
{
    mFaceAt = mKernel.sys.getTimeMs();
    const int32_t day = localDay(local);
    if (mDay != 0 && day != mDay) {
        mClimb = 0;   // a new local day; the daily step counter resets on its own
    }
    mDay = day;
    const uint8_t wanted = chooseFace(mKernel, local, mFixed);
    if (!mFaceSet || wanted != mFace) {
        LOG_INFO("face %u -> %u\n", static_cast<unsigned>(mFace), static_cast<unsigned>(wanted));
        mFaceSet = true;
        setFace(wanted);
        return true;
    }
    return false;
}

void Service::onSteps(uint32_t steps)
{
    mSteps = steps;
    // The counter dropped: it reset at midnight. Start the hour again from here.
    if (mStepHistN && steps < mStepHist[mStepHistN - 1]) {
        mStepHistN = 0;
    }
    if (mStepHistN == 0) {
        pushStepSample();
    }
}

void Service::pushStepSample()
{
    // 61 samples one minute apart: newest minus oldest covers 60 minutes.
    if (mStepHistN == 61) {
        std::copy(mStepHist + 1, mStepHist + 61, mStepHist);
        --mStepHistN;
    }
    mStepHist[mStepHistN++] = mSteps;
}

// Rises smaller than the hysteresis are noise, and the reference follows descents, which add nothing.
// A jump no climber could make in the time since the last reading is a new baseline, not climb.
void Service::onAltitude(float metres)
{
    constexpr float kClimbHysteresisM = 3.0f;
    constexpr float kMaxRiseMps = 5.0f;
    const uint32_t now = mKernel.sys.getTimeMs();
    const float dt = (now - mAltAt) / 1000.0f;
    mAltAt = now;
    mAlt = static_cast<int32_t>(std::lround(metres));
    if (!mHaveAlt || std::fabs(metres - mClimbRef) > kMaxRiseMps * dt + kClimbHysteresisM) {
        mClimbRef = metres;
        mHaveAlt = true;
        return;
    }
    if (metres > mClimbRef + kClimbHysteresisM) {
        mClimb += metres - mClimbRef;
        mClimbRef = metres;
    } else if (metres < mClimbRef - kClimbHysteresisM) {
        mClimbRef = metres;
    }
}

void Service::setFace(uint8_t face)
{
    disconnectAll();
    mFace = face;
    mHaveAlt = false;   // the next reading becomes the climb baseline
    mStepHistN = 0;   // samples taken while another face was up would span a gap
    // Subscribe only to what the face draws: an unused subscription costs power for nothing.
    for (CustomMessage::Source src : CustomMessage::kFaceSources[face]) {
        switch (src) {
            case CustomMessage::SRC_STEPS:
            case CustomMessage::SRC_STEPS_HOUR: mStepSensor.connect(); break;
            case CustomMessage::SRC_ACTIVE:     mActiveSensor.connect(); break;
            case CustomMessage::SRC_FLOORS:     mFloorSensor.connect(); break;
            case CustomMessage::SRC_RHR:        mHrSensor.connect(); break;
            case CustomMessage::SRC_BATTERY:    mBatterySensor.connect(); break;
            case CustomMessage::SRC_ALT_MOD:
            case CustomMessage::SRC_CLIMB:      mAltSensor.connect(); break;
            default: break;
        }
    }
    // A Face message is always followed by everything the new face shows.
    mFaceSent = mTimeSent = mValuesSent = false;
    refreshGoals();
}

void Service::refreshGoals()
{
    // Stamped before the request so a silent kernel is retried next interval, not every turn.
    mGoalAt = mKernel.sys.getTimeMs();
    if (!(usesSource(mFace, CustomMessage::SRC_STEPS) || usesSource(mFace, CustomMessage::SRC_ACTIVE) || usesSource(mFace, CustomMessage::SRC_FLOORS))) { return; }
    if (auto msg = SDK::make_msg<SDK::Message::RequestSystemSettings>(mKernel)) {
        if (msg.send(100) && msg.ok()) {
            mGoalSteps = msg->steps;
            mGoalActive = msg->activityMin;
            mGoalFloors = msg->floors;
        }
    }
}

void Service::publishTime(const std::tm &local)
{
    const uint8_t h = static_cast<uint8_t>(local.tm_hour);
    const uint8_t m = static_cast<uint8_t>(local.tm_min);
    const uint8_t d = static_cast<uint8_t>(local.tm_mday);
    if (mTimeSent && h == mHour && m == mMinute && d == mMday) {
        return;
    }
    mHour = h; mMinute = m; mMday = d; mTimeSent = true;
    SDK::send_msg<CustomMessage::Time>(mKernel, h, m, d);
}

void Service::publishFace()
{
    if (mFaceSent) {
        return;
    }
    mFaceSent = true;
    SDK::send_msg<CustomMessage::Face>(mKernel, mFace);
}

void Service::publishValues()
{
    using namespace CustomMessage;
    uint32_t value[SRC_COUNT] = {}, goal[SRC_COUNT] = {};
    bool known[SRC_COUNT] = {};
    value[SRC_STEPS] = mSteps;           goal[SRC_STEPS] = mGoalSteps;      known[SRC_STEPS] = mStepSensor.isConnected();
    value[SRC_ACTIVE] = mActive;         goal[SRC_ACTIVE] = mGoalActive;    known[SRC_ACTIVE] = mActiveSensor.isConnected();
    value[SRC_FLOORS] = mFloors;         goal[SRC_FLOORS] = mGoalFloors;    known[SRC_FLOORS] = mFloorSensor.isConnected();
    value[SRC_RHR] = mRestingHr;         known[SRC_RHR] = mRestingHr > 0;   // 0 bpm means no reading
    value[SRC_BATTERY] = mBattery;       goal[SRC_BATTERY] = 100;           known[SRC_BATTERY] = mHaveBattery;
    // Newest minus oldest sample; the sample count grows to 61 after start, with no history before it.
    value[SRC_STEPS_HOUR] = mStepHistN ? mSteps - mStepHist[0] : 0;
    goal[SRC_STEPS_HOUR] = 1000;         known[SRC_STEPS_HOUR] = mStepSensor.isConnected();
    // The altitude wraps every 1000 m so the needle never sticks at 12.
    value[SRC_ALT_MOD] = ((mAlt % 1000) + 1000) % 1000;
    goal[SRC_ALT_MOD] = 1000;            known[SRC_ALT_MOD] = mHaveAlt;
    value[SRC_CLIMB] = static_cast<uint32_t>(mClimb);
    goal[SRC_CLIMB] = 1000;              known[SRC_CLIMB] = mHaveAlt;
#ifdef ATELIER_SIM_SAMPLES
    auto force = [&](const char *name, Source src, uint32_t g) {
        if (const char *v = std::getenv(name)) {
            value[src] = std::atoi(v); goal[src] = g; known[src] = true;
        }
    };
    force("ATELIER_SIM_STEPS", SRC_STEPS, 10000);
    force("ATELIER_SIM_ACTIVE", SRC_ACTIVE, 30);
    force("ATELIER_SIM_FLOORS", SRC_FLOORS, 10);
    if (const char *v = std::getenv("ATELIER_SIM_HR")) {
        if (std::atoi(v) > 0) {   // 0 would mean no reading
            value[SRC_RHR] = std::atoi(v); known[SRC_RHR] = true;
        }
    }
    force("ATELIER_SIM_BATTERY", SRC_BATTERY, 100);
    force("ATELIER_SIM_STEPSHOUR", SRC_STEPS_HOUR, 1000);
    if (const char *v = std::getenv("ATELIER_SIM_ALT")) {
        value[SRC_ALT_MOD] = ((std::atoi(v) % 1000) + 1000) % 1000; known[SRC_ALT_MOD] = known[SRC_CLIMB] = true;
    }
    force("ATELIER_SIM_CLIMB", SRC_CLIMB, 1000);
    if (std::getenv("ATELIER_SIM_NODATA")) {
        known[SRC_ALT_MOD] = known[SRC_CLIMB] = known[SRC_RHR] = false;
    }
#endif
    uint16_t p[3];
    for (int c = 0; c < 3; ++c) {
        const Source src = kFaceSources[mFace][c];
        // Doctor sends raw bpm; every other source is progress against its goal.
        p[c] = src == SRC_NONE ? 0 : !known[src] ? kNoData : src == SRC_RHR ? static_cast<uint16_t>(value[src]) : permille(value[src], goal[src]);
    }
    if (mValuesSent && std::equal(p, p + 3, mSentValues)) {
        return;
    }
    std::copy(p, p + 3, mSentValues);
    mValuesSent = true;
#ifdef ATELIER_SIM_SAMPLES
    LOG_INFO("values -> %u %u %u permille (steps %lu/%lu battery %lu hour %lu alt %ld climb %lu)\n",
             (unsigned)p[0], (unsigned)p[1], (unsigned)p[2], (unsigned long)value[SRC_STEPS], (unsigned long)goal[SRC_STEPS],
             (unsigned long)value[SRC_BATTERY], (unsigned long)value[SRC_STEPS_HOUR], (long)mAlt, (unsigned long)value[SRC_CLIMB]);
#endif
    SDK::send_msg<Values>(mKernel, p[0], p[1], p[2]);
}
