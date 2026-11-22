#ifndef MODEL_LISTENER_HPP
#define MODEL_LISTENER_HPP

#include <cstdint>

class ModelListener
{
public:
    virtual ~ModelListener() = default;
    virtual void onTime(uint8_t hour, uint8_t minute, uint8_t mday) = 0;
    virtual void onValues(const uint16_t* value) = 0;   // three channels
    virtual void onFace(uint8_t index) = 0;
};

#endif // MODEL_LISTENER_HPP
