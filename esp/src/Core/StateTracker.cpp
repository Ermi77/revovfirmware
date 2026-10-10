#include "StateTracker.h"

#include <string.h>

#include "StateMachine.h"
#include "../Config.h"
#include "../Protocol/PinDriver.h"

namespace revov {
namespace {

uint8_t xSlot = 0;
uint8_t yPos = SHELF_FIRST;
bool zDown = false;
const char* zAction = "";
bool doorOpen = false;
LedMode ledMode = LedMode::ON;
bool errorLatch = false;
bool ledLit = false;
bool ledApplied = false;

void applyLed() {
    const bool lit = strcmp(st_ledColorName(), "off") != 0;
    if (!ledApplied || lit != ledLit) {
        ledLit = lit;
        ledApplied = true;
        pd_setLed(lit);
    }
}

}  // namespace

void st_noteReset() {
    xSlot = 0;
    yPos = SHELF_FIRST;
    zDown = false;
    zAction = "";
    doorOpen = false;
    ledMode = LedMode::ON;
    errorLatch = false;
    applyLed();
}

void st_noteError() {
    errorLatch = true;
    applyLed();
}

void st_noteStep(uint8_t dir, uint8_t data) {
    if (dir == DIR_X) {
        xSlot = data;
        return;
    }
    if (data >= SHELF_FIRST && data <= SHELF_LAST) {
        yPos = data;
    } else if (data == ACTION_PICK) {
        // PICK settles with Z retracted (Z_DOWN_DEG).
        zDown = true;
        zAction = "PICK";
    } else if (data == ACTION_PLACE) {
        // PLACE settles with Z retracted (Z_DOWN_DEG).
        zDown = true;
        zAction = "PLACE";
    } else if (data == ACTION_DOOR_OPEN) {
        doorOpen = true;
    } else if (data == ACTION_DOOR_CLOSE) {
        doorOpen = false;
    } else if (data == SIM_Z_EXTEND) {
        // Pick-routine phase: the manipulator extends.
        zDown = false;
        zAction = "EXTEND";
    } else if (data == SIM_GRIP_CLOSE) {
        // Pick-routine phase: the drink is gripped.
        zAction = "PICK";
    } else if (data == SIM_Z_RETRACT) {
        // Pick-routine phase: the manipulator retracts,
        // settling in the same state as the atomic PICK.
        zDown = true;
        zAction = "PICK";
    }
    // Conveyor codes (ACTION_CONVEYOR_FIRST..LAST) move no
    // tracked axis.
}

void st_setLedMode(LedMode mode) {
    ledMode = mode;
    applyLed();
}

void st_updateLed() { applyLed(); }

uint8_t st_x() { return xSlot; }

uint8_t st_y() { return yPos; }

const char* st_zName() { return zDown ? "down" : "up"; }

const char* st_zActionName() { return zAction; }

const char* st_doorName() { return doorOpen ? "open" : "closed"; }

const char* st_ledColorName() {
    if (errorLatch) return "red";
    switch (ledMode) {
    case LedMode::OFF:
        return "off";
    case LedMode::ON:
    default:
        return "green";
    }
}

const char* st_ledModeName() {
    switch (ledMode) {
    case LedMode::OFF:
        return "off";
    case LedMode::ON:
    default:
        return "on";
    }
}

}  // namespace revov
