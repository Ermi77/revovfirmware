#include "StateMachine.h"

#include "../Config.h"
#include "../Net/WebSocket.h"
#include "../Order/Inventory.h"
#include "../Order/OrderQueue.h"
#include "../Protocol/PinDriver.h"
#include "../Trajectory/Planner.h"
#include "EventBus.h"
#include "Scheduler.h"
#include "StateTracker.h"

#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace revov {
namespace {

// The conveyor advance is dispatched as step 9, past
// the eight drink steps, so its trace line carries no
// /8 denominator.
constexpr uint8_t CONVEYOR_STEP = 9;

State state = State::IDLE;
bool autoRun = false;
bool haveActive = false;
bool conveyorPending = false;
bool manipulatorAtHome = true;
uint8_t drinkIndex = 0;
uint8_t retryCount = 0;
uint8_t stepIndex = 0;
uint32_t readyDeadline = 0;
uint32_t lastProgressMs = 0;
uint32_t dispatchTime = 0;
uint32_t conveyorAdvances = 0;
Order activeOrder{0, 0, 0, ""};
Cmd steps[8]{};
uint8_t stepCount = 0;
bool manualPending = false;
uint8_t manualDir = 0;
uint8_t manualData = 0;
bool manualReadyPending = false;

// SIM_MODE pick routine: the atomic PICK primitive
// expanded into the manipulator's internal sequence —
// extend, pick, retract — so the operator gates each
// phase with the READY button.
uint8_t routine[3]{};
uint8_t routineCount = 0;
uint8_t routineIndex = 0;
bool routineActive = false;

const char* stateName(State value) {
    switch (value) {
    case State::DISPATCHING: return "DISPATCHING";
    case State::WAITING_FOR_READY: return "WAITING_FOR_READY";
    case State::ERROR: return "ERROR";
    default: return "IDLE";
    }
}

const char* stateName() { return stateName(state); }

bool parseNumber(const char* text, long& value) {
    if (!text || !*text) return false;
    char* end = nullptr;
    value = strtol(text, &end, 10);
    return end != text && *end == '\0';
}

bool validOrder(const Order& order, const char*& errorText) {
    if (order.shelfId < 1 || order.shelfId > SHELF_COUNT) {
        errorText = "shelfId must be 1..5";
        return false;
    }
    if (order.quantity < 1) {
        errorText = "quantity must be 1..255";
        return false;
    }
    if (order.startSlot < SLOT_FIRST || order.startSlot > SLOT_LAST) {
        errorText = "startSlot must be 1..10";
        return false;
    }
    return true;
}

bool parseTriplet(char* text, Order& order, const char*& errorText) {
    char* context = nullptr;
    char* first = strtok_r(text, " \t", &context);
    char* second = strtok_r(nullptr, " \t", &context);
    char* third = strtok_r(nullptr, " \t", &context);
    char* extra = strtok_r(nullptr, " \t", &context);
    long shelf = 0;
    long quantity = 0;
    long startSlot = 0;
    if (extra || !parseNumber(first, shelf) || !parseNumber(second, quantity) ||
        !parseNumber(third, startSlot)) {
        errorText = "each order group must be: shelfId quantity startSlot";
        return false;
    }
    if (shelf < 1 || shelf > SHELF_COUNT) {
        errorText = "shelfId must be 1..5";
        return false;
    }
    if (quantity < 1 || quantity > MAX_ORDER_QUANTITY) {
        errorText = "quantity must be 1..255";
        return false;
    }
    if (startSlot < SLOT_FIRST || startSlot > SLOT_LAST) {
        errorText = "startSlot must be 1..10";
        return false;
    }
    order = {};
    order.shelfId = static_cast<uint8_t>(shelf);
    order.quantity = static_cast<uint8_t>(quantity);
    order.startSlot = static_cast<uint8_t>(startSlot);
    return validOrder(order, errorText);
}

// Per-drink outcome funnel. Publishes to the backend
// when the WSS link is up; the event bus carries the
// outcome to the web console as a JSON-only drink_ok
// event, so nothing is printed here.
void publishDrinkOutcome(const char* orderId, uint8_t drinkIndex,
                         const char* status) {
    if (ws_isConnected()) {
        ws_publishDrinkOutcome(orderId, drinkIndex, status);
    }
}

// The one order-intake funnel. Every door (serial console, sim
// web page, WSS backend) validates and enqueues through here so
// the range checks, the halted guard, and the queue-full guard
// live in exactly one place.
const char* submitOrder(uint8_t shelfId, uint8_t quantity,
                        uint8_t startSlot, const char* orderId) {
    if (state == State::ERROR) {
        return "machine halted; send reset";
    }
    if (shelfId < 1 || shelfId > SHELF_COUNT) {
        return "shelfId must be 1..5";
    }
    if (quantity < 1 || quantity > MAX_ORDER_QUANTITY) {
        return "quantity must be 1..255";
    }
    if (startSlot < SLOT_FIRST || startSlot > SLOT_LAST) {
        return "startSlot must be 1..10";
    }
    if (oq_size() >= ORDER_QUEUE_CAPACITY) {
        return "order queue full";
    }
    Order order{};
    order.shelfId = shelfId;
    order.quantity = quantity;
    order.startSlot = startSlot;
    if (orderId && *orderId) {
        strncpy(order.orderId, orderId, sizeof(order.orderId) - 1);
        order.orderId[sizeof(order.orderId) - 1] = '\0';
    }
    oq_enqueueOrder(order);
    return nullptr;
}

// Shared reset body: clears queue and inventory, drops any active
// order, and returns to IDLE. Callers report their own outcome.
void resetMachine() {
    oq_clear();
    inv_resetAll();
    haveActive = false;
    conveyorPending = false;
    conveyorAdvances = 0;
    autoRun = false;
    manualPending = false;
    manualReadyPending = false;
    routineActive = false;
    routineCount = 0;
    routineIndex = 0;
    state = State::IDLE;
    st_noteReset();
#if SMART_PLANNER
    // Reset doubles as the ESP-side home analogue:
    // the position is unknown until a move settles.
    pl_resetTracking();
#endif
}

bool busy() { return state == State::DISPATCHING || state == State::WAITING_FOR_READY; }

void noteProgress() { lastProgressMs = millis(); }

void printHelp() {
    Serial.println(F("help"));
    Serial.println(F("order <shelfId> <quantity> <startSlot> [; shelfId quantity startSlot ...]"));
    Serial.println(F("status | run | step | reset | sim on | sim off | led on | led off"));
}

void printStatus() {
    Serial.print(F("STATUS state="));
    Serial.print(stateName());
    Serial.print(F(" sim="));
    Serial.print(pd_isSimulation() ? 1 : 0);
    Serial.print(F(" queue="));
    Serial.print(oq_size());
    Serial.print(F(" active="));
    if (haveActive) {
        Serial.print(activeOrder.shelfId);
        Serial.print(',');
        Serial.print(activeOrder.quantity);
        Serial.print(',');
        Serial.print(activeOrder.startSlot);
    } else {
        Serial.print(F("none"));
    }
    Serial.print(F(" drink="));
    Serial.print(haveActive ? drinkIndex + 1 : 0);
    Serial.print(F(" step="));
    Serial.print(stepIndex);
    Serial.print(F(" x="));
    Serial.print(st_x());
    Serial.print(F(" y="));
    Serial.print(st_y());
    Serial.print(F(" z="));
    Serial.print(st_zName());
    Serial.print(F(" door="));
    Serial.print(st_doorName());
    Serial.print(F(" led="));
    Serial.print(st_ledColorName());
    Serial.print(F(" nextSlot="));
    for (uint8_t shelf = 1; shelf <= SHELF_COUNT; ++shelf) {
        if (shelf > 1) Serial.print(',');
        Serial.print(inv_nextSlot(shelf));
    }
    Serial.print(F(" conv="));
    Serial.print(conveyorAdvances);
    Serial.println();
}

void startNextOrder() {
    if (haveActive || !oq_hasNext()) return;
    activeOrder = oq_peek();
    inv_setNextSlot(activeOrder.shelfId, activeOrder.startSlot);
    eb_orderStart(activeOrder.shelfId, activeOrder.quantity,
                  activeOrder.startSlot);
    eb_nextSlot(activeOrder.shelfId, activeOrder.startSlot);
    drinkIndex = 0;
    retryCount = 0;
    stepIndex = 0;
    conveyorPending = false;
    conveyorAdvances = 0;
    haveActive = true;
    manipulatorAtHome = false;
    stepCount = 0;
    noteProgress();
}

bool prepareDrink() {
    activeOrder.startSlot = inv_nextSlot(activeOrder.shelfId);
#if SMART_PLANNER
    stepCount = planOrderSmart(activeOrder, steps, PLAN_SIZE);
#else
    stepCount = planOrder(activeOrder, steps, PLAN_SIZE);
#endif
    stepIndex = 0;
    eb_drinkStart(static_cast<uint8_t>(drinkIndex + 1),
                  activeOrder.quantity, activeOrder.startSlot);
    return stepCount > 0;
}

void dispatchCurrent() {
    if (!haveActive) startNextOrder();
    if (!haveActive) return;
    if (conveyorPending) {
        const uint8_t command = conveyorCodeForShelf(activeOrder.shelfId);
        eb_step(CONVEYOR_STEP, 0, DIR_Y, command);
        pd_send(DIR_Y, command);
    } else {
        if (stepCount == 0 && !prepareDrink()) {
            state = State::ERROR;
            Serial.println(F("ERR unable to plan order"));
            return;
        }
#if SMART_PLANNER
        // The smart planner flagged no-op moves at
        // plan time. Walk the plan in order: trace
        // each skipped entry, then dispatch the next
        // real command. Actions are never flagged and
        // the plan ends on DOOR_CLOSE, so this always
        // lands on an entry that is sent — and a
        // skipped entry never arms the READY wait.
        while (stepIndex < PLAN_SIZE && steps[stepIndex].skip) {
            eb_stepSkip(static_cast<uint8_t>(stepIndex + 1),
                        stepCount, steps[stepIndex].axis,
                        steps[stepIndex].data);
            ++stepIndex;
        }
#endif
        const Cmd& step = steps[stepIndex];
        const uint8_t dir = step.axis == 'X' ? DIR_X : DIR_Y;
        eb_step(static_cast<uint8_t>(stepIndex + 1), stepCount,
                dir, static_cast<uint8_t>(step.data));
        pd_send(dir, static_cast<uint8_t>(step.data));
    }
    state = State::WAITING_FOR_READY;
    manualReadyPending = pd_isSimulation();
    readyDeadline = millis() + READY_TIMEOUT_MS;
    dispatchTime = millis();
    noteProgress();
}

void finishPrimitive() {
    if (conveyorPending) {
        // The conveyor advance settled: the row wrapped,
        // so the shelf restarts from the first slot.
        st_noteStep(DIR_Y, conveyorCodeForShelf(activeOrder.shelfId));
#if SMART_PLANNER
        // The belt shifted every column by one slot:
        // the X mapping resets to the dispensing area,
        // the shelf the manipulator sits on is unchanged.
        pl_noteConveyorSettled();
#endif
        const uint8_t shelf = activeOrder.shelfId;
        inv_resetSlots(shelf);
        conveyorPending = false;
        ++conveyorAdvances;
        eb_nextSlot(shelf, SLOT_FIRST);
        eb_drinkEnd();
        if (drinkIndex >= activeOrder.quantity) {
            oq_markDispensed();
            haveActive = false;
            manipulatorAtHome = true;
            eb_orderDone(activeOrder.shelfId, activeOrder.quantity,
                         conveyorAdvances, inv_nextSlot(activeOrder.shelfId));
            state = State::IDLE;
            if (autoRun) dispatchCurrent();
            return;
        }
        stepCount = 0;
    } else {
        // The planned primitive settled: fold it into the tracked
        // physical state before the step counter advances, so a
        // primitive that times out and is retried is never noted.
        const uint8_t dir =
            steps[stepIndex].axis == 'X' ? DIR_X : DIR_Y;
        const uint8_t data =
            static_cast<uint8_t>(steps[stepIndex].data);
        st_noteStep(dir, data);
#if SMART_PLANNER
        // The planner tracks the settled position so a
        // later move to the same target is a no-op.
        pl_noteSettled(dir, data);
#endif
        // The plan always ends on DOOR_CLOSE, so the
        // drink completes when the last entry settles.
        if (++stepIndex >= PLAN_SIZE) {
            ++drinkIndex;
            retryCount = 0;
            eb_drinkOk(activeOrder.orderId, drinkIndex);
            publishDrinkOutcome(activeOrder.orderId, drinkIndex, "ok");
            const uint8_t shelf = activeOrder.shelfId;
            const uint8_t next = static_cast<uint8_t>(inv_nextSlot(shelf) + 1);
            inv_setNextSlot(shelf, next);
            eb_nextSlot(shelf, next);
            if (inv_shouldAdvance(shelf)) {
                eb_conveyorCheck(next, SLOTS_PER_SHELF, true);
                conveyorPending = true;
            } else {
                eb_conveyorCheck(next, SLOTS_PER_SHELF, false);
                eb_drinkEnd();
                if (drinkIndex >= activeOrder.quantity) {
                    oq_markDispensed();
                    haveActive = false;
                    manipulatorAtHome = true;
                    eb_orderDone(activeOrder.shelfId, activeOrder.quantity,
                                 conveyorAdvances,
                                 inv_nextSlot(activeOrder.shelfId));
                    state = State::IDLE;
                    if (autoRun) dispatchCurrent();
                    return;
                }
                stepCount = 0;
            }
        }
    }
    state = State::IDLE;
    noteProgress();
    if (autoRun) dispatchCurrent();
}

// Name of a pick-routine phase for its instruction
// trace line.
const char* routinePhaseName(uint8_t data) {
    switch (data) {
    case SIM_Z_EXTEND: return "EXTEND";
    case SIM_GRIP_CLOSE: return "PICK";
    case SIM_Z_RETRACT: return "RETRACT";
    default: return "STEP";
    }
}

// Dispatches the routine phase at routineIndex through
// the manual-step path. Every phase of the routine is
// an instruction, so it is recorded in the system log
// as it dispatches.
void dispatchRoutinePhase() {
    manualDir = DIR_Y;
    manualData = routine[routineIndex];
    manualPending = true;
    pd_send(manualDir, manualData);
    state = State::WAITING_FOR_READY;
    readyDeadline = millis() + READY_TIMEOUT_MS;
    dispatchTime = millis();
    noteProgress();
    manualReadyPending = pd_isSimulation();
    char line[64];
    snprintf(line, sizeof(line),
             "Instruction from ESP: Z AXIS %s",
             routinePhaseName(manualData));
    eb_log(line);
}

void executeCommand(char* command) {
    char* cursor = command;
    while (*cursor == ' ' || *cursor == '\t') ++cursor;
    if (!*cursor) return;
    char* op = cursor;
    while (*cursor && *cursor != ' ' && *cursor != '\t') {
        if (*cursor >= 'a' && *cursor <= 'z') *cursor = static_cast<char>(*cursor - 'a' + 'A');
        ++cursor;
    }
    char* payload = cursor;
    if (*cursor) *cursor++ = '\0';
    while (*cursor == ' ' || *cursor == '\t') ++cursor;
    payload = cursor;
    char* arg = nullptr;
    char* arg2 = nullptr;
    if (strcmp(op, "ORDER") != 0) {
        arg = strtok(payload, " \t");
        arg2 = strtok(nullptr, " \t");
    }
    if (!*op) return;

    if (!strcmp(op, "HELP")) {
        printHelp();
    } else if (!strcmp(op, "STATUS")) {
        printStatus();
    } else if (!strcmp(op, "RESET")) {
        resetMachine();
        Serial.println(F("OK reset"));
    } else if (!strcmp(op, "SIM") && arg) {
        if (!strcmp(arg, "ON")) {
            pd_setSimulation(true);
            Serial.println(F("OK sim on"));
        } else if (!strcmp(arg, "OFF")) {
            pd_setSimulation(false);
            Serial.println(F("OK sim off"));
        } else {
            Serial.println(F("ERR sim expects on or off"));
        }
    } else if (!strcmp(op, "LED") && arg) {
        for (char* p = arg; *p; ++p) {
            if (*p >= 'a' && *p <= 'z') *p = static_cast<char>(*p - 'a' + 'A');
        }
        if (!strcmp(arg, "ON")) {
            st_setLedMode(LedMode::ON);
            Serial.println(F("OK led on"));
        } else if (!strcmp(arg, "OFF")) {
            st_setLedMode(LedMode::OFF);
            Serial.println(F("OK led off"));
        } else {
            Serial.println(F("ERR led expects on or off"));
        }
    } else if (!strcmp(op, "STALL") && arg) {
        if (!strcmp(arg, "ON")) {
            pd_setStall(true);
            Serial.println(F("OK stall on"));
        } else if (!strcmp(arg, "OFF")) {
            pd_setStall(false);
            Serial.println(F("OK stall off"));
        } else {
            Serial.println(F("ERR stall expects on or off"));
        }
    } else if (!strcmp(op, "RUN")) {
        autoRun = true;
        state = State::IDLE;
        dispatchCurrent();
    } else if (!strcmp(op, "STEP")) {
        if (busy()) {
            Serial.println(F("ERR wait for READY"));
        } else if (!haveActive && !oq_hasNext()) {
            Serial.println(F("ERR queue empty"));
        } else {
            autoRun = false;
            state = State::IDLE;
            dispatchCurrent();
        }
    } else if (!strcmp(op, "ORDER")) {
        Order parsed[ORDER_QUEUE_CAPACITY]{};
        uint8_t parsedCount = 0;
        char* group = payload;
        while (group && parsedCount < ORDER_QUEUE_CAPACITY) {
            char* next = strchr(group, ';');
            if (next) *next++ = '\0';
            const char* errorText = nullptr;
            if (!parseTriplet(group, parsed[parsedCount], errorText)) {
                Serial.print(F("ERR "));
                Serial.println(errorText);
                return;
            }
            ++parsedCount;
            group = next;
            if (group) {
                while (*group == ' ' || *group == '\t') ++group;
            }
        }
        if (group != nullptr) {
            Serial.println(F("ERR too many order groups"));
            return;
        }
        if (!parsedCount) {
            Serial.println(F("ERR order requires at least one triplet"));
            return;
        }
        for (uint8_t i = 0; i < parsedCount; ++i) {
            const char* error = submitOrder(parsed[i].shelfId,
                                              parsed[i].quantity,
                                              parsed[i].startSlot,
                                              parsed[i].orderId);
            if (error) {
                Serial.print(F("ERR "));
                Serial.println(error);
                return;
            }
        }
        Serial.print(F("OK order groups="));
        Serial.println(parsedCount);
    } else if (arg || arg2) {
        Serial.println(F("ERR unknown command"));
    } else {
        Serial.println(F("ERR unknown command"));
    }
}
}

void sm_handleCommand(char* command) { executeCommand(command); }

void startOrder() { startNextOrder(); }
void sm_noteProgress() { noteProgress(); }

void sm_init() {
    state = State::IDLE;
    autoRun = false;
    haveActive = false;
    conveyorPending = false;
    conveyorAdvances = 0;
    manipulatorAtHome = true;
    retryCount = 0;
    routineActive = false;
    routineCount = 0;
    routineIndex = 0;
    noteProgress();
    oq_clear();
    inv_resetAll();
    pd_setSimulation(
#ifdef SIM_MODE
        true
#else
        false
#endif
    );
    pd_init();
    st_updateLed();
#if SMART_PLANNER
    // Boot: the position is unknown until endstops
    // or a settled move confirm it.
    pl_resetTracking();
#endif
    sched_init();
}

void sm_update() {
    sched_update();
    if (state == State::WAITING_FOR_READY && pd_readyReceived()) {
        eb_ready(millis() - dispatchTime);
        manualReadyPending = false;
        if (manualPending) {
            manualPending = false;
            st_noteStep(manualDir, manualData);
#if SMART_PLANNER
            // A manual move settles the tracked
            // position too, so a later order can
            // skip a move the operator already
            // performed by hand.
            pl_noteSettled(manualDir, manualData);
#endif
            if (routineActive && routineIndex + 1 < routineCount) {
                // The routine continues: dispatch the
                // next phase and hold it for READY.
                ++routineIndex;
                dispatchRoutinePhase();
            } else {
                routineActive = false;
                state = State::IDLE;
                noteProgress();
            }
        } else {
            finishPrimitive();
        }
    } else if (manualReadyPending) {
        // Operator-gated: the machine holds the current
        // primitive until the READY button releases it —
        // no timeout, no retry, no halt.
    } else if (manualPending &&
               static_cast<int32_t>(millis() - readyDeadline) >= 0) {
        // A manual primitive has no drink to retry: drop
        // back to IDLE so the next instruction can run.
        manualPending = false;
        state = State::IDLE;
        noteProgress();
        eb_log("manual move timed out — READY not received");
    } else if (state == State::WAITING_FOR_READY &&
               static_cast<int32_t>(millis() - readyDeadline) >= 0) {
        ++retryCount;
        if (retryCount <= DRINK_RETRY_LIMIT) {
            // The whole drink is retried from step 1 at the
            // same slot: nextSlot is only advanced once the
            // final step settles, so a timeout never skips a
            // slot. The conveyor advance is step 9 and warns
            // without the /8 denominator.
            if (conveyorPending) {
                eb_warn(CONVEYOR_STEP, 0, retryCount,
                        DRINK_RETRY_LIMIT);
            } else {
                eb_warn(static_cast<uint8_t>(stepIndex + 1),
                        stepCount, retryCount, DRINK_RETRY_LIMIT);
            }
            state = State::IDLE;
            stepCount = 0;
            stepIndex = 0;
            noteProgress();
            if (autoRun) dispatchCurrent();
        } else {
            eb_error(DRINK_RETRY_LIMIT, HALT_ON_DRINK_FAILURE);
            publishDrinkOutcome(activeOrder.orderId, drinkIndex + 1,
                                "failed");
            if (HALT_ON_DRINK_FAILURE) {
                state = State::ERROR;
                autoRun = false;
                st_noteError();
#if SMART_PLANNER
                // E-stop: the position is no longer
                // trustworthy.
                pl_resetTracking();
#endif
            } else {
                ++drinkIndex;
                retryCount = 0;
                stepCount = 0;
                state = State::IDLE;
                if (drinkIndex >= activeOrder.quantity) {
                    oq_markDispensed();
                    haveActive = false;
                    manipulatorAtHome = true;
                    eb_orderDone(activeOrder.shelfId,
                                 activeOrder.quantity,
                                 conveyorAdvances,
                                 inv_nextSlot(activeOrder.shelfId));
                } else if (autoRun) {
                    dispatchCurrent();
                }
            }
        }
    } else if (state != State::IDLE && haveActive && !manipulatorAtHome &&
               static_cast<uint32_t>(millis() - lastProgressMs) >=
                   ORDER_TIMEOUT_MS) {
        state = State::ERROR;
        autoRun = false;
        st_noteError();
#if SMART_PLANNER
        // E-stop: the position is no longer
        // trustworthy.
        pl_resetTracking();
#endif
        Serial.println(F("ERR order timeout; reset required"));
    }
    // The machine drains its queue on its own: an order
    // submitted from any door starts as soon as the
    // manipulator is free, so the sim page needs no
    // separate run command to see the full trace.
    if (state == State::IDLE && !haveActive && oq_hasNext()) {
        autoRun = true;
        dispatchCurrent();
    }
    st_updateLed();
    eb_state();
}

State sm_state() { return state; }
bool isReturning() { return busy() || haveActive || oq_hasNext(); }
bool sm_hasActiveOrder() { return haveActive; }
Order sm_activeOrder() { return activeOrder; }
uint8_t sm_drinkIndex() { return drinkIndex; }
uint8_t sm_stepIndex() { return stepIndex; }
uint32_t sm_conveyorAdvances() { return conveyorAdvances; }
const char* sm_stateName() { return stateName(); }
const char* sm_stateName(State value) { return stateName(value); }

const char* sm_submitOrder(uint8_t shelfId, uint8_t quantity,
                               uint8_t startSlot, const char* orderId) {
    return submitOrder(shelfId, quantity, startSlot, orderId);
}

void sm_reset() { resetMachine(); }

// READY button release. Guards on the waiting flag so a
// press between primitives logs the signal without
// arming a stray READY for the next dispatch.
bool sm_signalReady() {
    if (state != State::WAITING_FOR_READY || !manualReadyPending) {
        return false;
    }
    manualReadyPending = false;
    pd_signalReady();
    return true;
}

// Manual single-primitive dispatch. The control panel sends
// absolute targets; the direction is carried by the caller's
// instruction, the machine simply moves to the target.
const char* sm_manualStep(uint8_t dir, uint8_t data) {
    if (state == State::ERROR) {
        return "machine halted; send reset";
    }
    if (state != State::IDLE || haveActive || oq_hasNext()) {
        return "machine busy";
    }
    manualDir = dir;
    manualData = data;
    manualPending = true;
    pd_send(dir, data);
    state = State::WAITING_FOR_READY;
    readyDeadline = millis() + READY_TIMEOUT_MS;
    dispatchTime = millis();
    noteProgress();
    manualReadyPending = pd_isSimulation();
    return nullptr;
}

// SIM_MODE pick routine: extend the manipulator, pick
// the drink, then retract. Each phase dispatches as its
// own primitive and holds for the operator's READY
// button, so the full sequence is three gated steps.
const char* sm_startPickRoutine() {
    if (state == State::ERROR) {
        return "machine halted; send reset";
    }
    if (state != State::IDLE || haveActive || oq_hasNext()) {
        return "machine busy";
    }
    routine[0] = SIM_Z_EXTEND;
    routine[1] = SIM_GRIP_CLOSE;
    routine[2] = SIM_Z_RETRACT;
    routineCount = 3;
    routineIndex = 0;
    routineActive = true;
    dispatchRoutinePhase();
    return nullptr;
}

}  // namespace revov
