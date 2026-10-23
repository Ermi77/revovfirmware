#include "ServoController.h"

#ifdef ARDUINO
#include <Servo.h>
#endif

namespace revov {

// ---------------- Configuration ----------------
constexpr uint8_t NUM_SERVOS = 3;
const uint8_t SERVO_PIN[NUM_SERVOS] = {9, 10, 11};

// Per-servo travel limits (protects your mechanism)
int MIN_ANGLE[NUM_SERVOS] = {0,   0,   0};
int MAX_ANGLE[NUM_SERVOS] = {180, 180, 180};

// ---------------- Internal state ----------------
#ifdef ARDUINO
Servo servo[NUM_SERVOS];
#else
void* servo[NUM_SERVOS] = {nullptr};
#endif
float         curAngle[NUM_SERVOS];
float         tgtAngle[NUM_SERVOS];
float         speedDPS[NUM_SERVOS];
unsigned long lastStepMs[NUM_SERVOS];
int           lastWritten[NUM_SERVOS];

// ============================================================
//  SETUP
// ============================================================
void sv_init() {
#ifdef ARDUINO
    for (uint8_t i = 0; i < NUM_SERVOS; i++) {
        servo[i].attach(SERVO_PIN[i]);
        curAngle[i]    = 90.0f;
        tgtAngle[i]    = 90.0f;
        speedDPS[i]    = 0.0f;
        lastStepMs[i]  = millis();
        lastWritten[i] = 90;
        servo[i].write(90);
    }
    Serial.println(F("=== Servo Controller Ready ==="));
#else
    (void)SERVO_PIN;
    (void)MIN_ANGLE;
    (void)MAX_ANGLE;
#endif
}

// ============================================================
//  CORE API
// ============================================================

// motor     : 0 .. NUM_SERVOS-1
// speed     : degrees per second (e.g. 30 = slow, 180 = fast). 0 = freeze.
// direction : +1 = increasing angle, -1 = decreasing angle
// amount    : how many degrees to travel from the CURRENT position (default 180)
void sv_moveServo(uint8_t index, uint16_t speed, int8_t direction, uint8_t amount) {
#ifdef ARDUINO
    if (index >= NUM_SERVOS) return;

    int8_t dir = (direction < 0) ? -1 : 1;
    float target = curAngle[index] + (float)dir * (float)amount;

    // Clamp to the servo's safe range
    if (target > MAX_ANGLE[index]) target = MAX_ANGLE[index];
    if (target < MIN_ANGLE[index]) target = MIN_ANGLE[index];

    tgtAngle[index]   = target;
    speedDPS[index]   = speed;
    lastStepMs[index] = millis();
#else
    (void)index; (void)speed; (void)direction; (void)amount;
#endif
}

// Set absolute position directly
void sv_setPosition(uint8_t index, int angle) {
#ifdef ARDUINO
    if (index >= NUM_SERVOS) return;
    if (angle < MIN_ANGLE[index]) angle = MIN_ANGLE[index];
    if (angle > MAX_ANGLE[index]) angle = MAX_ANGLE[index];

    curAngle[index] = angle;
    tgtAngle[index] = angle;
    speedDPS[index] = 0.0f;
    lastWritten[index] = angle;
    servo[index].write(angle);
#else
    (void)index; (void)angle;
#endif
}

// Stop at current position
void sv_stop(uint8_t index) {
#ifdef ARDUINO
    if (index >= NUM_SERVOS) return;
    tgtAngle[index] = curAngle[index];
    speedDPS[index] = 0.0f;
#else
    (void)index;
#endif
}

// Set per-servo travel limits
void sv_setLimits(uint8_t index, int minAngle, int maxAngle) {
    if (index >= NUM_SERVOS) return;
    MIN_ANGLE[index] = minAngle;
    MAX_ANGLE[index] = maxAngle;

#ifdef ARDUINO
    // Clamp current target if out of new bounds
    if (tgtAngle[index] < minAngle) tgtAngle[index] = minAngle;
    if (tgtAngle[index] > maxAngle) tgtAngle[index] = maxAngle;
    if (curAngle[index] < minAngle) {
        curAngle[index] = minAngle;
        servo[index].write(minAngle);
        lastWritten[index] = minAngle;
    }
    if (curAngle[index] > maxAngle) {
        curAngle[index] = maxAngle;
        servo[index].write(maxAngle);
        lastWritten[index] = maxAngle;
    }
#endif
}

// MUST be called constantly from loop()
void sv_update() {
#ifdef ARDUINO
    unsigned long now = millis();

    for (uint8_t i = 0; i < NUM_SERVOS; i++) {
        float diff = tgtAngle[i] - curAngle[i];
        if (diff == 0.0f || speedDPS[i] == 0.0f) continue;

        unsigned long dt = now - lastStepMs[i];
        if (dt == 0) continue;

        // How far should we move in the elapsed time?
        float step = speedDPS[i] * (dt / 1000.0f);
        if (step > fabs(diff)) step = fabs(diff);

        curAngle[i] += (diff > 0.0f) ? step : -step;
        lastStepMs[i] = now;

        int a = (int)(curAngle[i] + 0.5f);
        if (a != lastWritten[i]) {
            lastWritten[i] = a;
            servo[i].write(a);
        }
    }
#endif
}

// ============================================================
//  STATUS
// ============================================================
int sv_getCurrentAngle(uint8_t index) {
#ifdef ARDUINO
    if (index >= NUM_SERVOS) return 0;
    return (int)(curAngle[index] + 0.5f);
#else
    (void)index;
    return 0;
#endif
}

int sv_getTargetAngle(uint8_t index) {
#ifdef ARDUINO
    if (index >= NUM_SERVOS) return 0;
    return (int)(tgtAngle[index] + 0.5f);
#else
    (void)index;
    return 0;
#endif
}

uint16_t sv_getSpeed(uint8_t index) {
    if (index >= NUM_SERVOS) return 0;
    return (uint16_t)speedDPS[index];
}

bool sv_isMoving(uint8_t index) {
    if (index >= NUM_SERVOS) return false;
    return tgtAngle[index] != curAngle[index] && speedDPS[index] != 0.0f;
}

}  // namespace revov