#pragma once

#include <stdint.h>
#include "../Order/OrderTypes.h"

namespace revov {

enum class State : uint8_t { IDLE, DISPATCHING, WAITING_FOR_READY, ERROR };

void sm_init();
void sm_update();
State sm_state();
void sm_handleCommand(char* command);
void startOrder();
bool isReturning();
void sm_noteProgress();
bool sm_hasActiveOrder();
Order sm_activeOrder();
uint8_t sm_drinkIndex();
uint8_t sm_stepIndex();
uint32_t sm_conveyorAdvances();
const char* sm_stateName();
const char* sm_stateName(State state);

// Single order-intake funnel shared by every door (serial console,
 // sim web page, and WSS backend). Validates the triplet, then places
 // it in the order queue. Returns nullptr on success, or a static
 // error string:
 //   "machine halted; send reset"  (state == ERROR)
 //   "shelfId must be 1..5"
 //   "quantity must be 1..255"
 //   "startSlot must be 1..10"
 //   "order queue full"
 const char* sm_submitOrder(uint8_t shelfId, uint8_t quantity,
                            uint8_t startSlot, const char* orderId);

 // Submit a pre-built Order (with optional multi-items). Validates
 // and enqueues. Returns nullptr on success, or a static error string.
 const char* sm_submitOrder(const Order& order);

// Clears the queue, inventory counters, and active order, and returns
// the machine to IDLE. Shared by the serial RESET command and the sim
// web page reset control.
void sm_reset();

// Dispatches a single manual primitive (axis move, PICK/PLACE,
// or door action) from the control panel, outside an order.
// Rides the same READY handshake as a planned step so the
// trace and the state tracker see identical events. Returns
// nullptr when the primitive was dispatched, or a static
// error string ("machine halted; send reset", "machine busy").
const char* sm_manualStep(uint8_t dir, uint8_t data);

// SIM_MODE pick routine. Expands the atomic PICK
// primitive into the manipulator's internal sequence —
// extend, pick, retract — with every phase riding the
// same READY handshake as a planned step. Returns
// nullptr when the routine started, or a static error
// string ("machine halted; send reset", "machine busy").
const char* sm_startPickRoutine();

// READY button release. Returns true when a held
// primitive was released, false when the machine was
// not holding one (the signal is still logged).
bool sm_signalReady();

}  // namespace revov
