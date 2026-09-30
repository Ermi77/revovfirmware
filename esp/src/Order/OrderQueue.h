#pragma once

#include "OrderTypes.h"

namespace revov {

void oq_enqueue(uint8_t shelfId, uint8_t qty, uint8_t startSlot);
void oq_enqueueOrder(const Order& order);
bool oq_hasNext();
Order oq_peek();
void oq_markDispensed();
void oq_clear();
uint8_t oq_size();
}  // namespace revov
