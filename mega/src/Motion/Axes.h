#pragma once

#include "StepperAxis.h"

namespace revov {

void ax_init();
void ax_moveX(uint8_t slot);
void ax_moveXSteps(long steps);
void ax_moveY(uint8_t shelf);
void ax_moveYSteps(long steps);
void ax_homeX();
void ax_homeY();
bool ax_isBusy();
void ax_update(uint32_t nowUs);
void ax_updateLimits();
void ax_stopAll();
void ax_stopX();
void ax_stopY();
void ax_zeroX();
void ax_zeroY();
void ax_setSpeed(uint32_t stepsPerSecond);
void ax_setXSpeed(uint32_t stepsPerSecond);
void ax_setYSpeed(uint32_t stepsPerSecond);
void ax_setAcceleration(uint32_t stepsPerSecond2);
long ax_xPosition();
long ax_yPosition();
bool ax_xBusy();
bool ax_yBusy();
}  // namespace revov
