#pragma once

#include <stddef.h>
#include <stdint.h>

namespace revov {

// Receives one complete JSON message per event. The web
// module registers its WebSocket broadcast here so the
// bus never knows who is listening.
typedef void (*EventSink)(const char* json);

// Resets the emitted-state cache so the next state
// event is always pushed. Called once at boot, before
// any sink is registered.
void eb_init();

// Registers the fan-out sink. A single sink is retained;
// the last registration wins.
void eb_registerSink(EventSink sink);

// Line-carrying events. Each formats its line exactly
// once, prints the line to the serial monitor, and hands
// the JSON message to the registered sink.
void eb_orderStart(uint8_t shelfId, uint8_t quantity, uint8_t startSlot);
void eb_drinkStart(uint8_t drink, uint8_t of, uint8_t slot);
void eb_step(uint8_t step, uint8_t of, uint8_t dir, uint8_t data);

// Smart-planner skip trace: one line per filtered-out
// no-op move, logged in plan order between the
// dispatched steps. Carries the plan position and
// the effective count, like eb_step.
void eb_stepSkip(uint8_t step, uint8_t of, char axis,
                   int8_t data);
void eb_ready(uint32_t deltaMs);
void eb_nextSlot(uint8_t shelf, uint8_t value);
void eb_conveyorCheck(uint8_t value, uint8_t threshold, bool advance);
void eb_drinkEnd();
void eb_orderDone(uint8_t shelfId, uint8_t dispensed,
                  uint32_t conveyorAdvances, uint8_t finalNextSlot);
void eb_warn(uint8_t step, uint8_t of, uint8_t retry, uint8_t limit);
void eb_error(uint8_t retries, bool halted);

// Appends an arbitrary line to the system log. The sim
// page's log box renders it like any trace line, so ESP
// instructions and signals share one terminal.
void eb_log(const char* line);

// JSON-only events: no line, no serial output; the sink
// receives the message alone.
void eb_drinkOk(const char* orderId, uint8_t drink);
void eb_state();

// Forces the current state snapshot out through the
// registered sink, bypassing the change-only cache.
// The sim page calls this when a client connects so a
// fresh console renders the current machine at once.
void eb_pushState();

// Renders the human label for a primitive command. The
// trajectory preview uses the same formatter so the
// preview and the execution trace never drift apart.
void eb_stepLabel(uint8_t dir, uint8_t data, char* out, size_t outSize);

}  // namespace revov
