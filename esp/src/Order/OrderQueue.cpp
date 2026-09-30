#include "OrderQueue.h"
#include "../Config.h"

namespace revov {

namespace {
Order queue[ORDER_QUEUE_CAPACITY]{};
uint8_t head = 0;
uint8_t tail = 0;
uint8_t count = 0;
}

void oq_enqueue(uint8_t shelfId, uint8_t qty, uint8_t startSlot) {
    if (count >= ORDER_QUEUE_CAPACITY) return;
    Order& order = queue[tail];
    order.shelfId = shelfId;
    order.quantity = qty;
    order.startSlot = startSlot;
    order.orderId[0] = '\0';
    tail = static_cast<uint8_t>((tail + 1) % ORDER_QUEUE_CAPACITY);
    ++count;
}

void oq_enqueueOrder(const Order& order) {
    if (count >= ORDER_QUEUE_CAPACITY) return;
    queue[tail] = order;
    tail = static_cast<uint8_t>((tail + 1) % ORDER_QUEUE_CAPACITY);
    ++count;
}

bool oq_hasNext() { return count != 0; }

Order oq_peek() {
    if (!count) {
        Order empty{};
        return empty;
    }
    return queue[head];
}

void oq_markDispensed() {
    if (!count) return;
    head = static_cast<uint8_t>((head + 1) % ORDER_QUEUE_CAPACITY);
    --count;
}

void oq_clear() {
    head = 0;
    tail = 0;
    count = 0;
}

uint8_t oq_size() { return count; }

}  // namespace revov
