#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "../Order/OrderTypes.h"

namespace revov {

bool ws_connect();
void ws_maintain();
bool ws_pollOrder(Order out[], uint8_t& count);
bool ws_isConnected();
bool ws_isConfigured();
bool ws_publishDrinkOutcome(const char* orderId, uint8_t drinkIndex,
                            const char* status);
void diagnoseConnectionError();
void checkBackendContact();
uint32_t ws_lastContact();

}  // namespace revov
