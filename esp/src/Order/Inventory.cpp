#include "Inventory.h"

#include "../../shared/RevoVProtocol.h"

namespace revov {

namespace {
uint8_t nextSlots[SHELF_COUNT]{};
}

uint8_t inv_nextSlot(uint8_t shelfId) {
    if (shelfId < 1 || shelfId > SHELF_COUNT) return 0;
    return nextSlots[shelfId - 1];
}

bool inv_shouldAdvance(uint8_t shelfId) {
    return inv_nextSlot(shelfId) > SLOTS_PER_SHELF - 1;
}

void inv_resetSlots(uint8_t shelfId) {
    if (shelfId >= 1 && shelfId <= SHELF_COUNT) nextSlots[shelfId - 1] = 0;
}

void inv_setNextSlot(uint8_t shelfId, uint8_t slot) {
    if (shelfId >= 1 && shelfId <= SHELF_COUNT && slot <= SLOTS_PER_SHELF) {
        nextSlots[shelfId - 1] = slot;
    }
}

void inv_resetAll() {
    for (uint8_t i = 0; i < SHELF_COUNT; ++i) nextSlots[i] = 0;
}

}  // namespace revov
