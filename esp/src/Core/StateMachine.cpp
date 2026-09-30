#include "StateMachine.h"

#include "../Config.h"
#include "../Net/WebSocket.h"
#include "../Order/Inventory.h"
#include "../Order/OrderQueue.h"
#include "../Protocol/PinDriver.h"
#include "../Trajectory/Planner.h"
#include "Scheduler.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

namespace revov {
namespace {
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
Order activeOrder{0, 0, 0, ""};
ActionStep steps[8]{};
uint8_t stepCount = 0;

const char* stateName() {
    switch (state) {
    case State::DISPATCHING: return "DISPATCHING";
    case State::WAITING_FOR_READY: return "WAITING_FOR_READY";
    case State::ERROR: return "ERROR";
    default: return "IDLE";
    }
}

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
    if (order.startSlot >= SLOTS_PER_SHELF) {
        errorText = "startSlot must be 0..9";
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
    if (startSlot < 0 || startSlot >= SLOTS_PER_SHELF) {
        errorText = "startSlot must be 0..9";
        return false;
    }
    order = {};
    order.shelfId = static_cast<uint8_t>(shelf);
    order.quantity = static_cast<uint8_t>(quantity);
    order.startSlot = static_cast<uint8_t>(startSlot);
    return validOrder(order, errorText);
}

bool busy() { return state == State::DISPATCHING || state == State::WAITING_FOR_READY; }

void noteProgress() { lastProgressMs = millis(); }

void printHelp() {
    Serial.println(F("help"));
    Serial.println(F("order <shelfId> <quantity> <startSlot> [; shelfId quantity startSlot ...]"));
    Serial.println(F("status | run | step | reset | sim on | sim off"));
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
    Serial.print(F(" nextSlot="));
    for (uint8_t shelf = 1; shelf <= SHELF_COUNT; ++shelf) {
        if (shelf > 1) Serial.print(',');
        Serial.print(inv_nextSlot(shelf));
    }
    Serial.println();
}

void startNextOrder() {
    if (haveActive || !oq_hasNext()) return;
    activeOrder = oq_peek();
    inv_setNextSlot(activeOrder.shelfId, activeOrder.startSlot);
    drinkIndex = 0;
    retryCount = 0;
    stepIndex = 0;
    conveyorPending = false;
    haveActive = true;
    manipulatorAtHome = false;
    stepCount = 0;
    noteProgress();
}

bool prepareDrink() {
    activeOrder.startSlot = inv_nextSlot(activeOrder.shelfId);
    stepCount = planOrder(activeOrder, steps, 8);
    stepIndex = 0;
    return stepCount == 8;
}

void dispatchCurrent() {
    if (!haveActive) startNextOrder();
    if (!haveActive) return;
    if (conveyorPending) {
        const uint8_t command = conveyorCodeForShelf(activeOrder.shelfId);
        pd_send(DIR_Y, command);
    } else {
        if (stepCount == 0 && !prepareDrink()) {
            state = State::ERROR;
            Serial.println(F("ERR unable to plan order"));
            return;
        }
        const ActionStep& step = steps[stepIndex];
        pd_send(step.dir, step.data);
    }
    state = State::WAITING_FOR_READY;
    readyDeadline = millis() + READY_TIMEOUT_MS;
    noteProgress();
}

void finishPrimitive() {
    if (conveyorPending) {
        inv_resetSlots(activeOrder.shelfId);
        conveyorPending = false;
        if (drinkIndex >= activeOrder.quantity) {
            oq_markDispensed();
            haveActive = false;
            manipulatorAtHome = true;
            Serial.println(F("ORDER COMPLETE"));
            state = State::IDLE;
            if (autoRun) dispatchCurrent();
            return;
        }
        stepCount = 0;
    } else if (++stepIndex >= stepCount) {
        ++drinkIndex;
        retryCount = 0;
        ws_publishDrinkOutcome(activeOrder.orderId, drinkIndex, "ok");
        inv_setNextSlot(activeOrder.shelfId, static_cast<uint8_t>(inv_nextSlot(activeOrder.shelfId) + 1));
        if (inv_shouldAdvance(activeOrder.shelfId)) {
            conveyorPending = true;
        } else if (drinkIndex >= activeOrder.quantity) {
            oq_markDispensed();
            haveActive = false;
            manipulatorAtHome = true;
            Serial.println(F("ORDER COMPLETE"));
            state = State::IDLE;
            if (autoRun) dispatchCurrent();
            return;
        } else {
            stepCount = 0;
        }
    }
    state = State::IDLE;
    noteProgress();
    if (autoRun) dispatchCurrent();
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
        oq_clear();
        inv_resetAll();
        haveActive = false;
        conveyorPending = false;
        autoRun = false;
        state = State::IDLE;
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
        if (busy() || haveActive) {
            Serial.println(F("ERR busy"));
            return;
        }
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
            oq_enqueue(parsed[i].shelfId, parsed[i].quantity, parsed[i].startSlot);
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
    manipulatorAtHome = true;
    retryCount = 0;
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
    sched_init();
}

void sm_update() {
    sched_update();
    if (state == State::WAITING_FOR_READY && pd_readyReceived()) {
        finishPrimitive();
    } else if (state == State::WAITING_FOR_READY &&
               static_cast<int32_t>(millis() - readyDeadline) >= 0) {
        ++retryCount;
        if (retryCount <= DRINK_RETRY_LIMIT) {
            Serial.print(F("[DISPENSE] retry "));
            Serial.println(retryCount);
            state = State::IDLE;
            stepCount = 0;
            stepIndex = 0;
            noteProgress();
            if (autoRun) dispatchCurrent();
        } else {
            Serial.println(F("[DISPENSE] drink failed after 3 retries"));
            ws_publishDrinkOutcome(activeOrder.orderId, drinkIndex + 1, "failed");
            if (HALT_ON_DRINK_FAILURE) {
                state = State::ERROR;
                autoRun = false;
            } else {
                ++drinkIndex;
                retryCount = 0;
                stepCount = 0;
                state = State::IDLE;
                if (drinkIndex >= activeOrder.quantity) {
                    oq_markDispensed();
                    haveActive = false;
                    manipulatorAtHome = true;
                    Serial.println(F("ORDER COMPLETE"));
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
        Serial.println(F("ERR order timeout; reset required"));
    }
}

State sm_state() { return state; }
bool isReturning() { return busy() || haveActive || oq_hasNext(); }
bool sm_hasActiveOrder() { return haveActive; }
Order sm_activeOrder() { return activeOrder; }
uint8_t sm_drinkIndex() { return drinkIndex; }
uint8_t sm_stepIndex() { return stepIndex; }
const char* sm_stateName() { return stateName(); }

}  // namespace revov
