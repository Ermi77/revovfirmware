#include "StateMachine.h"
#include "../Config.h"
#include "../EndEffector/Door.h"
#include "../EndEffector/Gripper.h"
#include "../Motion/Axes.h"
#include "../Shelf/Conveyor.h"
#include "../Homing/Home.h"
#include <Arduino.h>
#include <string.h>
#include <stdlib.h>

namespace revov {
namespace {
char line[CMD_MAX_LENGTH], original[CMD_MAX_LENGTH];
uint8_t length = 0;
bool active = false;
uint8_t activeShelf = 0;
MotionState state = MotionState::Idle;
void upper(char* s) { for (; *s; ++s) if (*s >= 'a' && *s <= 'z') *s -= 'a' - 'A'; }
bool number(const char* s) {
    if (!s || !*s) return false;
    for (; *s; ++s) if (*s < '0' || *s > '9') return false;
    return true;
}
void begin(uint8_t shelf = 0) {
    active = true; activeShelf = shelf;
    state = MotionState::Running;
}
bool settled() {
    return !ax_isBusy() && !grip_isBusy() && !door_isBusy() &&
        !home_isBusy() &&
        (!activeShelf || !conv_isBusy(activeShelf));
}
void error(const __FlashStringHelper* text) { Serial.print(F("ERR ")); Serial.println(text); }
void execute(char* text) {
    char* op = strtok(text, " ");
    char* a = strtok(nullptr, " ");
    char* b = strtok(nullptr, " ");
    if (!op) return;
    if (!strcmp(op, "HELP")) {
        Serial.println(F("help; x <slot>; xf/xb <steps>; y <shelf>; yf/yb <steps>; z up/down; grip close/open; pick/place; door open/close; conv 1..5; home; status; speed x/y/conv <hz>; stop"));
        return;
    }
    if (!strcmp(op, "STATUS")) {
#ifdef BENCH_MODE
        Serial.print(F("STATUS x=")); Serial.print(ax_xPosition());
        Serial.print(F(" y=")); Serial.print(ax_yPosition());
        Serial.print(F(" z=")); Serial.print(grip_zPosition());
        Serial.print(F(" grip=")); Serial.print(grip_gripPosition());
        Serial.print(F(" door=")); Serial.print(door_position());
        Serial.print(F(" busy=")); Serial.print(active ? 1 : 0);
        Serial.print(F(" xbusy=")); Serial.print(ax_xBusy());
        Serial.print(F(" ybusy=")); Serial.print(ax_yBusy());
        Serial.print(F(" zbusy=")); Serial.print(grip_isBusy());
        Serial.print(F(" doorbusy=")); Serial.print(door_isBusy());
        Serial.print(F(" convbusy="));
        for (uint8_t i = 1; i <= SHELF_COUNT; ++i) {
            if (i > 1) Serial.print(',');
            Serial.print(conv_isBusy(i) ? 1 : 0);
        }
        Serial.print(F(" convpos="));
        for (uint8_t i = 1; i <= SHELF_COUNT; ++i) {
            if (i > 1) Serial.print(',');
            Serial.print(conv_position(i));
        }
        Serial.println();
        return;
#else
        error(F("bench mode disabled"));
        return;
#endif
    }
    if (!strcmp(op, "STOP")) {
        ax_stopAll(); grip_stop(); door_stop(); conv_stop();
        active = false; activeShelf = 0; state = MotionState::EmergencyStop;
        Serial.print(F("OK ")); Serial.println(original);
        return;
    }
    if (!strcmp(op, "HOME")) { home_all(); begin(); return; }
    if (!strcmp(op, "X") && number(a)) {
        const uint8_t slot = atoi(a);
        if (slot >= SLOTS_PER_SHELF) { error(F("x slot")); return; }
        ax_moveX(slot); begin(); return;
    }
    if ((!strcmp(op, "XF") || !strcmp(op, "XB")) && number(a)) {
        long steps = atol(a); if (!strcmp(op, "XB")) steps = -steps;
        ax_moveXSteps(steps); begin(); return;
    }
    if (!strcmp(op, "Y") && number(a)) {
        const uint8_t shelf = atoi(a);
        if (shelf == 15) { home_all(); begin(); return; }
        if (shelf >= SHELF_COUNT) { error(F("y shelf")); return; }
        ax_moveY(shelf); begin(); return;
    }
    if ((!strcmp(op, "YF") || !strcmp(op, "YB")) && number(a)) {
        long steps = atol(a); if (!strcmp(op, "YB")) steps = -steps;
        ax_moveYSteps(steps); begin(); return;
    }
    if (!strcmp(op, "Z") && a) {
        if (!strcmp(a, "UP")) grip_zUp();
        else if (!strcmp(a, "DOWN")) grip_zDown();
        else { error(F("z direction")); return; }
        begin(); return;
    }
    if (!strcmp(op, "GRIP") && a) {
        if (!strcmp(a, "OPEN")) grip_open();
        else if (!strcmp(a, "CLOSE")) grip_close();
        else { error(F("grip action")); return; }
        begin(); return;
    }
    if (!strcmp(op, "PICK")) { grip_pick(); begin(); return; }
    if (!strcmp(op, "PLACE")) { grip_place(); begin(); return; }
    if (!strcmp(op, "DOOR") && a) {
        if (!strcmp(a, "OPEN")) door_open();
        else if (!strcmp(a, "CLOSE")) door_close();
        else { error(F("door action")); return; }
        begin(); return;
    }
    if (!strcmp(op, "CONV") && number(a)) {
        const uint8_t shelf = atoi(a);
        if (shelf < 1 || shelf > SHELF_COUNT) { error(F("conveyor")); return; }
        conv_advance(shelf); begin(shelf); return;
    }
    if (!strcmp(op, "SPEED") && a && number(b)) {
#ifdef BENCH_MODE
        const uint32_t hz = strtoul(b, nullptr, 10);
        if (!strcmp(a, "X")) ax_setXSpeed(hz);
        else if (!strcmp(a, "Y")) ax_setYSpeed(hz);
        else if (!strcmp(a, "CONV")) conv_setSpeed(hz);
        else { error(F("speed axis")); return; }
        Serial.print(F("OK ")); Serial.println(original); return;
#else
        error(F("bench mode disabled"));
        return;
#endif
    }
    error(F("command"));
}
}
void sm_init() { active = false; length = 0; state = MotionState::Idle; }
MotionState sm_state() { return state; }
void sm_update() {
    while (Serial.available()) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r') continue;
        if (c == '\n') {
            line[length] = '\0'; strncpy(original, line, sizeof(original) - 1);
            original[sizeof(original) - 1] = '\0';
            upper(line);
            if (active && strncmp(line, "STOP", 4)) error(F("busy"));
            else execute(line);
            length = 0;
        } else if (length + 1U < CMD_MAX_LENGTH) line[length++] = c;
    }
    if (active && settled()) {
        Serial.print(F("OK ")); Serial.println(original);
        active = false; activeShelf = 0; state = MotionState::Idle;
    }
}
}
