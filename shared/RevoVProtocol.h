#pragma once

#include <stdint.h>

namespace revov {

// TODO: provision per-machine credentials outside the repository for deployment.
constexpr const char* MACHINE_ID = "revov-dev";
constexpr const char* MACHINE_TOKEN = "REPLACE_ME";

constexpr uint8_t SHELF_COUNT = 5;
constexpr uint8_t SLOTS_PER_SHELF = 10;
constexpr uint8_t COLUMNS_PER_SHELF = 10;

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
