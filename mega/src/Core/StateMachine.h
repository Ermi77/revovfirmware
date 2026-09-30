#pragma once

#include <stdint.h>

namespace revov {

enum class MotionState : uint8_t { Idle, Running, EmergencyStop };

void sm_init();
void sm_update();
MotionState sm_state();

}  // namespace revov
