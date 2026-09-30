#include "Planner.h"

#include "../../shared/RevoVProtocol.h"
#include "../Config.h"

namespace revov {

uint8_t planOrder(const Order& order, ActionStep* steps, uint8_t maxSteps) {
    if (!steps || maxSteps < 8 || order.shelfId < 1 || order.shelfId > SHELF_COUNT ||
        order.quantity == 0 || order.startSlot >= SLOTS_PER_SHELF) {
        return 0;
    }
    const uint8_t shelf = static_cast<uint8_t>(order.shelfId - 1);
    steps[0] = {DIR_X, order.startSlot, "move X to slot"};
    steps[1] = {DIR_Y, shelf, "move Y to shelf"};
    steps[2] = {DIR_Y, ACTION_PICK, "PICK"};
    steps[3] = {DIR_Y, DISPENSE_SHELF, "move Y to dispense shelf"};
    steps[4] = {DIR_X, DISPENSE_SLOT, "move X to dispense slot"};
    steps[5] = {DIR_Y, ACTION_DOOR_OPEN, "DOOR OPEN"};
    steps[6] = {DIR_Y, ACTION_PLACE, "PLACE"};
    steps[7] = {DIR_Y, ACTION_DOOR_CLOSE, "DOOR CLOSE"};
    return 8;
}

}  // namespace revov
