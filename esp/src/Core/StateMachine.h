#pragma once

#include <stdint.h>
#include "../Order/OrderTypes.h"

namespace revov {

enum class State : uint8_t { IDLE, DISPATCHING, WAITING_FOR_READY, ERROR };

void sm_init();
void sm_update();
State sm_state();
void sm_handleCommand(char* command);
void startOrder();
bool isReturning();
void sm_noteProgress();
bool sm_hasActiveOrder();
Order sm_activeOrder();
uint8_t sm_drinkIndex();
uint8_t sm_stepIndex();
const char* sm_stateName();

}  // namespace revov
