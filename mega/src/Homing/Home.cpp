#include "Home.h"

#include "../Motion/Axes.h"
#include "../EndEffector/Gripper.h"
#include "../EndEffector/Door.h"

namespace revov {

namespace {
enum class HomePhase : uint8_t { Idle, RetractZ, HomeX, HomeY };
HomePhase phase = HomePhase::Idle;
}

void home_all() {
    if (phase != HomePhase::Idle) return;
    grip_zDown();
    phase = HomePhase::RetractZ;
}

void home_update() {
    if (phase == HomePhase::RetractZ && !grip_isBusy()) {
        ax_homeX();
        phase = HomePhase::HomeX;
    } else if (phase == HomePhase::HomeX && !ax_xBusy()) {
        ax_zeroX();
        ax_homeY();
        phase = HomePhase::HomeY;
    } else if (phase == HomePhase::HomeY && !ax_yBusy()) {
        ax_zeroY();
        door_close();
        phase = HomePhase::Idle;
    }
}

bool home_isBusy() {
    return phase != HomePhase::Idle;
}

}  // namespace revov
