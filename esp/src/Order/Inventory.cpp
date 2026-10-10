#include "Inventory.h"

#include "../../shared/RevoVProtocol.h"

namespace revov {

namespace {
uint8_t nextSlots[SHELF_COUNT]{};
// The row serves slots 1..SLOTS_PER_SHELF, so every
// shelf starts at the first slot.
struct Initializer {
    Initializer() {
        for (uint8_t i = 0; i < SHELF_COUNT; ++i) {
            nextSlots[i] = SLOT_FIRST;
        }
    }
} initializer;
}

uint8_t inv_nextSlot(uint8_t shelfId) {
    if (shelfId < SHELF_FIRST || shelfId > SHELF_LAST) return 0;
    return nextSlots[shelfId - SHELF_FIRST];
}

bool inv_shouldAdvance(uint8_t shelfId) {
    // CONVEYOR_RULE: "greater than" comparison,
    // owner-confirmed. The row serves slots 1..10, so
    // the counter passes SLOTS_PER_SHELF (10) once the
    // last slot of the shelf has been dispensed.
    return inv_nextSlot(shelfId) > SLOTS_PER_SHELF;
}

void inv_resetSlots(uint8_t shelfId) {
    if (shelfId >= SHELF_FIRST && shelfId <= SHELF_LAST) {
        nextSlots[shelfId - SHELF_FIRST] = SLOT_FIRST;
    }
}

void inv_setNextSlot(uint8_t shelfId, uint8_t slot) {
    // The counter passes SLOTS_PER_SHELF (10) once the
    // last slot is dispensed; the conveyor rule then
    // wraps the row back to the first slot, so the
    // transient value SLOTS_PER_SHELF + 1 is stored.
    if (shelfId >= SHELF_FIRST && shelfId <= SHELF_LAST &&
        slot >= SLOT_FIRST && slot <= SLOTS_PER_SHELF + 1) {
        nextSlots[shelfId - SHELF_FIRST] = slot;
    }
}

void inv_resetAll() {
    for (uint8_t i = 0; i < SHELF_COUNT; ++i) nextSlots[i] = SLOT_FIRST;
}

}  // namespace revov
