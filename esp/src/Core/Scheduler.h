#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace revov {

void sched_init();
void sched_update();
}  // namespace revov
