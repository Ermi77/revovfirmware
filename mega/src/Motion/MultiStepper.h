#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#include <AccelStepper.h>
#endif

namespace revov {

void ms_init();
void ms_update();
void ms_handleSerial();

// Direct motor control
void ms_moveMotor(uint8_t index, long steps, float speed, bool direction);
void ms_stopMotor(uint8_t index);
void ms_emergencyStop(uint8_t index);
void ms_enableMotor(uint8_t index, bool en);

// Status
void ms_printStatus();
long ms_getPosition(uint8_t index);
long ms_getTarget(uint8_t index);
float ms_getSpeed(uint8_t index);
bool ms_isContinuous(uint8_t index);

}  // namespace revov