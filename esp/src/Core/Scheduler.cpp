#include "Scheduler.h"

#include "../Net/WebSocket.h"
#include "../Protocol/ReadyHandler.h"

namespace revov {

void sched_init() { rh_init(); }
void sched_update() {
    ws_maintain();
    rh_update();
}

}  // namespace revov
