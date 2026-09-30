#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace revov {

void rh_init();
void rh_update();
bool rh_received();
void rh_setSimulation(bool enabled);
void rh_scheduleSimulation();
}  // namespace revov
