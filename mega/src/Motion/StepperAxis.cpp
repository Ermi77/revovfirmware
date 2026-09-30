#include "StepperAxis.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace revov {

void StepperAxis::configure(uint8_t stepPin, uint8_t dirPin, uint8_t enablePin, const char* name) {
    stepPin_ = stepPin;
    dirPin_ = dirPin;
    enablePin_ = enablePin;
    name_ = name;
}

void StepperAxis::init() {
#ifdef ARDUINO
    if (stepPin_ != 255) pinMode(stepPin_, OUTPUT);
    if (dirPin_ != 255) pinMode(dirPin_, OUTPUT);
    if (enablePin_ != 255) {
        pinMode(enablePin_, OUTPUT);
        digitalWrite(enablePin_, HIGH);
    }
#endif
    lastUs_ = micros();
}

void StepperAxis::setSpeed(uint32_t value) { speed_ = value ? value : 1; }
void StepperAxis::setAcceleration(uint32_t value) { acceleration_ = value ? value : 1; }
void StepperAxis::setCurrent(uint16_t value) { current_ = value; }
void StepperAxis::setPosition(long position) {
    position_ = position;
    target_ = position;
    velocity_ = 0.0f;
    enabled_ = false;
}
void StepperAxis::move(long steps) { moveTo(position_ + steps); }
void StepperAxis::moveTo(long position) {
    target_ = position;
    enabled_ = position_ != target_;
    if (enabled_) enable(true);
}

void StepperAxis::update(uint32_t nowUs) {
    const uint32_t elapsedUs = nowUs - lastUs_;
    lastUs_ = nowUs;
    if (!enabled_) return;

    const float dt = (elapsedUs > 100000U ? 0.1f : elapsedUs / 1000000.0f);
    const long remaining = target_ - position_;
    const float direction = remaining > 0 ? 1.0f : -1.0f;
    const float desired = direction * static_cast<float>(speed_);
    const float stepVelocity = static_cast<float>(acceleration_) * dt;
    if (velocity_ < desired) velocity_ = (velocity_ + stepVelocity > desired) ? desired : velocity_ + stepVelocity;
    if (velocity_ > desired) velocity_ = (velocity_ - stepVelocity < desired) ? desired : velocity_ - stepVelocity;
    if (velocity_ > -0.5f && velocity_ < 0.5f) return;

    if (static_cast<int32_t>(nowUs - nextStepUs_) < 0) return;
    const bool forward = velocity_ > 0.0f;
#ifdef ARDUINO
    if (dirPin_ != 255) digitalWrite(dirPin_, forward ? HIGH : LOW);
    if (stepPin_ != 255) {
        digitalWrite(stepPin_, HIGH);
        digitalWrite(stepPin_, LOW);
    }
#endif
    position_ += forward ? 1 : -1;
    const float magnitude = velocity_ < 0.0f ? -velocity_ : velocity_;
    nextStepUs_ = nowUs + static_cast<uint32_t>(1000000.0f / magnitude);
    if (position_ == target_) {
        velocity_ = 0.0f;
        enabled_ = false;
        enable(false);
    }
}

bool StepperAxis::isBusy() const { return enabled_ && position_ != target_; }

void StepperAxis::stop() {
    target_ = position_;
    velocity_ = 0.0f;
    enabled_ = false;
    enable(false);
}

void StepperAxis::enable(bool enabled) {
#ifdef ARDUINO
    if (enablePin_ != 255) digitalWrite(enablePin_, enabled ? LOW : HIGH);
#else
    (void)enabled;
#endif
    enabled_ = enabled && position_ != target_;
}

}  // namespace revov
