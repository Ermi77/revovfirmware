#pragma once

#include <stdint.h>

namespace revov {

// LED behavior selected from the serial console. The output is
// strictly binary: ON holds the LED lit; OFF darkens it. An
// error latch overrides the mode with red until the next reset.
enum class LedMode : uint8_t { ON, OFF };

// Event notes from the state machine. st_noteStep reports a
// primitive that has settled, so the tracker mirrors the machine
// rather than the command stream: a primitive that times out and
// is retried is never noted.
void st_noteReset();
void st_noteError();
void st_noteStep(uint8_t dir, uint8_t data);

// Selects the LED behavior. Mirrored by the "led on | off"
// console command.
void st_setLedMode(LedMode mode);

// Re-applies the LED to the board pin.
void st_updateLed();

// Tracked physical state. X is the slot position (0..10,
// where 0 is the dispensing area), Y is the shelf
// position (1..5). Z and door are reported by name;
// PICK and PLACE both settle with Z retracted, matching the Mega
// gripper sequence.
uint8_t st_x();
uint8_t st_y();
const char* st_zName();
const char* st_doorName();

// Last Z-axis action name: "PICK" or "PLACE", or an empty
// string before the first Z action settles. The sim page
// colors the Z chip from this name: PICK amber, PLACE blue.
const char* st_zActionName();
const char* st_ledColorName();
const char* st_ledModeName();

}  // namespace revov
