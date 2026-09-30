#include "PinReader.h"

#include "../Config.h"

namespace revov {

namespace {
uint32_t readyStarted = 0;
bool readyActive = false;
uint8_t lastDir = 0;
uint8_t lastData = 0;
uint32_t stableSince = 0;
}

void pr_init() {
    stableSince = millis();
}

uint8_t pr_direction() {
    return lastDir;
}

uint8_t pr_data() {
    return lastData;
}

void pr_pulseReady() {
    readyActive = true;
    readyStarted = millis();
}

void pr_update() {
    if (readyActive && millis() - readyStarted >= READY_PULSE_MS) {
        readyActive = false;
        // PinReader is intentionally a stub until the ESP wiring is installed.
    }
}

}  // namespace revov
