#pragma once

#include <stdint.h>
#include "../../shared/RevoVProtocol.h"

namespace revov {

constexpr uint8_t PIN_UNUSED = 255;

// Pins
constexpr uint8_t PIN_X_STEP = 6; // TODO: calibrate
constexpr uint8_t PIN_X_DIR = 7; // TODO: calibrate
constexpr uint8_t PIN_X_ENABLE = 255; // TODO: calibrate
constexpr uint8_t PIN_Y1_STEP = 8; // TODO: calibrate
constexpr uint8_t PIN_Y1_DIR = 9; // TODO: calibrate
constexpr uint8_t PIN_Y2_STEP = 26; // TODO: calibrate
constexpr uint8_t PIN_Y2_DIR = 27; // TODO: calibrate
constexpr uint8_t PIN_Y_ENABLE = 255; // TODO: calibrate
constexpr uint8_t PIN_GRIPPER_SERVO = 10; // TODO: calibrate
constexpr uint8_t PIN_Z_SERVO = 11; // TODO: calibrate
constexpr uint8_t PIN_DOOR_SERVO = 12; // TODO: calibrate
constexpr uint8_t PIN_CONVEYOR1_STEP = 13; // TODO: calibrate
constexpr uint8_t PIN_CONVEYOR1_DIR = 14; // TODO: calibrate
constexpr uint8_t PIN_CONVEYOR2_STEP = 15; // TODO: calibrate
constexpr uint8_t PIN_CONVEYOR2_DIR = 16; // TODO: calibrate
constexpr uint8_t PIN_CONVEYOR3_STEP = 17; // TODO: calibrate
constexpr uint8_t PIN_CONVEYOR3_DIR = 18; // TODO: calibrate
constexpr uint8_t PIN_CONVEYOR4_STEP = 19; // TODO: calibrate
constexpr uint8_t PIN_CONVEYOR4_DIR = 20; // TODO: calibrate
constexpr uint8_t PIN_CONVEYOR5_STEP = 21; // TODO: calibrate
constexpr uint8_t PIN_CONVEYOR5_DIR = 22; // TODO: calibrate
constexpr uint8_t PIN_CONVEYOR_ENABLE = 255; // TODO: calibrate
// Limit and safety switches. Assign real pins only after the pin map is locked.
constexpr uint8_t X_HOME = PIN_UNUSED; // TODO: calibrate
constexpr uint8_t Y_TOP_LIMIT = PIN_UNUSED; // TODO: calibrate
constexpr uint8_t Y_BOTTOM_LIMIT = PIN_UNUSED; // TODO: calibrate
constexpr uint8_t Y_HOME = PIN_UNUSED; // TODO: calibrate
constexpr uint8_t Z_EXT_LIMIT = PIN_UNUSED; // TODO: calibrate
constexpr uint8_t Z_RET_LIMIT = PIN_UNUSED; // TODO: calibrate
constexpr uint8_t Z_HOME = PIN_UNUSED; // TODO: calibrate
constexpr uint8_t SHELF1_L_LIMIT = PIN_UNUSED; // TODO: calibrate
constexpr uint8_t SHELF1_R_LIMIT = PIN_UNUSED; // TODO: calibrate
constexpr uint8_t SHELF2_L_LIMIT = PIN_UNUSED; // TODO: calibrate
constexpr uint8_t SHELF2_R_LIMIT = PIN_UNUSED; // TODO: calibrate
constexpr uint8_t SHELF3_L_LIMIT = PIN_UNUSED; // TODO: calibrate
constexpr uint8_t SHELF3_R_LIMIT = PIN_UNUSED; // TODO: calibrate
constexpr uint8_t SHELF4_L_LIMIT = PIN_UNUSED; // TODO: calibrate
constexpr uint8_t SHELF4_R_LIMIT = PIN_UNUSED; // TODO: calibrate
constexpr uint8_t SHELF5_L_LIMIT = PIN_UNUSED; // TODO: calibrate
constexpr uint8_t SHELF5_R_LIMIT = PIN_UNUSED; // TODO: calibrate
constexpr uint8_t DISPENSE_SWITCH = PIN_UNUSED; // TODO: calibrate

constexpr uint8_t ENDSTOP_X = X_HOME; // TODO: calibrate
constexpr uint8_t ENDSTOP_Y = Y_HOME; // TODO: calibrate

// Stepper tuning: speed (Hz), acceleration (Hz/s), current (mA), distance.
constexpr uint32_t X_SPEED_MAX_HZ = 800; // TODO: calibrate
constexpr uint32_t X_ACCELERATION = 1600; // TODO: calibrate
constexpr uint16_t X_CURRENT_MA = 900; // TODO: calibrate
constexpr int32_t X_DISTANCE_STEPS_PER_SLOT = 200; // TODO: calibrate
constexpr int32_t X_STEPS_PER_SLOT = X_DISTANCE_STEPS_PER_SLOT; // TODO: calibrate
constexpr uint32_t Y_SPEED_MAX_HZ = 800; // TODO: calibrate
constexpr uint32_t Y_ACCELERATION = 1600; // TODO: calibrate
constexpr uint16_t Y_CURRENT_MA = 900; // TODO: calibrate
constexpr int32_t Y_DISTANCE_STEPS_PER_SHELF = 300; // TODO: calibrate
constexpr int32_t Y_STEPS_PER_SHELF = Y_DISTANCE_STEPS_PER_SHELF; // TODO: calibrate
constexpr uint32_t CONVEYOR_SPEED_MAX_HZ = 800; // TODO: calibrate
constexpr uint32_t CONVEYOR_ACCELERATION = 1600; // TODO: calibrate
constexpr uint16_t CONVEYOR_CURRENT_MA = 900; // TODO: calibrate
constexpr int32_t CONVEYOR_DISTANCE_STEPS_PER_COLUMN = 400; // TODO: calibrate
constexpr int32_t CONVEYOR_STEPS_PER_COLUMN = CONVEYOR_DISTANCE_STEPS_PER_COLUMN; // TODO: calibrate

// Servo tuning: speed (Hz), acceleration, current (mA via driver Vref where fitted), distance.
constexpr uint16_t Z_SPEED_MAX_HZ = 90; // TODO: calibrate
constexpr uint16_t Z_ACCELERATION = 180; // TODO: calibrate
constexpr uint16_t Z_CURRENT_MA = 0; // TODO: calibrate
constexpr uint16_t Z_DISTANCE_DEG = 90; // TODO: calibrate
constexpr uint16_t GRIP_SPEED_MAX_HZ = 90; // TODO: calibrate
constexpr uint16_t GRIP_ACCELERATION = 180; // TODO: calibrate
constexpr uint16_t GRIP_CURRENT_MA = 0; // TODO: calibrate
constexpr uint16_t GRIP_DISTANCE_DEG = 90; // TODO: calibrate
constexpr uint16_t DOOR_SPEED_MAX_HZ = 90; // TODO: calibrate
constexpr uint16_t DOOR_ACCELERATION = 180; // TODO: calibrate
constexpr uint16_t DOOR_CURRENT_MA = 0; // TODO: calibrate
constexpr uint16_t DOOR_DISTANCE_DEG = 90; // TODO: calibrate
constexpr uint16_t DOOR_CLOSED_DEG = 0; // TODO: calibrate
constexpr uint16_t DOOR_OPEN_DEG = DOOR_DISTANCE_DEG; // TODO: calibrate

constexpr uint16_t Z_UP_DEG = Z_DISTANCE_DEG; // TODO: calibrate
constexpr uint16_t Z_DOWN_DEG = 0; // TODO: calibrate
constexpr uint16_t GRIP_OPEN_DEG = 0; // TODO: calibrate
constexpr uint16_t GRIP_CLOSE_DEG = GRIP_DISTANCE_DEG; // TODO: calibrate
constexpr uint16_t GRIP_HOLD_MS = 200; // TODO: calibrate
constexpr uint16_t READY_PULSE_MS = 100; // TODO: calibrate
constexpr uint16_t CMD_MAX_LENGTH = 96; // TODO: calibrate

}  // namespace revov
