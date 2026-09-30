#include "Axes.h"

#include "../Config.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace revov {

namespace {
StepperAxis xAxis;
StepperAxis yAxis1;
StepperAxis yAxis2;

bool switchActive(uint8_t pin) {
#ifdef ARDUINO
   return pin != PIN_UNUSED && digitalRead(pin) == LOW;
#else
   (void)pin;
   return false;
#endif
}

void logSwitch(const __FlashStringHelper* name) {
#ifdef ARDUINO
   Serial.print(F("[LIMIT] "));
   Serial.print(name);
   Serial.println(F(" fired"));
#else
   (void)name;
#endif
}

void setupSwitch(uint8_t pin) {
#ifdef ARDUINO
   if (pin != PIN_UNUSED) pinMode(pin, INPUT_PULLUP);
#else
   (void)pin;
#endif
}

bool previousXHome = false;
bool previousYTop = false;
bool previousYBottom = false;
bool previousYHome = false;
bool previousShelfLeft[SHELF_COUNT] = {};
bool previousShelfRight[SHELF_COUNT] = {};
bool previousDispense = false;
}

void ax_init() {
   xAxis.configure(PIN_X_STEP, PIN_X_DIR, PIN_X_ENABLE, "X");
   yAxis1.configure(PIN_Y1_STEP, PIN_Y1_DIR, PIN_Y_ENABLE, "Y1");
   yAxis2.configure(PIN_Y2_STEP, PIN_Y2_DIR, PIN_Y_ENABLE, "Y2");
   xAxis.setSpeed(X_SPEED_MAX_HZ);
   xAxis.setAcceleration(X_ACCELERATION);
   xAxis.setCurrent(X_CURRENT_MA);
   yAxis1.setSpeed(Y_SPEED_MAX_HZ);
   yAxis1.setAcceleration(Y_ACCELERATION);
   yAxis1.setCurrent(Y_CURRENT_MA);
   yAxis2.setSpeed(Y_SPEED_MAX_HZ);
   yAxis2.setAcceleration(Y_ACCELERATION);
   yAxis2.setCurrent(Y_CURRENT_MA);
   xAxis.init();
   yAxis1.init();
   yAxis2.init();
   setupSwitch(X_HOME);
   setupSwitch(Y_TOP_LIMIT);
   setupSwitch(Y_BOTTOM_LIMIT);
   setupSwitch(Y_HOME);
   const uint8_t left[] = {SHELF1_L_LIMIT, SHELF2_L_LIMIT, SHELF3_L_LIMIT,
                            SHELF4_L_LIMIT, SHELF5_L_LIMIT};
   const uint8_t right[] = {SHELF1_R_LIMIT, SHELF2_R_LIMIT, SHELF3_R_LIMIT,
                             SHELF4_R_LIMIT, SHELF5_R_LIMIT};
   for (uint8_t i = 0; i < SHELF_COUNT; ++i) {
       setupSwitch(left[i]);
       setupSwitch(right[i]);
   }
   setupSwitch(DISPENSE_SWITCH);
}
void ax_moveX(uint8_t slot) {
    if (slot < SLOTS_PER_SHELF) xAxis.moveTo(static_cast<long>(slot) * X_STEPS_PER_SLOT);
}
void ax_moveXSteps(long steps) { xAxis.move(steps); }
void ax_moveY(uint8_t shelf) {
   if (shelf < SHELF_COUNT) {
       const long target = static_cast<long>(shelf) * Y_STEPS_PER_SHELF;
       yAxis1.moveTo(target);
       yAxis2.moveTo(target);
   }
}
void ax_moveYSteps(long steps) { yAxis1.move(steps); yAxis2.move(steps); }
void ax_homeX() { xAxis.moveTo(0); }
void ax_homeY() { yAxis1.moveTo(0); yAxis2.moveTo(0); }
bool ax_isBusy() { return xAxis.isBusy() || yAxis1.isBusy() || yAxis2.isBusy(); }
void ax_update(uint32_t nowUs) { xAxis.update(nowUs); yAxis1.update(nowUs); yAxis2.update(nowUs); }
void ax_updateLimits() {
   const bool xHome = switchActive(X_HOME);
   const bool yTop = switchActive(Y_TOP_LIMIT);
   const bool yBottom = switchActive(Y_BOTTOM_LIMIT);
   const bool yHome = switchActive(Y_HOME);
   if (xHome && !previousXHome) { xAxis.stop(); logSwitch(F("X_HOME")); }
   if ((yTop && !previousYTop) || (yBottom && !previousYBottom) ||
       (yHome && !previousYHome)) {
       yAxis1.stop();
       yAxis2.stop();
       if (yTop && !previousYTop) logSwitch(F("Y_TOP_LIMIT"));
       if (yBottom && !previousYBottom) logSwitch(F("Y_BOTTOM_LIMIT"));
       if (yHome && !previousYHome) logSwitch(F("Y_HOME"));
   }
   const uint8_t left[] = {SHELF1_L_LIMIT, SHELF2_L_LIMIT, SHELF3_L_LIMIT,
                           SHELF4_L_LIMIT, SHELF5_L_LIMIT};
   const uint8_t right[] = {SHELF1_R_LIMIT, SHELF2_R_LIMIT, SHELF3_R_LIMIT,
                            SHELF4_R_LIMIT, SHELF5_R_LIMIT};
   for (uint8_t i = 0; i < SHELF_COUNT; ++i) {
       const bool leftActive = switchActive(left[i]);
       const bool rightActive = switchActive(right[i]);
       if (leftActive && !previousShelfLeft[i]) {
           xAxis.stop();
           logSwitch(i == 0 ? F("SHELF1_L_LIMIT") :
                     i == 1 ? F("SHELF2_L_LIMIT") :
                     i == 2 ? F("SHELF3_L_LIMIT") :
                     i == 3 ? F("SHELF4_L_LIMIT") : F("SHELF5_L_LIMIT"));
       }
       if (rightActive && !previousShelfRight[i]) {
           xAxis.stop();
           logSwitch(i == 0 ? F("SHELF1_R_LIMIT") :
                     i == 1 ? F("SHELF2_R_LIMIT") :
                     i == 2 ? F("SHELF3_R_LIMIT") :
                     i == 3 ? F("SHELF4_R_LIMIT") : F("SHELF5_R_LIMIT"));
       }
       previousShelfLeft[i] = leftActive;
       previousShelfRight[i] = rightActive;
   }
   const bool dispense = switchActive(DISPENSE_SWITCH);
   if (dispense && !previousDispense) logSwitch(F("DISPENSE_SWITCH shelf 3"));
   previousXHome = xHome;
   previousYTop = yTop;
   previousYBottom = yBottom;
   previousYHome = yHome;
   previousDispense = dispense;
}
void ax_stopAll() { xAxis.stop(); yAxis1.stop(); yAxis2.stop(); }
void ax_stopX() { xAxis.stop(); }
void ax_stopY() { yAxis1.stop(); yAxis2.stop(); }
void ax_zeroX() { xAxis.setPosition(0); }
void ax_zeroY() { yAxis1.setPosition(0); yAxis2.setPosition(0); }
void ax_setSpeed(uint32_t value) { xAxis.setSpeed(value); ax_setYSpeed(value); }
void ax_setXSpeed(uint32_t value) { xAxis.setSpeed(value); }
void ax_setYSpeed(uint32_t value) { yAxis1.setSpeed(value); yAxis2.setSpeed(value); }
void ax_setAcceleration(uint32_t value) {
   xAxis.setAcceleration(value);
   yAxis1.setAcceleration(value);
   yAxis2.setAcceleration(value);
}
long ax_xPosition() { return xAxis.position(); }
long ax_yPosition() { return yAxis1.position(); }
bool ax_xBusy() { return xAxis.isBusy(); }
bool ax_yBusy() { return yAxis1.isBusy() || yAxis2.isBusy(); }

}  // namespace revov
