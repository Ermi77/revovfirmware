#include <Arduino.h>
#include <unity.h>

#include "Config.h"
#include "Core/StateMachine.h"
#include "Order/Inventory.h"
#include "Order/OrderQueue.h"
#include "Trajectory/Planner.h"

using namespace revov;

void setUp() {}
void tearDown() {}

// The queue is a FIFO ring: peek returns the oldest
// entry, markDispensed pops it, and size tracks both.
void testQueueFifo() {
    oq_clear();
    TEST_ASSERT_EQUAL_UINT8(0, oq_size());
    TEST_ASSERT_FALSE(oq_hasNext());
    oq_enqueue(1, 2, 3);
    oq_enqueue(4, 5, 6);
    TEST_ASSERT_EQUAL_UINT8(2, oq_size());
    TEST_ASSERT_TRUE(oq_hasNext());
    const Order first = oq_peek();
    TEST_ASSERT_EQUAL_UINT8(1, first.shelfId);
    TEST_ASSERT_EQUAL_UINT8(2, first.quantity);
    TEST_ASSERT_EQUAL_UINT8(3, first.startSlot);
    oq_markDispensed();
    const Order second = oq_peek();
    TEST_ASSERT_EQUAL_UINT8(4, second.shelfId);
    TEST_ASSERT_EQUAL_UINT8(5, second.quantity);
    TEST_ASSERT_EQUAL_UINT8(6, second.startSlot);
    oq_markDispensed();
    TEST_ASSERT_EQUAL_UINT8(0, oq_size());
    TEST_ASSERT_FALSE(oq_hasNext());
}

// The queue holds at most ORDER_QUEUE_CAPACITY
// orders; extra enqueues are dropped, not wrapped.
void testQueueCapacity() {
    oq_clear();
    for (uint8_t i = 0; i < ORDER_QUEUE_CAPACITY + 4; ++i) {
        oq_enqueue(1, 1, SLOT_FIRST);
    }
    TEST_ASSERT_EQUAL_UINT8(ORDER_QUEUE_CAPACITY, oq_size());
}

// Inventory tracks the next slot per shelf; only the
// addressed shelf moves, and reset returns it to the
// first slot. An out-of-range shelf reports 0, which
// is never a valid pickup slot.
void testInventoryPerShelf() {
    inv_resetAll();
    for (uint8_t shelf = 1; shelf <= SHELF_COUNT; ++shelf) {
        TEST_ASSERT_EQUAL_UINT8(SLOT_FIRST, inv_nextSlot(shelf));
    }
    inv_setNextSlot(2, 7);
    TEST_ASSERT_EQUAL_UINT8(7, inv_nextSlot(2));
    TEST_ASSERT_EQUAL_UINT8(SLOT_FIRST, inv_nextSlot(1));
    TEST_ASSERT_EQUAL_UINT8(SLOT_FIRST, inv_nextSlot(3));
    inv_resetSlots(2);
    TEST_ASSERT_EQUAL_UINT8(SLOT_FIRST, inv_nextSlot(2));
    TEST_ASSERT_EQUAL_UINT8(0, inv_nextSlot(0));
    TEST_ASSERT_EQUAL_UINT8(0, inv_nextSlot(99));
}

// The conveyor rule advances the row once the counter
// passes SLOTS_PER_SHELF; the last slot itself does
// not advance it.
void testInventoryConveyorRule() {
    inv_resetAll();
    inv_setNextSlot(1, static_cast<uint8_t>(SLOTS_PER_SHELF - 1));
    TEST_ASSERT_FALSE(inv_shouldAdvance(1));
    inv_setNextSlot(1, SLOTS_PER_SHELF);
    TEST_ASSERT_FALSE(inv_shouldAdvance(1));
    inv_setNextSlot(1, static_cast<uint8_t>(SLOTS_PER_SHELF + 1));
    TEST_ASSERT_TRUE(inv_shouldAdvance(1));
}

// The planner carries the shelf index directly on the
// Y axis (1..SHELF_COUNT) and reserves slot 0 for the
// dispensing area on the third shelf.
void testPlannerSteps() {
    Order order{};
    order.shelfId = 2;
    order.quantity = 1;
    order.startSlot = 4;
    Cmd steps[8]{};
    TEST_ASSERT_EQUAL_UINT8(8, planOrder(order, steps, 8));
    TEST_ASSERT_EQUAL_CHAR('X', steps[0].axis);
    TEST_ASSERT_EQUAL_INT8(4, steps[0].data);
    TEST_ASSERT_EQUAL_CHAR('Y', steps[1].axis);
    TEST_ASSERT_EQUAL_INT8(2, steps[1].data);
    TEST_ASSERT_EQUAL_CHAR('Y', steps[2].axis);
    TEST_ASSERT_EQUAL_INT8(ACTION_PICK, steps[2].data);
    TEST_ASSERT_EQUAL_CHAR('Y', steps[3].axis);
    TEST_ASSERT_EQUAL_INT8(DISPENSE_SHELF, steps[3].data);
    TEST_ASSERT_EQUAL_CHAR('X', steps[4].axis);
    TEST_ASSERT_EQUAL_INT8(DISPENSE_SLOT, steps[4].data);
    TEST_ASSERT_EQUAL_CHAR('Y', steps[5].axis);
    TEST_ASSERT_EQUAL_INT8(ACTION_DOOR_OPEN, steps[5].data);
    TEST_ASSERT_EQUAL_CHAR('Y', steps[6].axis);
    TEST_ASSERT_EQUAL_INT8(ACTION_PLACE, steps[6].data);
    TEST_ASSERT_EQUAL_CHAR('Y', steps[7].axis);
    TEST_ASSERT_EQUAL_INT8(ACTION_DOOR_CLOSE, steps[7].data);
    // Slot 0 is the dispensing area: never a valid
    // pickup location.
    order.startSlot = DISPENSE_SLOT;
    TEST_ASSERT_EQUAL_UINT8(0, planOrder(order, steps, 8));
    order.startSlot = static_cast<uint8_t>(SLOT_LAST + 1);
    TEST_ASSERT_EQUAL_UINT8(0, planOrder(order, steps, 8));
}

// The intake funnel range-checks before touching any
// state, so invalid triplets return errors and leave
// the queue empty; a valid triplet enqueues.
void testOrderValidation() {
    oq_clear();
    TEST_ASSERT_NOT_NULL(sm_submitOrder(0, 1, SLOT_FIRST, "t"));
    TEST_ASSERT_NOT_NULL(sm_submitOrder(SHELF_COUNT + 1, 1, SLOT_FIRST, "t"));
    TEST_ASSERT_NOT_NULL(sm_submitOrder(1, 0, SLOT_FIRST, "t"));
    TEST_ASSERT_NOT_NULL(sm_submitOrder(1, 1, DISPENSE_SLOT, "t"));
    TEST_ASSERT_NOT_NULL(sm_submitOrder(1, 1, SLOT_LAST + 1, "t"));
    TEST_ASSERT_EQUAL_UINT8(0, oq_size());
    TEST_ASSERT_NULL(sm_submitOrder(1, 1, SLOT_FIRST, "t"));
    TEST_ASSERT_NULL(sm_submitOrder(1, 1, SLOT_LAST, "t"));
    TEST_ASSERT_EQUAL_UINT8(2, oq_size());
    oq_clear();
}

#if SMART_PLANNER

// Runs one drink through the planner tracking the
// way the state machine does: every dispatched
// entry settles through pl_noteSettled, skipped
// entries do not (there is no READY pulse for
// them).
void runDrink(const Order& order) {
    Cmd steps[8]{};
    planOrderSmart(order, steps, PLAN_SIZE);
    for (uint8_t i = 0; i < PLAN_SIZE; ++i) {
        if (!steps[i].skip) {
            const uint8_t dir =
                steps[i].axis == 'X' ? DIR_X : DIR_Y;
            pl_noteSettled(dir,
                             static_cast<uint8_t>(steps[i].data));
        }
    }
}

// Scenario 1 — fresh boot, no skips: both axes
// are unknown, and the drop shelf (3) differs
// from the pickup shelf, so all eight commands
// are sent.
void testScenario1FreshBootNoSkips() {
    pl_resetTracking();
    Order order{};
    order.shelfId = 1;
    order.quantity = 1;
    order.startSlot = 4;
    Cmd steps[8]{};
    TEST_ASSERT_EQUAL_UINT8(8, planOrderSmart(order, steps, 8));
    for (uint8_t i = 0; i < PLAN_SIZE; ++i) {
        TEST_ASSERT_FALSE(steps[i].skip);
    }
}

// Scenario 2 — same-shelf pick and drop (the
// bug report): shelf 3 is the drop shelf, so
// the move back to the drop height (STEP 4) is
// a no-op right after the pick and is skipped.
void testScenario2SameShelfPickDrop() {
    pl_resetTracking();
    Order order{};
    order.shelfId = 3;
    order.quantity = 1;
    order.startSlot = 1;
    Cmd steps[8]{};
    TEST_ASSERT_EQUAL_UINT8(7, planOrderSmart(order, steps, 8));
    TEST_ASSERT_FALSE(steps[0].skip);  // X to slot 1
    TEST_ASSERT_FALSE(steps[1].skip);  // Y to shelf 3
    TEST_ASSERT_FALSE(steps[2].skip);  // PICK
    TEST_ASSERT_TRUE(steps[3].skip);   // Y to shelf 3 — already there
    TEST_ASSERT_FALSE(steps[4].skip);  // X to slot 0
    TEST_ASSERT_FALSE(steps[5].skip);  // DOOR OPEN
    TEST_ASSERT_FALSE(steps[6].skip);  // PLACE
    TEST_ASSERT_FALSE(steps[7].skip);  // DOOR CLOSE
}

// Scenario 3 — conveyor rollover: the belt resets
// the X mapping to the dispensing area and leaves
// the shelf untouched, so a slot-0 target is a
// no-op (STEP 1 would be skipped if the belt
// parked a column at the dispensing position) and
// the next shelf-3 drink skips both Y moves to
// the drop shelf.
void testScenario3ConveyorRollover() {
    pl_resetTracking();
    Order order{};
    order.shelfId = 3;
    order.quantity = 1;
    order.startSlot = 1;
    runDrink(order);
    TEST_ASSERT_EQUAL_INT8(0, pl_curXSlot());
    TEST_ASSERT_EQUAL_INT8(3, pl_curYShelf());
    pl_noteConveyorSettled();
    TEST_ASSERT_EQUAL_INT8(0, pl_curXSlot());
    TEST_ASSERT_EQUAL_INT8(3, pl_curYShelf());
    // The skip rule the rollover arms: a slot-0
    // target at the post-rollover position is a
    // no-op.
    TEST_ASSERT_FALSE(pl_shouldMoveX(0));
    TEST_ASSERT_FALSE(pl_shouldMoveY(3));
    TEST_ASSERT_TRUE(pl_shouldMoveX(1));
    Cmd steps[8]{};
    TEST_ASSERT_EQUAL_UINT8(6, planOrderSmart(order, steps, 8));
    TEST_ASSERT_FALSE(steps[0].skip);  // X 0 -> 1
    TEST_ASSERT_TRUE(steps[1].skip);   // Y already at shelf 3
    TEST_ASSERT_FALSE(steps[2].skip);  // PICK
    TEST_ASSERT_TRUE(steps[3].skip);   // Y already at shelf 3
    TEST_ASSERT_FALSE(steps[4].skip);  // X 1 -> 0
    TEST_ASSERT_FALSE(steps[5].skip);
    TEST_ASSERT_FALSE(steps[6].skip);
    TEST_ASSERT_FALSE(steps[7].skip);
}

// Scenario 4 — consecutive drinks on the same
// shelf: the drop position (X 0, Y 3) carries
// into the next drink, so both Y moves to the
// drop shelf are skipped.
void testScenario4ConsecutiveDrinksSameShelf() {
    pl_resetTracking();
    Order order{};
    order.shelfId = 3;
    order.quantity = 2;
    order.startSlot = 1;
    runDrink(order);  // drink 1 from slot 1
    order.startSlot = 2;
    Cmd steps[8]{};
    TEST_ASSERT_EQUAL_UINT8(6, planOrderSmart(order, steps, 8));
    TEST_ASSERT_FALSE(steps[0].skip);  // X 0 -> 2
    TEST_ASSERT_TRUE(steps[1].skip);   // Y still at shelf 3
    TEST_ASSERT_FALSE(steps[2].skip);  // PICK
    TEST_ASSERT_TRUE(steps[3].skip);   // Y still at shelf 3
    TEST_ASSERT_FALSE(steps[4].skip);  // X 2 -> 0
    TEST_ASSERT_FALSE(steps[5].skip);
    TEST_ASSERT_FALSE(steps[6].skip);
    TEST_ASSERT_FALSE(steps[7].skip);
}

// Scenario 5 — unknown position: a known axis may
// skip, an unknown axis (-1) never equals a
// target, so its move is always sent.
void testScenario5UnknownPositionNoSkip() {
    // X is known (a settled move), Y is unknown
    // (an E-stop invalidated it).
    pl_resetTracking();
    pl_noteSettled(DIR_X, 4);
    TEST_ASSERT_EQUAL_INT8(4, pl_curXSlot());
    TEST_ASSERT_EQUAL_INT8(-1, pl_curYShelf());
    TEST_ASSERT_FALSE(pl_shouldMoveX(4));  // known + equal → skip
    TEST_ASSERT_TRUE(pl_shouldMoveX(5));   // known + different → move
    TEST_ASSERT_TRUE(pl_shouldMoveY(3));   // unknown → must move
    Order order{};
    order.shelfId = 3;
    order.quantity = 1;
    order.startSlot = 4;
    Cmd steps[8]{};
    TEST_ASSERT_EQUAL_UINT8(6, planOrderSmart(order, steps, 8));
    TEST_ASSERT_TRUE(steps[0].skip);   // X already at slot 4
    TEST_ASSERT_FALSE(steps[1].skip);  // unknown Y must move
    TEST_ASSERT_FALSE(steps[2].skip);  // PICK
    TEST_ASSERT_TRUE(steps[3].skip);   // Y settled at shelf 3
    TEST_ASSERT_FALSE(steps[4].skip);  // X 4 -> 0
    TEST_ASSERT_FALSE(steps[5].skip);
    TEST_ASSERT_FALSE(steps[6].skip);
    TEST_ASSERT_FALSE(steps[7].skip);
}

// Scenario 6 — two drops in a row at slot 0: the
// drop leaves X at the dispensing area, so a
// slot-0 target is a no-op the first and the
// second time.
void testScenario6TwoDropsAtSlot0() {
    pl_resetTracking();
    pl_noteSettled(DIR_X, DISPENSE_SLOT);  // first drop
    TEST_ASSERT_EQUAL_INT8(0, pl_curXSlot());
    TEST_ASSERT_FALSE(pl_shouldMoveX(0));
    pl_noteSettled(DIR_X, 5);                // next pickup
    pl_noteSettled(DIR_X, DISPENSE_SLOT);  // second drop
    TEST_ASSERT_EQUAL_INT8(0, pl_curXSlot());
    TEST_ASSERT_FALSE(pl_shouldMoveX(0));
    // A valid drink still sends STEP 5: the
    // pickup (slot 1..10) moves X away from the
    // drop slot before the drop.
    Order order{};
    order.shelfId = 3;
    order.quantity = 1;
    order.startSlot = 1;
    Cmd steps[8]{};
    TEST_ASSERT_EQUAL_UINT8(7, planOrderSmart(order, steps, 8));
    TEST_ASSERT_FALSE(steps[4].skip);
}

#endif  // SMART_PLANNER

void setup() {
    Serial.begin(115200);
    UNITY_BEGIN();
    RUN_TEST(testQueueFifo);
    RUN_TEST(testQueueCapacity);
    RUN_TEST(testInventoryPerShelf);
    RUN_TEST(testInventoryConveyorRule);
    RUN_TEST(testPlannerSteps);
    RUN_TEST(testOrderValidation);
#if SMART_PLANNER
    RUN_TEST(testScenario1FreshBootNoSkips);
    RUN_TEST(testScenario2SameShelfPickDrop);
    RUN_TEST(testScenario3ConveyorRollover);
    RUN_TEST(testScenario4ConsecutiveDrinksSameShelf);
    RUN_TEST(testScenario5UnknownPositionNoSkip);
    RUN_TEST(testScenario6TwoDropsAtSlot0);
#endif
    UNITY_END();
}

void loop() {}
