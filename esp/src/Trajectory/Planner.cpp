#include "Planner.h"

#include "../../shared/RevoVProtocol.h"
#include "../Config.h"

namespace revov {
namespace {

// Tracked manipulator position. int8_t: -1 = unknown
// / needs homing. Updated only after a READY pulse,
// so a primitive that times out and is retried never
// moves the tracked position.
int8_t curX_slot = -1;
int8_t curY_shelf = -1;

// Fills the fixed eight-entry plan for one drink.
// The skip flags are cleared here; the smart planner
// sets them in its filter pass.
void fillPlan(uint8_t shelf, uint8_t startSlot, Cmd* steps) {
    steps[0] = {'X', static_cast<int8_t>(startSlot),
                "move X to slot", false};
    steps[1] = {'Y', static_cast<int8_t>(shelf),
                "move Y to shelf", false};
    steps[2] = {'Y', ACTION_PICK, "PICK", false};
    steps[3] = {'Y', DISPENSE_SHELF,
                "move Y to dispense shelf", false};
    steps[4] = {'X', DISPENSE_SLOT,
                "move X to dispense slot", false};
    steps[5] = {'Y', ACTION_DOOR_OPEN, "DOOR OPEN", false};
    steps[6] = {'Y', ACTION_PLACE, "PLACE", false};
    steps[7] = {'Y', ACTION_DOOR_CLOSE, "DOOR CLOSE", false};
}

// A Y entry is a shelf move (1..SHELF_COUNT); every
// other Y code is an action (PICK, PLACE, door) that
// never changes the shelf position.
bool isShelfMove(int8_t data) {
    return data >= SHELF_FIRST && data <= SHELF_LAST;
}

}  // namespace

uint8_t planOrder(uint8_t shelfId, uint8_t startSlot, Cmd* steps,
                   uint8_t maxSteps) {
    if (!steps || maxSteps < PLAN_SIZE || shelfId < SHELF_FIRST ||
        shelfId > SHELF_LAST || startSlot < SLOT_FIRST || startSlot > SLOT_LAST) {
        return 0;
    }
    fillPlan(shelfId, startSlot, steps);
    return PLAN_SIZE;
}

#if SMART_PLANNER

uint8_t planOrderSmart(uint8_t shelfId, uint8_t startSlot, Cmd* steps,
                        uint8_t maxSteps) {
    if (!steps || maxSteps < PLAN_SIZE || shelfId < SHELF_FIRST ||
        shelfId > SHELF_LAST || startSlot < SLOT_FIRST || startSlot > SLOT_LAST) {
        return 0;
    }
    fillPlan(shelfId, startSlot, steps);
    // Filter pass: forward-simulate the plan against
    // the tracked position. A move whose target already
    // equals the position is a no-op and is flagged for
    // skipping; the simulation advances only on sent
    // moves, so each entry is judged at the position the
    // machine will actually be in when it runs. The
    // predicate is the one pl_shouldMoveX /
    // pl_shouldMoveY expose, applied to the simulated
    // position: an unknown position (-1) never equals a
    // valid target, so it always moves.
    int8_t x = curX_slot;
    int8_t y = curY_shelf;
    uint8_t sent = 0;
    for (uint8_t i = 0; i < PLAN_SIZE; ++i) {
        Cmd& step = steps[i];
        if (step.axis == 'X') {
            if (x != step.data) {
                x = step.data;
                ++sent;
            } else {
                step.skip = true;
            }
        } else if (isShelfMove(step.data)) {
            if (y != step.data) {
                y = step.data;
                ++sent;
            } else {
                step.skip = true;
            }
        } else {
            // Action commands (PICK, PLACE, door) always
            // run and are never skipped.
            ++sent;
        }
    }
    return sent;
}

void pl_resetTracking() {
    // Boot, home sequence, and E-stop: the position is
    // unknown until endstops or a settled move confirm
    // it. Never trust stale tracking after a home.
    curX_slot = -1;
    curY_shelf = -1;
}

void pl_noteSettled(uint8_t dir, uint8_t data) {
    if (dir == DIR_X) {
        curX_slot = static_cast<int8_t>(data);
        return;
    }
    // Only a shelf move changes the Y position; action
    // codes (PICK, PLACE, door) leave the manipulator
    // on the same shelf.
    if (isShelfMove(static_cast<int8_t>(data))) {
        curY_shelf = static_cast<int8_t>(data);
    }
}

void pl_noteConveyorSettled() {
    // The belt shifted every column by one slot, so the
    // X column mapping resets to the dispensing area.
    // The manipulator itself did not move: the shelf it
    // sits on is unchanged.
    curX_slot = 0;
}

bool pl_shouldMoveX(int8_t targetSlot) {
    return curX_slot != targetSlot;
}

bool pl_shouldMoveY(int8_t targetShelf) {
    return curY_shelf != targetShelf;
}

int8_t pl_curXSlot() { return curX_slot; }

int8_t pl_curYShelf() { return curY_shelf; }

#endif  // SMART_PLANNER

}  // namespace revov
