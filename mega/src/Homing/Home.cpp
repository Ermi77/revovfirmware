#include "Home.h"

#include "../Motion/Axes.h"
#include "../EndEffector/Gripper.h"
#include "../EndEffector/Door.h"
#include "../Config.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace revov {

namespace {
enum class HomePhase : uint8_t { Idle, RetractZ, HomeX, HomeY };
enum class HomeXSubPhase : uint8_t { MoveToLeft, Backoff, Zero };
enum class HomeYSubPhase : uint8_t { MoveToDown, Backoff, Zero };
HomePhase phase = HomePhase::Idle;
HomeXSubPhase xSubPhase = HomeXSubPhase::MoveToLeft;
HomeYSubPhase ySubPhase = HomeYSubPhase::MoveToDown;
}

void home_all() {
    if (phase != HomePhase::Idle) return;
    grip_zDown();
    phase = HomePhase::RetractZ;
}

void home_update() {
    if (phase == HomePhase::RetractZ && !grip_isBusy()) {
        // Move X towards left limit switch
        xSubPhase = HomeXSubPhase::MoveToLeft;
        ax_moveXSteps(-100000); // Large negative move towards left
        phase = HomePhase::HomeX;
    } else if (phase == HomePhase::HomeX) {
        const bool xLeft = digitalRead(X_LEFT_LIMIT) == LOW;
        if (xSubPhase == HomeXSubPhase::MoveToLeft) {
            if (xLeft) {
                ax_stopX();
                xSubPhase = HomeXSubPhase::Backoff;
                ax_moveXSteps(500); // Back off slightly
            }
        } else if (xSubPhase == HomeXSubPhase::Backoff) {
            if (!ax_xBusy()) {
                xSubPhase = HomeXSubPhase::Zero;
                ax_zeroX();
            }
        } else if (xSubPhase == HomeXSubPhase::Zero) {
            // Move Y towards down limit switch
            ySubPhase = HomeYSubPhase::MoveToDown;
            ax_moveYSteps(-100000); // Large negative move towards down
            phase = HomePhase::HomeY;
        }
    } else if (phase == HomePhase::HomeY) {
        const bool yDown = digitalRead(Y_DOWN_LIMIT) == LOW;
        if (ySubPhase == HomeYSubPhase::MoveToDown) {
            if (yDown) {
                ax_stopY();
                ySubPhase = HomeYSubPhase::Backoff;
                ax_moveYSteps(500); // Back off slightly
            }
        } else if (ySubPhase == HomeYSubPhase::Backoff) {
            if (!ax_yBusy()) {
                ySubPhase = HomeYSubPhase::Zero;
                ax_zeroY();
            }
        } else if (ySubPhase == HomeYSubPhase::Zero) {
            door_close();
            phase = HomePhase::Idle;
        }
    }
}

bool home_isBusy() {
    return phase != HomePhase::Idle;
}

}  // namespace revov
