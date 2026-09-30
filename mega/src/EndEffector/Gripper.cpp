#include "Gripper.h"

#include "../Config.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <Servo.h>
#endif

namespace revov {
namespace {
#ifdef ARDUINO
Servo zServo;
Servo gripServo;
#endif
enum Phase : uint8_t { IDLE, Z_ONLY, PICK_UP, PICK_CLOSE, PICK_HOLD, PICK_RETRACT,
                       PLACE_UP, PLACE_OPEN, PLACE_RETRACT };
Phase phase = IDLE;
uint16_t zPosition = Z_UP_DEG;
uint16_t gripPosition = GRIP_OPEN_DEG;
uint16_t zTarget = Z_UP_DEG;
uint16_t gripTarget = GRIP_OPEN_DEG;
uint32_t zLastMs = 0;
uint32_t gripLastMs = 0;
uint32_t phaseStartedMs = 0;

void writeZ(uint16_t value) {
    zPosition = value;
#ifdef ARDUINO
    zServo.write(value);
#endif
}

void writeGrip(uint16_t value) {
    gripPosition = value;
#ifdef ARDUINO
    gripServo.write(value);
#endif
}

void commandZ(uint16_t target) {
    zTarget = target;
    zLastMs = millis();
}

void commandGrip(uint16_t target) {
    gripTarget = target;
    gripLastMs = millis();
}

bool updatePosition(uint16_t& position, uint16_t target, uint16_t speed, uint32_t& lastMs, void (*writer)(uint16_t)) {
    if (position == target) return true;
    const uint32_t now = millis();
    const uint32_t elapsed = now - lastMs;
    const uint16_t rate = speed ? speed : 1;
    const uint16_t degrees = static_cast<uint16_t>((static_cast<uint32_t>(elapsed) * rate) / 1000U);
    if (!degrees) return false;
    lastMs = now;
    const uint16_t distance = position > target ? position - target : target - position;
    const uint16_t step = degrees > distance ? distance : degrees;
    position = position > target ? position - step : position + step;
    writer(position);
    return position == target;
}
}

void grip_init() {
#ifdef ARDUINO
    zServo.attach(PIN_Z_SERVO);
    gripServo.attach(PIN_GRIPPER_SERVO);
    if (Z_EXT_LIMIT != PIN_UNUSED) pinMode(Z_EXT_LIMIT, INPUT_PULLUP);
    if (Z_RET_LIMIT != PIN_UNUSED) pinMode(Z_RET_LIMIT, INPUT_PULLUP);
    if (Z_HOME != PIN_UNUSED) pinMode(Z_HOME, INPUT_PULLUP);
#endif
    phase = IDLE;
    zTarget = Z_UP_DEG;
    gripTarget = GRIP_OPEN_DEG;
    writeZ(zPosition);
    writeGrip(gripPosition);
    zLastMs = gripLastMs = millis();
}

void grip_zUp() { if (!grip_isBusy()) { commandZ(Z_UP_DEG); phase = Z_ONLY; } }
void grip_zDown() { if (!grip_isBusy()) { commandZ(Z_DOWN_DEG); phase = Z_ONLY; } }
void grip_open() { if (!grip_isBusy()) { commandGrip(GRIP_OPEN_DEG); phase = Z_ONLY; } }
void grip_close() { if (!grip_isBusy()) { commandGrip(GRIP_CLOSE_DEG); phase = Z_ONLY; } }

void grip_pick() {
    if (!grip_isBusy()) {
        commandZ(Z_UP_DEG);
        phase = PICK_UP;
        phaseStartedMs = millis();
    }
}

void grip_place() {
    if (!grip_isBusy()) {
        commandZ(Z_UP_DEG);
        phase = PLACE_UP;
        phaseStartedMs = millis();
    }
}

bool grip_isBusy() { return phase != IDLE; }
long grip_zPosition() { return zPosition; }
long grip_gripPosition() { return gripPosition; }

void grip_stop() {
    zTarget = zPosition;
    gripTarget = gripPosition;
    phase = IDLE;
}

void grip_updateLimits() {
#ifdef ARDUINO
    const bool ext = Z_EXT_LIMIT != PIN_UNUSED && digitalRead(Z_EXT_LIMIT) == LOW;
    const bool ret = Z_RET_LIMIT != PIN_UNUSED && digitalRead(Z_RET_LIMIT) == LOW;
    const bool home = Z_HOME != PIN_UNUSED && digitalRead(Z_HOME) == LOW;
    static bool oldExt = false;
    static bool oldRet = false;
    static bool oldHome = false;
    if (ext || ret || home) {
        zTarget = zPosition;
        if (ext && !oldExt) Serial.println(F("[LIMIT] Z_EXT_LIMIT fired"));
        if (ret && !oldRet) Serial.println(F("[LIMIT] Z_RET_LIMIT fired"));
        if (home && !oldHome) Serial.println(F("[LIMIT] Z_HOME fired"));
    }
    oldExt = ext;
    oldRet = ret;
#else
    (void)Z_EXT_LIMIT;
    (void)Z_RET_LIMIT;
    (void)Z_HOME;
#endif
}

void grip_update() {
    const bool zDone = updatePosition(zPosition, zTarget, Z_SPEED_MAX_HZ, zLastMs, writeZ);
    const bool gripDone = updatePosition(gripPosition, gripTarget, GRIP_SPEED_MAX_HZ, gripLastMs, writeGrip);
    if (phase == IDLE) return;
    if (phase == Z_ONLY) {
        if (zDone && gripDone) phase = IDLE;
    } else if (phase == PICK_UP) {
        if (zDone) {
            commandGrip(GRIP_CLOSE_DEG);
            phase = PICK_CLOSE;
        }
    } else if (phase == PICK_CLOSE) {
        if (gripDone) {
            phase = PICK_HOLD;
            phaseStartedMs = millis();
        }
    } else if (phase == PICK_HOLD && millis() - phaseStartedMs >= GRIP_HOLD_MS) {
        commandZ(Z_DOWN_DEG);
        phase = PICK_RETRACT;
    } else if (phase == PICK_RETRACT && zDone) {
        phase = IDLE;
    } else if (phase == PLACE_UP) {
        if (zDone) {
            commandGrip(GRIP_OPEN_DEG);
            phase = PLACE_OPEN;
        }
    } else if (phase == PLACE_OPEN) {
        if (gripDone) {
            commandZ(Z_DOWN_DEG);
            phase = PLACE_RETRACT;
        }
    } else if (phase == PLACE_RETRACT && zDone) {
        phase = IDLE;
    }
}

}  // namespace revov
