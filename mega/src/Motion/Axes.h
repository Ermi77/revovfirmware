#pragma once

#include "StepperAxis.h"

namespace revov {

enum AxisId : uint8_t {
    AXIS_X = 0,
    AXIS_Y1 = 1,
    AXIS_Y2 = 2,
    AXIS_CONVEYOR_1 = 3,
    AXIS_CONVEYOR_2 = 4,
    AXIS_CONVEYOR_3 = 5,
    AXIS_CONVEYOR_4 = 6,
    AXIS_CONVEYOR_5 = 7,
    AXIS_COUNT = 8
};

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

// Unified absolute move for any axis
void stepper_move(uint8_t axisId, int32_t targetPosition);

}  // namespace revov
