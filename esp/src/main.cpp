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
#include "Order/Inventory.h"
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
    bool changed = state != lastPrintedState || queue != lastPrintedQueue ||
                   connected != lastPrintedConnected ||
                   active != lastPrintedActive ||
                   revov::sm_drinkIndex() != lastPrintedDrink ||
                   revov::sm_stepIndex() != lastPrintedStep;
    for (uint8_t i = 0; i < revov::SHELF_COUNT; ++i) {
        changed = changed || revov::inv_nextSlot(i + 1) != lastPrintedSlots[i];
    }
    if (active) {
        changed = changed || order.shelfId != lastPrintedOrder.shelfId ||
                  order.quantity != lastPrintedOrder.quantity ||
                  order.startSlot != lastPrintedOrder.startSlot;
    }
    if (!changed) return;
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
#ifdef ESP32
    esp_bt_controller_disable();
#endif
    WiFi.mode(WIFI_STA);
    if (revov::WIFI_SSID[0] != '\0') {
        WiFi.begin(revov::WIFI_SSID, revov::WIFI_PASSWORD);
    }
    revov::sm_init();
    revov::ws_connect();
    printHeader();
    Serial.println(F("Type help for commands."));
}

void loop() {
    revov::ws_maintain();
    revov::checkBackendContact();
    revov::Order incoming[revov::ORDER_QUEUE_CAPACITY]{};
    uint8_t count = 0;
    if (revov::ws_pollOrder(incoming, count)) {
        for (uint8_t i = 0; i < count; ++i) {
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
