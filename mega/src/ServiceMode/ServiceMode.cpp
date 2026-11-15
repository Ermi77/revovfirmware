#include "ServiceMode.h"

#include "../Config.h"
#include "../Motion/Axes.h"
#include "../EndEffector/Gripper.h"
#include "../EndEffector/Door.h"
#include "../Shelf/Conveyor.h"
#include "../Homing/Home.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace revov {
namespace {

ServiceMode currentMode = ServiceMode::OFF;
TestResult lastResult;

uint32_t testStartTime = 0;
uint8_t testStep = 0;
uint8_t testSubStep = 0;
uint32_t stepStartTime = 0;
bool stepComplete = false;

uint32_t ledUpBlinkInterval = 0;
uint32_t ledDownBlinkInterval = 0;
uint32_t ledUpLastToggle = 0;
uint32_t ledDownLastToggle = 0;
bool ledUpState = false;
bool ledDownState = false;

void setLedRaw(uint8_t pin, bool on) {
#ifdef ARDUINO
    if (pin != PIN_UNUSED) digitalWrite(pin, on ? HIGH : LOW);
#endif
}

void beginStep() {
    stepStartTime = millis();
    stepComplete = false;
    testSubStep = 0;
}

bool stepDone(uint32_t timeoutMs) {
    return stepComplete || (millis() - stepStartTime >= timeoutMs);
}

void completeStep() {
    stepComplete = true;
}

bool testAxisX() {
    if (testSubStep == 0) {
        ax_homeX();
        testSubStep = 1;
    } else if (testSubStep == 1) {
        if (!ax_xBusy()) {
            ax_zeroX();
            ax_moveXSteps(X_STEPS_PER_SLOT * 2);
            testSubStep = 2;
        }
    } else if (testSubStep == 2) {
        if (!ax_xBusy()) {
            ax_moveXSteps(-X_STEPS_PER_SLOT * 2);
            testSubStep = 3;
        }
    } else if (testSubStep == 3) {
        if (!ax_xBusy()) {
            ax_zeroX();
            lastResult.xOk = true;
            return true;
        }
    }
    return false;
}

bool testAxisY() {
    if (testSubStep == 0) {
        ax_homeY();
        testSubStep = 1;
    } else if (testSubStep == 1) {
        if (!ax_yBusy()) {
            ax_zeroY();
            ax_moveYSteps(Y_STEPS_PER_SHELF * 2);
            testSubStep = 2;
        }
    } else if (testSubStep == 2) {
        if (!ax_yBusy()) {
            ax_moveYSteps(-Y_STEPS_PER_SHELF * 2);
            testSubStep = 3;
        }
    } else if (testSubStep == 3) {
        if (!ax_yBusy()) {
            ax_zeroY();
            lastResult.yOk = true;
            return true;
        }
    }
    return false;
}

bool testZ() {
    if (testSubStep == 0) {
        grip_zDown();
        testSubStep = 1;
    } else if (testSubStep == 1) {
        if (!grip_isBusy()) {
            grip_zUp();
            testSubStep = 2;
        }
    } else if (testSubStep == 2) {
        if (!grip_isBusy()) {
            lastResult.zOk = true;
            return true;
        }
    }
    return false;
}

bool testGrip() {
    if (testSubStep == 0) {
        grip_close();
        testSubStep = 1;
    } else if (testSubStep == 1) {
        if (!grip_isBusy()) {
            grip_open();
            testSubStep = 2;
        }
    } else if (testSubStep == 2) {
        if (!grip_isBusy()) {
            lastResult.gripOk = true;
            return true;
        }
    }
    return false;
}

bool testDoor() {
    if (testSubStep == 0) {
        door_open();
        testSubStep = 1;
    } else if (testSubStep == 1) {
        if (!door_isBusy()) {
            door_close();
            testSubStep = 2;
        }
    } else if (testSubStep == 2) {
        if (!door_isBusy()) {
            lastResult.doorOk = true;
            return true;
        }
    }
    return false;
}

bool testConveyors() {
    if (testSubStep == 0) {
        conv_advance(1);
        testSubStep = 1;
    } else if (testSubStep <= 5) {
        if (!conv_isBusy(testSubStep)) {
            if (testSubStep < 5) {
                testSubStep++;
                conv_advance(testSubStep);
            } else {
                for (uint8_t i = 1; i <= 5; ++i) {
                    lastResult.conveyorsOk[i-1] = true;
                }
                return true;
            }
        }
    }
    return false;
}

bool testLimits() {
    if (testSubStep == 0) {
        ax_updateLimits();
        grip_updateLimits();
        lastResult.limitsOk = true;
        return true;
    }
    return false;
}

bool testLeds() {
    if (testSubStep == 0) {
        svc_setLedUp(true);
        svc_setLedDown(true);
        svc_setLedService(true);
        testSubStep = 1;
        stepStartTime = millis();
    } else if (testSubStep == 1) {
        if (millis() - stepStartTime >= 1000) {
            svc_setLedUp(false);
            svc_setLedDown(false);
            svc_setLedService(false);
            lastResult.testTimeMs = millis() - testStartTime;
            return true;
        }
    }
    return false;
}

bool runCurrentTest() {
    switch (currentMode) {
        case ServiceMode::TEST_X: return testAxisX();
        case ServiceMode::TEST_Y: return testAxisY();
        case ServiceMode::TEST_Z: return testZ();
        case ServiceMode::TEST_GRIP: return testGrip();
        case ServiceMode::TEST_DOOR: return testDoor();
        case ServiceMode::TEST_CONVEYORS: return testConveyors();
        case ServiceMode::TEST_LIMITS: return testLimits();
        case ServiceMode::TEST_LEDS: return testLeds();
        case ServiceMode::TEST_ALL: {
            if (testStep == 0) { if (testAxisX()) testStep = 1; }
            else if (testStep == 1) { if (testAxisY()) testStep = 2; }
            else if (testStep == 2) { if (testZ()) testStep = 3; }
            else if (testStep == 3) { if (testGrip()) testStep = 4; }
            else if (testStep == 4) { if (testDoor()) testStep = 5; }
            else if (testStep == 5) { if (testConveyors()) testStep = 6; }
            else if (testStep == 6) { if (testLimits()) testStep = 7; }
            else if (testStep == 7) { if (testLeds()) return true; }
            return false;
        }
        case ServiceMode::REFILL: {
            if (testSubStep == 0) {
                ax_homeX();
                ax_homeY();
                testSubStep = 1;
            } else if (testSubStep == 1) {
                if (!ax_xBusy() && !ax_yBusy()) {
                    ax_zeroX();
                    ax_zeroY();
                    door_open();
                    testSubStep = 2;
                }
            } else if (testSubStep == 2) {
                if (!door_isBusy()) {
                    svc_setLedService(true);
                    svc_blinkLedUp(500);
                    return true;
                }
            }
            return false;
        }
        default: return true;
    }
}

}  // namespace

void svc_init() {
#ifdef ARDUINO
    if (PIN_LED_UP != PIN_UNUSED) pinMode(PIN_LED_UP, OUTPUT);
    if (PIN_LED_DOWN != PIN_UNUSED) pinMode(PIN_LED_DOWN, OUTPUT);
    if (PIN_LED_SERVICE != PIN_UNUSED) pinMode(PIN_LED_SERVICE, OUTPUT);
    setLedRaw(PIN_LED_UP, false);
    setLedRaw(PIN_LED_DOWN, false);
    setLedRaw(PIN_LED_SERVICE, false);
#endif
    currentMode = ServiceMode::OFF;
}

void svc_update() {
    uint32_t now = millis();

    if (ledUpBlinkInterval && (now - ledUpLastToggle >= ledUpBlinkInterval)) {
        ledUpState = !ledUpState;
        setLedRaw(PIN_LED_UP, ledUpState);
        ledUpLastToggle = now;
    }
    if (ledDownBlinkInterval && (now - ledDownLastToggle >= ledDownBlinkInterval)) {
        ledDownState = !ledDownState;
        setLedRaw(PIN_LED_DOWN, ledDownState);
        ledDownLastToggle = now;
    }

    if (currentMode == ServiceMode::OFF) return;

    if (!runCurrentTest()) return;

    if (currentMode == ServiceMode::TEST_ALL || currentMode == ServiceMode::REFILL) {
        lastResult.testTimeMs = millis() - testStartTime;
    }
    svc_exit();
}

void svc_enter(ServiceMode mode) {
    if (currentMode != ServiceMode::OFF) return;
    currentMode = mode;
    testStartTime = millis();
    testStep = 0;
    testSubStep = 0;
    lastResult = {};
    beginStep();

    if (mode == ServiceMode::TEST_ALL || mode == ServiceMode::REFILL) {
        svc_setLedService(true);
    }
}

void svc_exit() {
    svc_setLedUp(false);
    svc_setLedDown(false);
    svc_setLedService(false);
    ledUpBlinkInterval = 0;
    ledDownBlinkInterval = 0;
    currentMode = ServiceMode::OFF;
}

ServiceMode svc_currentMode() { return currentMode; }
bool svc_isActive() { return currentMode != ServiceMode::OFF; }

void svc_runTest() {
    if (currentMode != ServiceMode::OFF) return;
    svc_enter(ServiceMode::TEST_ALL);
}

const TestResult& svc_getLastResult() { return lastResult; }

void svc_setLedUp(bool on) {
    ledUpBlinkInterval = 0;
    ledUpState = on;
    setLedRaw(PIN_LED_UP, on);
}

void svc_setLedDown(bool on) {
    ledDownBlinkInterval = 0;
    ledDownState = on;
    setLedRaw(PIN_LED_DOWN, on);
}

void svc_setLedService(bool on) {
    setLedRaw(PIN_LED_SERVICE, on);
}

void svc_blinkLedUp(uint32_t intervalMs) {
    ledUpBlinkInterval = intervalMs;
    ledUpLastToggle = millis();
    ledUpState = true;
    setLedRaw(PIN_LED_UP, true);
}

void svc_blinkLedDown(uint32_t intervalMs) {
    ledDownBlinkInterval = intervalMs;
    ledDownLastToggle = millis();
    ledDownState = true;
    setLedRaw(PIN_LED_DOWN, true);
}

}  // namespace revov