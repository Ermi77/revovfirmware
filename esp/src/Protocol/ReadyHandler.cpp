#include "ReadyHandler.h"

#include "../Config.h"

namespace revov {
namespace {
volatile bool received = false;
uint32_t readyAt = 0;
bool simulated = false;
}

void rh_init() {
    received = false;
    readyAt = 0;
#ifdef ARDUINO
    pinMode(PIN_ESP_READY, INPUT);
#endif
}

void rh_setSimulation(bool enabled) { simulated = enabled; }

void rh_update() {
    if (simulated && readyAt != 0 && static_cast<int32_t>(millis() - readyAt) >= 0) {
        readyAt = 0;
        received = true;
        Serial.println(F("[SIM] <- Mega: READY"));
    } else if (!simulated && digitalRead(PIN_ESP_READY) == HIGH) {
        received = true;
    }
}

void rh_scheduleSimulation() {
    if (simulated) readyAt = millis() + SIM_READY_DELAY_MS;
}

bool rh_received() {
    const bool value = received;
    received = false;
    return value;
}

}  // namespace revov
