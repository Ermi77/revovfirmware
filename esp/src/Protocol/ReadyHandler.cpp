#include "ReadyHandler.h"

#include "../Config.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

#ifdef ESP32
#include <esp_system.h>
#endif

namespace revov {
namespace {
volatile bool received = false;
uint32_t readyAt = 0;
bool simulated = false;
bool stalled = false;
}

void rh_init() {
    received = false;
    readyAt = 0;
#ifdef ESP32
    // Seed from the hardware TRNG so the jitter window differs per boot.
    randomSeed(static_cast<unsigned long>(esp_random()));
#endif
#ifdef ARDUINO
    pinMode(PIN_ESP_READY, INPUT);
#endif
}

void rh_setSimulation(bool enabled) { simulated = enabled; }

void rh_setStall(bool enabled) {
    stalled = enabled;
    // Cancel any READY already queued so the stall takes
    // effect on the very next primitive.
    if (stalled) readyAt = 0;
}

void rh_update() {
    if (simulated && readyAt != 0 && static_cast<int32_t>(millis() - readyAt) >= 0) {
        readyAt = 0;
        received = true;
        // The event bus owns the READY trace line; the
        // state machine emits it with the dispatch-to-
        // READY delta.
    } else if (!simulated && digitalRead(PIN_ESP_READY) == HIGH) {
        received = true;
    }
}

void rh_scheduleSimulation() {
    if (!simulated || stalled) return;
    const uint32_t span = SIM_READY_DELAY_MAX_MS - SIM_READY_DELAY_MIN_MS;
    const uint32_t jitter = span ? static_cast<uint32_t>(random(static_cast<long>(span) + 1)) : 0;
    readyAt = millis() + SIM_READY_DELAY_MIN_MS + jitter;
}

bool rh_received() {
    const bool value = received;
    received = false;
    return value;
}

}  // namespace revov
