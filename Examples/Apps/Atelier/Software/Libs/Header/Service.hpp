#ifndef SERVICE_HPP
#define SERVICE_HPP

#include "SDK/Kernel/Kernel.hpp"
#include "SDK/SensorLayer/SensorConnection.hpp"

#include <cstdint>
#include <ctime>

class Service
{
public:
    explicit Service(SDK::Kernel &kernel)
        : mKernel(kernel)
        , mStepSensor(SDK::Sensor::Type::STEP_COUNTER_DAILY)
        , mActiveSensor(SDK::Sensor::Type::ACTIVITY_TIME_DAILY)
        , mFloorSensor(SDK::Sensor::Type::FLOOR_COUNTER_DAILY)
        , mHrSensor(SDK::Sensor::Type::HEART_RATE_METRICS_DAILY)
        , mBatterySensor(SDK::Sensor::Type::BATTERY_LEVEL)
        , mAltSensor(SDK::Sensor::Type::ALTIMETER) {}

    void run();

private:
    bool updateFace(const std::tm &local);   // true if the face changed
    void onSteps(uint32_t steps);
    void onAltitude(float metres);
    void pushStepSample();
    void disconnectAll();
    void setFace(uint8_t face);
    void publishTime(const std::tm &local);
    void publishValues();
    void publishFace();
    void refreshGoals();

    SDK::Kernel            &mKernel;
    SDK::Sensor::Connection mStepSensor;
    SDK::Sensor::Connection mActiveSensor;
    SDK::Sensor::Connection mFloorSensor;
    SDK::Sensor::Connection mHrSensor;
    SDK::Sensor::Connection mBatterySensor;
    SDK::Sensor::Connection mAltSensor;
    uint8_t                 mFace = 0;
    bool                    mFaceSet = false;
    bool                    mFaceSent = false;
    bool                    mTimeSent = false;
    uint8_t                 mHour = 0, mMinute = 0, mMday = 0;
    bool                    mValuesSent = false;
    uint16_t                mSentValues[3] = {};
    // Zero is a real value, so only these two need a flag; the other sources are "known" while their sensor is connected.
    bool                    mHaveBattery = false, mHaveAlt = false;
    uint32_t                mSteps = 0, mActive = 0, mFloors = 0, mBattery = 0;
    int32_t                 mAlt = 0;
    // Today's climb: positive altitude changes since local midnight, lost if the app restarts.
    float                   mClimb = 0, mClimbRef = 0;
    uint32_t                mAltAt = 0;
    // Daily step count sampled once a minute, oldest first; steps in the last hour = newest - oldest.
    uint32_t                mStepHist[61] = {};
    uint8_t                 mStepHistN = 0;
    int32_t                 mHistKey = -1;
    uint8_t                 mRestingHr = 0;   // 0 = no data
    uint32_t                mGoalSteps = 0, mGoalActive = 0, mGoalFloors = 0;   // 0 = no goal
    uint32_t                mGoalAt = 0;
    uint32_t                mFaceAt = 0;
    int32_t                 mDay = 0;
    int32_t                 mFixed = 0;   // last good config `face`; 0 = rotate
};

#endif // SERVICE_HPP
