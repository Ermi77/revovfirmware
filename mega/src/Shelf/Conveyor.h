#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "../../shared/RevoVProtocol.h"

namespace revov {

void conv_init();
void conv_advance(uint8_t shelfId);
bool conv_isBusy(uint8_t shelfId);
long conv_position(uint8_t shelfId);
void conv_update(uint32_t nowUs);
void conv_stop();
void conv_setSpeed(uint32_t hz);
StepperAxis* conv_getAxis(uint8_t shelfId); // 1-based shelf ID

}  // namespace revov
