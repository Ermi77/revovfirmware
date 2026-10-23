#include <Arduino.h>

#include "Config.h"
#include "Core/Scheduler.h"
#include "Core/StateMachine.h"
#include "EndEffector/Door.h"
#include "EndEffector/Gripper.h"
#include "Homing/Home.h"
#include "Motion/Axes.h"
#include "Motion/MultiStepper.h"
#include "Motion/ServoController.h"
#include "Protocol/PinReader.h"
#include "Shelf/Conveyor.h"

// TODO: implement (Milestone M1)
void setup() {
    Serial.begin(115200);
    revov::pr_init();
    revov::ax_init();
    revov::conv_init();
    revov::door_init();
    revov::grip_init();
    revov::ms_init();
    revov::sv_init();
    revov::sm_init();
    revov::sched_init();
}

void loop() {
    revov::ms_handleSerial();
    revov::ms_update();
    revov::sv_update();
    revov::sm_update();
    revov::sched_update(micros());
}
