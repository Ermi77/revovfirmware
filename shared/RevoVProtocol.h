#pragma once

#include <stdint.h>

namespace revov {

// TODO: provision per-machine credentials outside the repository for deployment.
constexpr const char* MACHINE_ID = "revov-dev";
constexpr const char* MACHINE_TOKEN = "REPLACE_ME";

constexpr uint8_t SHELF_COUNT = 5;
constexpr uint8_t SLOTS_PER_SHELF = 10;
constexpr uint8_t COLUMNS_PER_SHELF = 10;

// Index space. Shelves are numbered 1..SHELF_COUNT with no
// index 0. Slots are numbered 1..SLOTS_PER_SHELF; index 0 is
// the dedicated dispensing area at a corner of the third
// shelf, reserved strictly for dispensing and never a valid
// location to pick up drinks.
constexpr uint8_t SHELF_FIRST = 1;
constexpr uint8_t SHELF_LAST = SHELF_COUNT;
constexpr uint8_t SLOT_FIRST = 1;
constexpr uint8_t SLOT_LAST = SLOTS_PER_SHELF;
constexpr uint8_t DISPENSE_SHELF = 3;
constexpr uint8_t DISPENSE_SLOT = 0;

constexpr uint8_t DIR_X = 0;
constexpr uint8_t DIR_Y = 1;

constexpr uint8_t ACTION_PICK = 6;
constexpr uint8_t ACTION_PLACE = 7;
constexpr uint8_t ACTION_DOOR_OPEN = 8;
constexpr uint8_t ACTION_DOOR_CLOSE = 9;
constexpr uint8_t ACTION_CONVEYOR_GENERAL = 5;
constexpr uint8_t ACTION_CONVEYOR_FIRST = 10;
constexpr uint8_t ACTION_CONVEYOR_LAST = 14;

constexpr uint8_t conveyorCodeForShelf(uint8_t shelfId) {
    return static_cast<uint8_t>(ACTION_CONVEYOR_FIRST + shelfId - 1);
}

}  // namespace revov
