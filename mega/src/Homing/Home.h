#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace revov {

void home_all();
void home_update();
bool home_isBusy();
}  // namespace revov
