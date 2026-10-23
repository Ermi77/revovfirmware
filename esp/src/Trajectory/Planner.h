#pragma once

#include "ActionStep.h"
#include "../Order/OrderTypes.h"

namespace revov {

// Smart planner switch. 1 (default) drops no-op moves
// from the dispatched stream; 0 restores the legacy
// fixed 8-step stream. Override from build_flags
// (-DSMART_PLANNER=0) for A/B testing and debugging.
#ifndef SMART_PLANNER
#define SMART_PLANNER 1
#endif

// Plan length: one drink is always eight entries.
constexpr uint8_t PLAN_SIZE = 8;

// Legacy fixed 8-step plan: every command is emitted.
// Plans a single drink using explicit parameters (for multi-item orders).
uint8_t planOrder(uint8_t shelfId, uint8_t startSlot, Cmd* steps, uint8_t maxSteps);

#if SMART_PLANNER

// Smart plan: builds the same eight entries, then flags
// every move that is a no-op at the tracked position
// (the manipulator is already at the target). Returns
// the effective number of commands that will be sent;
// flagged entries stay in the array so the dispatcher
// traces them in plan order.
uint8_t planOrderSmart(uint8_t shelfId, uint8_t startSlot, Cmd* steps,
                       uint8_t maxSteps);

// Tracked manipulator position. int8_t: -1 = unknown /
// needs homing. An unknown position always moves — the
// planner never assumes where the machine is.
void pl_resetTracking();                     // boot, home, E-stop
void pl_noteSettled(uint8_t dir, uint8_t data);  // after a READY pulse
void pl_noteConveyorSettled();               // after a conveyor advance
bool pl_shouldMoveX(int8_t targetSlot);
bool pl_shouldMoveY(int8_t targetShelf);
int8_t pl_curXSlot();
int8_t pl_curYShelf();

#endif

// Backward-compatible wrappers for legacy single-item orders
inline uint8_t planOrder(const Order& order, Cmd* steps, uint8_t maxSteps) {
    return planOrder(orderEffectiveShelf(order), orderEffectiveStartSlot(order), steps, maxSteps);
}

#if SMART_PLANNER
inline uint8_t planOrderSmart(const Order& order, Cmd* steps, uint8_t maxSteps) {
    return planOrderSmart(orderEffectiveShelf(order), orderEffectiveStartSlot(order), steps, maxSteps);
}
#endif

}  // namespace revov
