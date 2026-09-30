#pragma once

#include <stdint.h>

namespace revov {

class StepperAxis {
public:
    void configure(uint8_t stepPin, uint8_t dirPin, uint8_t enablePin, const char* name);
    void init();
    void setSpeed(uint32_t stepsPerSecond);
    void setAcceleration(uint32_t stepsPerSecond2);
    void setCurrent(uint16_t milliamps);
    void move(long steps);
    void moveTo(long position);
    void update(uint32_t nowUs);
    bool isBusy() const;
    void stop();
    void enable(bool enabled);
    long position() const { return position_; }
    void setPosition(long position);
    uint32_t speed() const { return speed_; }

private:
    uint8_t stepPin_ = 255;
    uint8_t dirPin_ = 255;
    uint8_t enablePin_ = 255;
    const char* name_ = nullptr;
    long position_ = 0;
    long target_ = 0;
    uint32_t speed_ = 1;
    uint32_t acceleration_ = 1;
    uint16_t current_ = 0;
    float velocity_ = 0.0f;
    uint32_t lastUs_ = 0;
    uint32_t nextStepUs_ = 0;
    bool enabled_ = false;
};

}  // namespace revov
