#include "MultiStepper.h"

#ifdef ARDUINO
#include <AccelStepper.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#endif

namespace revov {

// ---------------- Configuration ----------------
constexpr uint8_t NUM_MOTORS = 8;

// Pin map: index -> {STEP, DIR, ENA}
const uint8_t STEP_PINS[NUM_MOTORS] = { 2,  5,  8, 11, 22, 25, 28, 31};
const uint8_t DIR_PINS [NUM_MOTORS] = { 3,  6,  9, 12, 23, 26, 29, 32};
const uint8_t ENA_PINS [NUM_MOTORS] = { 4,  7, 10, 13, 24, 27, 30, 33};

constexpr long  STEPS_PER_REV     = 1600;    // 1/8 microstep
constexpr float DEFAULT_MAX_SPEED = 2000.0f; // steps/s
constexpr float DEFAULT_ACCEL     = 4000.0f; // steps/s^2

// ---------------- Globals ----------------
#ifdef ARDUINO
AccelStepper* motors[NUM_MOTORS];
#else
void* motors[NUM_MOTORS] = {nullptr};
#endif
bool  continuousMode[NUM_MOTORS];
float continuousSpeed[NUM_MOTORS];

char    lineBuf[48];
uint8_t lineLen = 0;

// ============================================================
//  SETUP
// ============================================================
void ms_init() {
#ifdef ARDUINO
    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        motors[i] = new AccelStepper(AccelStepper::DRIVER, STEP_PINS[i], DIR_PINS[i]);
        motors[i]->setMaxSpeed(DEFAULT_MAX_SPEED);
        motors[i]->setAcceleration(DEFAULT_ACCEL);
        motors[i]->setCurrentPosition(0);

        continuousMode[i]  = false;
        continuousSpeed[i] = 0.0f;

        pinMode(ENA_PINS[i], OUTPUT);
        digitalWrite(ENA_PINS[i], LOW);   // LOW = enabled on 2DM860
    }
    Serial.println(F("=== Multi-Stepper Controller Ready ==="));
#else
    (void)STEP_PINS;
    (void)DIR_PINS;
    (void)ENA_PINS;
    (void)STEPS_PER_REV;
    (void)DEFAULT_MAX_SPEED;
    (void)DEFAULT_ACCEL;
#endif
}

// ============================================================
//  CORE API
// ============================================================

// Move one motor.
//   index     : 0 .. NUM_MOTORS-1
//   steps     : magnitude in steps; 0 = continuous rotation
//   speed     : magnitude in steps/s
//   direction : true = CW (+), false = CCW (-)
void ms_moveMotor(uint8_t index, long steps, float speed, bool direction) {
#ifdef ARDUINO
    if (index >= NUM_MOTORS) return;
    AccelStepper* m = motors[index];

    if (speed <= 0)                speed = 1.0f;
    if (speed > DEFAULT_MAX_SPEED) speed = DEFAULT_MAX_SPEED;

    m->setMaxSpeed(speed);

    if (steps == 0) {
        // Continuous rotation at constant speed (no acceleration ramp)
        continuousMode[index]  = true;
        continuousSpeed[index] = direction ? speed : -speed;
        m->setSpeed(continuousSpeed[index]);
    } else {
        // Positioned move with acceleration ramp
        continuousMode[index] = false;
        m->move(direction ? steps : -steps);   // relative to current target
    }
#else
    (void)index; (void)steps; (void)speed; (void)direction;
#endif
}

// Decelerate to a stop (keeps position)
void ms_stopMotor(uint8_t index) {
#ifdef ARDUINO
    if (index >= NUM_MOTORS) return;
    continuousMode[index]  = false;
    continuousSpeed[index] = 0.0f;
    motors[index]->stop();
#else
    (void)index;
#endif
}

// Hard halt — clears target and speed instantly
void ms_emergencyStop(uint8_t index) {
#ifdef ARDUINO
    if (index >= NUM_MOTORS) return;
    continuousMode[index]  = false;
    continuousSpeed[index] = 0.0f;
    AccelStepper* m = motors[index];
    // setCurrentPosition() also resets _targetPos and _speed to 0
    m->setCurrentPosition(m->currentPosition());
#else
    (void)index;
#endif
}

// Enable / disable driver outputs
void ms_enableMotor(uint8_t index, bool en) {
#ifdef ARDUINO
    if (index >= NUM_MOTORS) return;
    digitalWrite(ENA_PINS[index], en ? LOW : HIGH);
    if (en) motors[index]->enableOutputs();
    else    motors[index]->disableOutputs();
#else
    (void)index; (void)en;
#endif
}

// MUST be called as fast as possible in loop()
void ms_update() {
#ifdef ARDUINO
    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        if (continuousMode[i]) motors[i]->runSpeed();
        else                   motors[i]->run();
    }
#endif
}

// ============================================================
//  SERIAL INTERFACE
// ============================================================
void ms_handleSerial() {
#ifdef ARDUINO
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (lineLen > 0) {
                lineBuf[lineLen] = '\0';
                ms_parseCommand(lineBuf);
                lineLen = 0;
            }
        } else if (lineLen < sizeof(lineBuf) - 1) {
            lineBuf[lineLen++] = c;
        }
    }
#else
    (void)lineBuf;
    (void)lineLen;
#endif
}

#ifdef ARDUINO
void ms_parseCommand(char* s) {
    char* t[5];
    uint8_t n = 0;
    char* p = strtok(s, " ,\t");
    while (p && n < 5) { t[n++] = p; p = strtok(NULL, " ,\t"); }
    if (n == 0) return;

    // lowercase the first token for keyword comparison
    for (char* c = t[0]; *c; ++c) *c = tolower(*c);

    if (!strcmp(t[0], "stop") && n >= 2)  { ms_stopMotor(atoi(t[1]));         Serial.println(F("OK stop"));  return; }
    if (!strcmp(t[0], "estop") && n >= 2) { ms_emergencyStop(atoi(t[1]));     Serial.println(F("OK estop")); return; }
    if (!strcmp(t[0], "en") && n >= 3)    { ms_enableMotor(atoi(t[1]), atoi(t[2]) != 0); Serial.println(F("OK en")); return; }
    if (!strcmp(t[0], "status"))          { ms_printStatus();                 return; }

    // Positional / velocity move:  <motor> <steps> <speed> <dir>
    if (n < 4) { Serial.println(F("ERR: <motor> <steps> <speed> <dir>")); return; }

    uint8_t idx = (uint8_t)atoi(t[0]);
    long    stp = atol(t[1]);
    float   spd = atof(t[2]);

    char d0 = tolower(t[3][0]);
    char d1 = tolower(t[3][1]);
    bool dir;
    if      (d0 == '+')                 dir = true;
    else if (d0 == '-')                 dir = false;
    else if (d0 == 'c' && d1 == 'c')    dir = false;   // CCW
    else                                dir = true;    // CW / C

    ms_moveMotor(idx, stp, spd, dir);

    Serial.print(F("OK M"));   Serial.print(idx);
    Serial.print(F(" steps=")); Serial.print(stp);
    Serial.print(F(" speed=")); Serial.print(spd, 0);
    Serial.print(F(" dir="));   Serial.println(dir ? F("CW") : F("CCW"));
}
#endif

// Status getters
long ms_getPosition(uint8_t index) {
#ifdef ARDUINO
    if (index >= NUM_MOTORS) return 0;
    return motors[index]->currentPosition();
#else
    (void)index;
    return 0;
#endif
}

long ms_getTarget(uint8_t index) {
#ifdef ARDUINO
    if (index >= NUM_MOTORS) return 0;
    return motors[index]->targetPosition();
#else
    (void)index;
    return 0;
#endif
}

float ms_getSpeed(uint8_t index) {
#ifdef ARDUINO
    if (index >= NUM_MOTORS) return 0.0f;
    return motors[index]->speed();
#else
    (void)index;
    return 0.0f;
#endif
}

bool ms_isContinuous(uint8_t index) {
    if (index >= NUM_MOTORS) return false;
    return continuousMode[index];
}

void ms_printStatus() {
#ifdef ARDUINO
    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        Serial.print(F("M")); Serial.print(i);
        Serial.print(F(" pos=")); Serial.print(motors[i]->currentPosition());
        Serial.print(F(" tgt=")); Serial.print(motors[i]->targetPosition());
        Serial.print(F(" spd=")); Serial.print(motors[i]->speed(), 0);
        Serial.print(F(" mode=")); Serial.println(continuousMode[i] ? F("cont") : F("pos"));
    }
#endif
}

}  // namespace revov