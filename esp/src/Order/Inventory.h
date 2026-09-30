#pragma once

#include "OrderTypes.h"

namespace revov {

uint8_t inv_nextSlot(uint8_t shelfId);
bool inv_shouldAdvance(uint8_t shelfId);
void inv_resetSlots(uint8_t shelfId);
void inv_setNextSlot(uint8_t shelfId, uint8_t slot);
void inv_resetAll();
}  // namespace revov
