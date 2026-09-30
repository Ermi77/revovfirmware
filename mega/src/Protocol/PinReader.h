#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace revov {

void pr_init();
uint8_t pr_direction();
uint8_t pr_data();
void pr_pulseReady();
void pr_update();
}  // namespace revov
