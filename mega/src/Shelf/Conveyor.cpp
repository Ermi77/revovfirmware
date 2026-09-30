#include "Conveyor.h"

#include "../../shared/RevoVProtocol.h"
#include "../Motion/StepperAxis.h"

#include "../Config.h"

namespace revov {

namespace { StepperAxis conveyors[SHELF_COUNT]; }

void conv_init() {
    for (uint8_t i = 0; i < SHELF_COUNT; ++i) {
        const uint8_t steps[] = {PIN_CONVEYOR1_STEP, PIN_CONVEYOR2_STEP, PIN_CONVEYOR3_STEP, PIN_CONVEYOR4_STEP, PIN_CONVEYOR5_STEP};
        const uint8_t dirs[] = {PIN_CONVEYOR1_DIR, PIN_CONVEYOR2_DIR, PIN_CONVEYOR3_DIR, PIN_CONVEYOR4_DIR, PIN_CONVEYOR5_DIR};
        conveyors[i].configure(steps[i], dirs[i], PIN_CONVEYOR_ENABLE, "conveyor");
        conveyors[i].setSpeed(CONVEYOR_SPEED_MAX_HZ); conveyors[i].setAcceleration(CONVEYOR_ACCELERATION);
        conveyors[i].setCurrent(CONVEYOR_CURRENT_MA);
        conveyors[i].init();
    }
}

void conv_advance(uint8_t shelfId) {
    if (shelfId >= 1 && shelfId <= SHELF_COUNT) {
        conveyors[shelfId - 1].move(CONVEYOR_STEPS_PER_COLUMN);
    }
}

bool conv_isBusy(uint8_t shelfId) {
    return shelfId >= 1 && shelfId <= SHELF_COUNT && conveyors[shelfId - 1].isBusy();
}
long conv_position(uint8_t shelfId) {
    return shelfId >= 1 && shelfId <= SHELF_COUNT ? conveyors[shelfId - 1].position() : 0;
}

void conv_update(uint32_t nowUs) {
    for (uint8_t i = 0; i < SHELF_COUNT; ++i) conveyors[i].update(nowUs);
}
void conv_stop() {
    for (uint8_t i = 0; i < SHELF_COUNT; ++i) conveyors[i].stop();
}
void conv_setSpeed(uint32_t hz) {
    for (uint8_t i = 0; i < SHELF_COUNT; ++i) conveyors[i].setSpeed(hz);
}

}  // namespace revov
