#include "Scheduler.h"

#include "../EndEffector/Door.h"
#include "../EndEffector/Gripper.h"
#include "../Motion/Axes.h"
#include "../Protocol/PinReader.h"
#include "../Shelf/Conveyor.h"
#include "../Homing/Home.h"

namespace revov {

void sched_init() {}
void sched_run() {
    sched_update(micros());
}
void sched_update(uint32_t nowUs) {
    ax_update(nowUs);
    grip_update();
    door_update();
    conv_update(nowUs);
    home_update();
    ax_updateLimits();
    grip_updateLimits();
    pr_update();
}

}  // namespace revov
