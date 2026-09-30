#pragma once

#include "ActionStep.h"
#include "../Order/OrderTypes.h"

namespace revov {

uint8_t planOrder(const Order& order, ActionStep* steps, uint8_t maxSteps);
}  // namespace revov
