#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace revov {

void sched_init();
void sched_run();
void sched_update(uint32_t nowUs);
}  // namespace revov
