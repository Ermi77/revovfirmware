#include "Door.h"

#include "../Config.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <Servo.h>
#endif

namespace revov {
namespace {
#ifdef ARDUINO
Servo servo;
#endif
uint16_t position = DOOR_CLOSED_DEG;
uint16_t target = DOOR_CLOSED_DEG;
uint32_t lastMs = 0;
bool busy = false;
}

void door_init() {
#ifdef ARDUINO
    servo.attach(PIN_DOOR_SERVO);
#endif
    position = target = DOOR_CLOSED_DEG;
    busy = false;
    lastMs = millis();
#ifdef ARDUINO
    servo.write(position);
#endif
}

static void command(uint16_t value) {
    target = value;
    busy = position != target;
    lastMs = millis();
}

void door_open() { command(DOOR_OPEN_DEG); }
void door_close() { command(DOOR_CLOSED_DEG); }
bool door_isBusy() { return busy; }

void door_update() {
    if (!busy) return;
    const uint32_t now = millis();
    const uint16_t degrees = static_cast<uint16_t>((static_cast<uint32_t>(now - lastMs) * DOOR_SPEED_MAX_HZ) / 1000U);
    if (!degrees) return;
    lastMs = now;
    const uint16_t distance = position > target ? position - target : target - position;
    const uint16_t step = degrees > distance ? distance : degrees;
    position = position > target ? position - step : position + step;
#ifdef ARDUINO
    servo.write(position);
#endif
    busy = position != target;
}

void door_stop() {
    target = position;
    busy = false;
}

long door_position() { return position; }

}  // namespace revov
