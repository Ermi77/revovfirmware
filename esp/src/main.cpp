#include <Arduino.h>
#ifdef ESP32
#include <esp_bt.h>
#include <esp_bt_main.h>
#endif

#include "Config.h"
#include "Core/Diag.h"
#include "Core/Scheduler.h"
#include "Core/StateMachine.h"
#include "Net/WebSocket.h"
#include "Order/Inventory.h"
#include "Order/OrderQueue.h"
#include "Protocol/PinDriver.h"
#include "Protocol/ReadyHandler.h"
#include "Trajectory/Planner.h"

namespace {
char serialInput[revov::CMD_MAX_LENGTH];
uint8_t serialLength = 0;
uint32_t lastHeapLog = 0;
revov::State lastPrintedState = revov::State::ERROR;
uint8_t lastPrintedQueue = 255;
bool lastPrintedConnected = false;
bool lastPrintedActive = false;
revov::Order lastPrintedOrder{0, 0, 0, ""};
uint8_t lastPrintedDrink = 255;
uint8_t lastPrintedStep = 255;
uint8_t lastPrintedSlots[revov::SHELF_COUNT] = {255, 255, 255, 255, 255};
bool firstStatus = true;

void printHeader() {
    Serial.println(F("================================"));
    Serial.println(F("RevoV ESP32 brain"));
    Serial.println(F("Transport: WebSocket Secure"));
#ifdef SIM_MODE
    Serial.println(F("Mode: SIM_MODE"));
#else
    Serial.println(F("Mode: production"));
#endif
    Serial.println(F("================================"));
}

void logHeap() {
#ifdef ESP32
    Serial.print(F("[MEM] free heap="));
    Serial.println(ESP.getFreeHeap());
#endif
}

void printStatus() {
    const revov::State state = revov::sm_state();
    const uint8_t queue = revov::oq_size();
    const bool connected = revov::ws_isConnected();
    const bool active = revov::sm_hasActiveOrder();
    const revov::Order order = revov::sm_activeOrder();
    const uint8_t drink = revov::sm_drinkIndex();
    const uint8_t step = revov::sm_stepIndex();
    bool changed = state != lastPrintedState || queue != lastPrintedQueue ||
                   connected != lastPrintedConnected ||
                   active != lastPrintedActive ||
                   drink != lastPrintedDrink || step != lastPrintedStep;
    bool slotsChanged = false;
    for (uint8_t i = 0; i < revov::SHELF_COUNT; ++i) {
        if (revov::inv_nextSlot(i + 1) != lastPrintedSlots[i]) {
            slotsChanged = true;
            changed = true;
        }
    }
    if (active) {
        changed = changed || order.shelfId != lastPrintedOrder.shelfId ||
                  order.quantity != lastPrintedOrder.quantity ||
                  order.startSlot != lastPrintedOrder.startSlot;
    }
    if (!changed) return;
    if (!firstStatus) {
        if (state != lastPrintedState) {
            Serial.print(F("[TRACE] state "));
            Serial.print(revov::sm_stateName(lastPrintedState));
            Serial.print(F(" -> "));
            Serial.print(revov::sm_stateName());
            Serial.println();
        }
        if (queue != lastPrintedQueue) {
            Serial.print(F("[TRACE] queue "));
            Serial.print(lastPrintedQueue);
            Serial.print(F(" -> "));
            Serial.print(queue);
            Serial.println();
        }
        if (connected != lastPrintedConnected) {
            Serial.print(F("[TRACE] wss "));
            Serial.print(lastPrintedConnected ? 1 : 0);
            Serial.print(F(" -> "));
            Serial.print(connected ? 1 : 0);
            Serial.println();
        }
        if (drink != lastPrintedDrink || step != lastPrintedStep) {
            Serial.print(F("[TRACE] progress drink="));
            Serial.print(drink);
            Serial.print(F(" step="));
            Serial.print(step);
            Serial.println();
        }
        if (active && (order.shelfId != lastPrintedOrder.shelfId ||
                       order.quantity != lastPrintedOrder.quantity ||
                       order.startSlot != lastPrintedOrder.startSlot)) {
            Serial.print(F("[TRACE] active order shelf="));
            Serial.print(order.shelfId);
            Serial.print(F(" qty="));
            Serial.print(order.quantity);
            Serial.print(F(" slot="));
            Serial.print(order.startSlot);
            Serial.println();
        }
        if (slotsChanged) {
            Serial.print(F("[TRACE] nextSlot="));
            for (uint8_t i = 0; i < revov::SHELF_COUNT; ++i) {
                if (i) Serial.print(',');
                Serial.print(revov::inv_nextSlot(i + 1));
            }
            Serial.println();
        }
    }
    firstStatus = false;
    lastPrintedState = state;
    lastPrintedQueue = queue;
    lastPrintedConnected = connected;
    lastPrintedActive = active;
    lastPrintedOrder = order;
    lastPrintedDrink = revov::sm_drinkIndex();
    lastPrintedStep = revov::sm_stepIndex();
    for (uint8_t i = 0; i < revov::SHELF_COUNT; ++i) {
        lastPrintedSlots[i] = revov::inv_nextSlot(i + 1);
    }
    Serial.print(F("[STATUS] state="));
    Serial.print(revov::sm_stateName());
    Serial.print(F(" wss="));
    Serial.print(connected ? 1 : 0);
    Serial.print(F(" queue="));
    Serial.print(queue);
    Serial.print(F(" active="));
    if (active) {
        Serial.print(order.shelfId);
        Serial.print(',');
        Serial.print(order.quantity);
        Serial.print(',');
        Serial.print(order.startSlot);
    } else {
        Serial.print(F("none"));
    }
    Serial.print(F(" drink="));
    Serial.print(revov::sm_drinkIndex());
    Serial.print(F(" step="));
    Serial.print(revov::sm_stepIndex());
    Serial.print(F(" nextSlot="));
    for (uint8_t i = 0; i < revov::SHELF_COUNT; ++i) {
        if (i) Serial.print(',');
        Serial.print(revov::inv_nextSlot(i + 1));
    }
    Serial.println();
}

void handleSerialInput() {
    while (Serial.available()) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r') continue;
        if (c == '\n') {
            serialInput[serialLength] = '\0';
            Serial.print(F("[CMD] "));
            Serial.println(serialInput);
            revov::sm_handleCommand(serialInput);
            serialLength = 0;
        } else if (serialLength + 1U < revov::CMD_MAX_LENGTH) {
            serialInput[serialLength++] = c;
        }
    }
}
}

void setup() {
    Serial.begin(115200);
    Serial.println(F("Hello, world!"));
#ifdef ESP32
    esp_bt_controller_disable();
#endif
    Serial.println(F("[BOOT] revov esp32 boot"));
    Serial.print(F("[BOOT] wifi ssid set="));
    Serial.println(revov::WIFI_SSID[0] != '\0' ? 1 : 0);
    Serial.print(F("[BOOT] ws host set="));
    Serial.println(revov::WEBSOCKET_HOST[0] != '\0' ? 1 : 0);
    WiFi.mode(WIFI_STA);
    if (revov::WIFI_SSID[0] != '\0') {
        WiFi.begin(revov::WIFI_SSID, revov::WIFI_PASSWORD);
        Serial.println(F("[BOOT] wifi begin"));
    } else {
        Serial.println(F("[BOOT] wifi skipped (no ssid configured)"));
    }
    revov::sm_init();
    Serial.println(F("[BOOT] state machine initialized"));
    const bool wsStarted = revov::ws_connect();
    Serial.print(F("[BOOT] ws start ok="));
    Serial.println(wsStarted ? 1 : 0);
    printHeader();
    Serial.println(F("Type help for commands."));
}

void loop() {
    revov::ws_maintain();
    revov::checkBackendContact();
    revov::Order incoming[revov::ORDER_QUEUE_CAPACITY]{};
    uint8_t count = 0;
    if (revov::ws_pollOrder(incoming, count)) {
        Serial.print(F("[NET] polled "));
        Serial.print(count);
        Serial.println(F(" order(s)"));
        for (uint8_t i = 0; i < count; ++i) {
            Serial.print(F("[NET] order id="));
            Serial.print(incoming[i].orderId);
            Serial.print(F(" shelf="));
            Serial.print(incoming[i].shelfId);
            Serial.print(F(" qty="));
            Serial.print(incoming[i].quantity);
            Serial.print(F(" slot="));
            Serial.print(incoming[i].startSlot);
            Serial.println();
            revov::oq_enqueueOrder(incoming[i]);
        }
    }
    handleSerialInput();
    revov::sm_update();
    printStatus();
    if (millis() - lastHeapLog >= revov::HEAP_LOG_INTERVAL_MS) {
        lastHeapLog = millis();
        logHeap();
    }
}
