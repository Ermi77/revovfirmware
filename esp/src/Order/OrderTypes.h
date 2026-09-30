#pragma once

#include <stdint.h>

namespace revov {

struct OrderItem {
    uint8_t shelfId;
    uint8_t quantity;
    uint8_t startSlot;
};

struct Order {
    uint8_t shelfId;
    uint8_t quantity;
    uint8_t startSlot;
    char orderId[32];
};

}  // namespace revov
