#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace revov {

void pd_init();
void pd_send(uint8_t dir, uint8_t data);
bool pd_waitReady(uint32_t timeoutMs);
bool pd_readyReceived();
void pd_setSimulation(bool enabled);
bool pd_isSimulation();
}  // namespace revov
