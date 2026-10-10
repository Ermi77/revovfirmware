#include "PinDriver.h"

#include "../Config.h"
#include "ReadyHandler.h"

namespace revov {
namespace {
bool simulation = false;

}

void pd_init() {
#ifdef ARDUINO
    pinMode(PIN_ESP_DIR, OUTPUT);
    pinMode(PIN_ESP_DATA0, OUTPUT);
    pinMode(PIN_ESP_DATA1, OUTPUT);
    pinMode(PIN_ESP_DATA2, OUTPUT);
    pinMode(PIN_ESP_DATA3, OUTPUT);
    pinMode(PIN_ESP_STROBE, OUTPUT);
    pinMode(PIN_ESP_READY, INPUT);
    pinMode(PIN_LED, OUTPUT);
#endif
    rh_setSimulation(simulation);
}

void pd_setSimulation(bool enabled) {
    simulation = enabled;
    rh_setSimulation(enabled);
}

void pd_setStall(bool enabled) {
    rh_setStall(enabled);
}

void pd_setLed(bool on) {
#ifdef ARDUINO
    digitalWrite(PIN_LED, on ? HIGH : LOW);
#else
    (void)on;
#endif
}

void pd_signalReady() {
    // Operator READY button: arms the simulated READY
    // the machine is holding for. A no-op in production,
    // where the real Mega signals READY on the input pin.
    rh_scheduleSimulation();
}

bool pd_isSimulation() { return simulation; }

void pd_send(uint8_t dir, uint8_t data) {
#ifdef SIM_MODE
    if (simulation) {
        // The event bus owns the command trace line. The
        // READY is operator-gated: the machine holds each
        // primitive until the READY button releases it
        // through pd_signalReady(), so the simulation
        // schedules nothing here.
        return;
    }
#endif
#ifdef ARDUINO
    digitalWrite(PIN_ESP_DIR, dir != 0 ? HIGH : LOW);
    digitalWrite(PIN_ESP_DATA0, (data >> 0) & 1);
    digitalWrite(PIN_ESP_DATA1, (data >> 1) & 1);
    digitalWrite(PIN_ESP_DATA2, (data >> 2) & 1);
    digitalWrite(PIN_ESP_DATA3, (data >> 3) & 1);
    digitalWrite(PIN_ESP_STROBE, HIGH);
#else
    (void)dir;
    (void)data;
#endif
}

bool pd_waitReady(uint32_t timeoutMs) {
    const uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        rh_update();
        if (pd_readyReceived()) return true;
    }
    return false;
}

bool pd_readyReceived() {
    rh_update();
    return rh_received();
}

}  // namespace revov
