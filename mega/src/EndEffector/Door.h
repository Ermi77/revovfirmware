#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace revov {

void door_init();
void door_open();
void door_close();
bool door_isBusy();
void door_update();
void door_stop();
long door_position();
}  // namespace revov
