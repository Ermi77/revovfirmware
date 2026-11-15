#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace revov {

enum class ServiceMode : uint8_t { OFF, TEST_ALL, TEST_X, TEST_Y, TEST_Z, TEST_GRIP, TEST_DOOR, TEST_CONVEYORS, TEST_LIMITS, TEST_LEDS, REFILL };

void svc_init();
void svc_update();
void svc_enter(ServiceMode mode);
void svc_exit();
ServiceMode svc_currentMode();
bool svc_isActive();
void svc_runTest();

// LED control
void svc_setLedUp(bool on);
void svc_setLedDown(bool on);
void svc_setLedService(bool on);
void svc_blinkLedUp(uint32_t intervalMs = 500);
void svc_blinkLedDown(uint32_t intervalMs = 500);

// Test result reporting
struct TestResult {
    bool xOk = false;
    bool yOk = false;
    bool zOk = false;
    bool gripOk = false;
    bool doorOk = false;
    bool conveyorsOk[5] = {false, false, false, false, false};
    bool limitsOk = false;
    uint32_t testTimeMs = 0;
};
const TestResult& svc_getLastResult();

}  // namespace revov