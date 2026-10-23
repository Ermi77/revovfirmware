#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#include <Servo.h>
#endif

namespace revov {

void sv_init();
void sv_update();

// Direct servo control
void sv_moveServo(uint8_t index, uint16_t speedDPS, int8_t direction, uint8_t amount = 180);
void sv_setPosition(uint8_t index, int angle);
void sv_stop(uint8_t index);
void sv_setLimits(uint8_t index, int minAngle, int maxAngle);

// Status
int sv_getCurrentAngle(uint8_t index);
int sv_getTargetAngle(uint8_t index);
uint16_t sv_getSpeed(uint8_t index);
bool sv_isMoving(uint8_t index);

}  // namespace revov