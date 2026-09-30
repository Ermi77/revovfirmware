#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace revov {

void grip_init();
void grip_pick();
void grip_place();
bool grip_isBusy();
void grip_update();
void grip_updateLimits();
void grip_stop();
void grip_zUp();
void grip_zDown();
void grip_open();
void grip_close();
long grip_zPosition();
long grip_gripPosition();
}  // namespace revov
