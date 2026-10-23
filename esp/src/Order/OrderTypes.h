#pragma once

#include <stdint.h>
#include <cstring>

namespace revov {

// Maximum number of drink items per order
constexpr uint8_t MAX_ORDER_ITEMS = 5;

// Single drink selection within an order
struct OrderItem {
    uint8_t shelfId;      // 1..SHELF_COUNT
    uint8_t quantity;     // 1..255
    uint8_t startSlot;    // 1..SLOT_LAST
};

// Extended order with multiple drink items
struct Order {
    // Backward-compatible single-item fields (for legacy orders)
    uint8_t shelfId;
    uint8_t quantity;
    uint8_t startSlot;
    char orderId[32];

    // Multi-item support
    OrderItem items[MAX_ORDER_ITEMS];
    uint8_t itemCount = 0;
    bool isMultiItem = false;  // true = use items[], false = use legacy fields

    // Current processing state (runtime only, not persisted)
    uint8_t currentItemIndex = 0;
    uint8_t currentItemDrinkIndex = 0;
};

// Check if order uses multi-item format
inline bool orderIsMultiItem(const Order& order) {
    return order.isMultiItem && order.itemCount > 0;
}

// Get total drink count across all items
inline uint16_t orderTotalDrinks(const Order& order) {
    if (orderIsMultiItem(order)) {
        uint16_t total = 0;
        for (uint8_t i = 0; i < order.itemCount; ++i) {
            total += order.items[i].quantity;
        }
        return total;
    }
    return order.quantity;
}

// Get current item being processed (for multi-item orders)
inline const OrderItem* orderCurrentItem(const Order& order) {
    if (orderIsMultiItem(order) && order.currentItemIndex < order.itemCount) {
        return &order.items[order.currentItemIndex];
    }
    return nullptr;
}

// Legacy compatibility: get effective shelf/quantity/startSlot for current item
inline uint8_t orderEffectiveShelf(const Order& order) {
    if (const OrderItem* item = orderCurrentItem(order)) return item->shelfId;
    return order.shelfId;
}

inline uint8_t orderEffectiveQuantity(const Order& order) {
    if (const OrderItem* item = orderCurrentItem(order)) return item->quantity;
    return order.quantity;
}

inline uint8_t orderEffectiveStartSlot(const Order& order) {
    if (const OrderItem* item = orderCurrentItem(order)) return item->startSlot;
    return order.startSlot;
}

// Initialize order from legacy single-item format
inline void orderFromLegacy(Order& order, uint8_t shelfId, uint8_t quantity,
                            uint8_t startSlot, const char* orderId = "") {
    order = {};
    order.shelfId = shelfId;
    order.quantity = quantity;
    order.startSlot = startSlot;
    order.isMultiItem = false;
    if (orderId && *orderId) {
        strncpy(order.orderId, orderId, sizeof(order.orderId) - 1);
        order.orderId[sizeof(order.orderId) - 1] = '\0';
    }
}

// Initialize order from multi-item format
inline void orderFromItems(Order& order, const OrderItem* items, uint8_t count,
                           const char* orderId = "") {
    order = {};
    order.isMultiItem = true;
    order.itemCount = (count > MAX_ORDER_ITEMS) ? MAX_ORDER_ITEMS : count;
    for (uint8_t i = 0; i < order.itemCount; ++i) {
        order.items[i] = items[i];
    }
    if (orderId && *orderId) {
        strncpy(order.orderId, orderId, sizeof(order.orderId) - 1);
        order.orderId[sizeof(order.orderId) - 1] = '\0';
    }
    // For backward compat: populate legacy fields from first item
    if (order.itemCount > 0) {
        order.shelfId = order.items[0].shelfId;
        order.quantity = order.items[0].quantity;
        order.startSlot = order.items[0].startSlot;
    }
}

// Reset runtime state for new order processing
inline void orderResetRuntime(Order& order) {
    order.currentItemIndex = 0;
    order.currentItemDrinkIndex = 0;
}

// Advance to next drink within current item, or next item
inline bool orderAdvanceDrink(Order& order) {
    if (orderIsMultiItem(order)) {
        const OrderItem* item = orderCurrentItem(order);
        if (item) {
            order.currentItemDrinkIndex++;
            if (order.currentItemDrinkIndex >= item->quantity) {
                order.currentItemDrinkIndex = 0;
                order.currentItemIndex++;
            }
            return order.currentItemIndex < order.itemCount;
        }
        return false;
    } else {
        // Legacy: just count drinks
        return false; // StateMachine handles this differently
    }
}

// Check if multi-item order is complete
inline bool orderIsComplete(const Order& order) {
    if (orderIsMultiItem(order)) {
        return order.currentItemIndex >= order.itemCount;
    }
    return false;
}

}  // namespace revov