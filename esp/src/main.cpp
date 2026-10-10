#include <Arduino.h>
#ifdef ESP32
#include <esp_bt.h>
#include <esp_bt_main.h>
#endif

#include <ESPmDNS.h>

#include "Config.h"
#include "Core/EventBus.h"
#include "Core/Scheduler.h"
#include "Core/StateMachine.h"
#include "Net/WebSocket.h"
#include "Order/Inventory.h"
#include "Order/OrderQueue.h"
#include "Protocol/PinDriver.h"
#include "Protocol/ReadyHandler.h"
#include "Trajectory/Planner.h"
#include "Web/Ui.h"

namespace {
char serialInput[revov::CMD_MAX_LENGTH];
uint8_t serialLength = 0;
uint32_t lastHeapLog = 0;
revov::State lastPrintedState = revov::State::ERROR;
uint8_t lastPrintedQueue = 255;
bool lastPrintedConnected = false;
bool lastPrintedActive = false;
uint8_t lastPrintedStations = 255;
revov::Order lastPrintedOrder{0, 0, 0, ""};
uint8_t lastPrintedDrink = 255;
uint8_t lastPrintedStep = 255;
uint8_t lastPrintedSlots[revov::SHELF_COUNT] = {255, 255, 255, 255, 255};
bool firstStatus = true;
bool pageAnnounced = false;
bool staAnnounced = false;
uint32_t lastWifiKeepAlive = 0;
uint32_t lastStatusMs = 0;

// Registers the mDNS hostname so the sim page is reachable
// as http://revov.local/ on the served interface.
void startMdns() {
    if (MDNS.begin("revov")) {
        MDNS.addService("http", "tcp", static_cast<uint16_t>(revov::WEB_UI_PORT));
    }
}

// Prints the sim page URL on the served interface.
void announceSimPage() {
    const IPAddress address =
#ifdef SIM_MODE
        WiFi.softAPIP();
#else
        WiFi.localIP();
#endif
    Serial.print(F("[NET] sim page: http://"));
    Serial.print(address);
    Serial.print(F("/"));
    Serial.println();
}

// Prints the station address once the board joins
// the local network, so LAN devices know where to
// reach the page.
void announceStaPage() {
    Serial.print(F("[NET] sta page: http://"));
    Serial.print(WiFi.localIP());
    Serial.print(F("/"));
    Serial.println();
}

// Net/WebSocket.cpp stays untouched, so the Wi-Fi keep-alive
// lives here: when an SSID is configured but the station is
// down, retry every keep-alive interval, and announce the
// sim page URL once the link is up.
void wifiKeepAlive() {
#ifdef SIM_MODE
    // The sim page is served from the soft AP, which is up
    // for the whole run; announce once mDNS has registered.
    if (!pageAnnounced) {
        pageAnnounced = true;
        startMdns();
        announceSimPage();
    }
    // The station link takes a few seconds to associate;
    // announce the LAN address as soon as it is up.
    if (!staAnnounced && WiFi.status() == WL_CONNECTED) {
        staAnnounced = true;
        announceStaPage();
    }
    const uint8_t stations = WiFi.softAPgetStationNum();
    if (stations != lastPrintedStations) {
        lastPrintedStations = stations;
        Serial.print(F("[NET] sim ap stations="));
        Serial.println(stations);
    }
#else
    if (WiFi.status() == WL_CONNECTED) {
        if (!pageAnnounced) {
            pageAnnounced = true;
            startMdns();
            announceSimPage();
        }
        return;
    }
    if (revov::WIFI_SSID[0] == '\0') return;
    const uint32_t now = millis();
    if (static_cast<uint32_t>(now - lastWifiKeepAlive) >=
        revov::WIFI_KEEPALIVE_INTERVAL_MS) {
        lastWifiKeepAlive = now;
        WiFi.disconnect();
        WiFi.begin(revov::WIFI_SSID, revov::WIFI_PASSWORD);
        Serial.println(F("[NET] Wi-Fi keep-alive retry"));
    }
#endif
}

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

// The unit-test build compiles this file alongside the
// test suites, which own setup()/loop() there, so the
// sketch entry points are excluded from test builds.
#ifndef UNIT_TEST
#ifndef PIO_UNIT_TESTING
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
#ifdef SIM_MODE
    // The sim console is served from a soft AP so the order
    // form, trajectory box, reset, and LED are reachable
    // without backend credentials or a station network.
    // A station link to the configured network runs
    // alongside the AP, so the board also has internet
    // and is reachable from the LAN.
    WiFi.mode(WIFI_AP_STA);
    const bool apStarted =
        WiFi.softAP(revov::SIM_AP_SSID, revov::SIM_AP_PASSWORD);
    if (revov::WIFI_SSID[0] != '\0') {
        WiFi.begin(revov::WIFI_SSID, revov::WIFI_PASSWORD);
    }
    Serial.print(F("[BOOT] sim ap ssid="));
    Serial.println(revov::SIM_AP_SSID);
    Serial.print(F("[BOOT] sim ap started="));
    Serial.println(apStarted ? 1 : 0);
    Serial.print(F("[BOOT] sim ap ip="));
    Serial.println(WiFi.softAPIP().toString());
#else
    WiFi.mode(WIFI_STA);
    // Default DTIM sleep adds 100-300 ms to every request
    // on the station interface; keep the radio awake for
    // the web UI. (No-op for the sim soft AP.)
    WiFi.setSleep(false);
    if (revov::WIFI_SSID[0] != '\0') {
        WiFi.begin(revov::WIFI_SSID, revov::WIFI_PASSWORD);
        Serial.println(F("[BOOT] wifi begin"));
    } else {
        Serial.println(F("[BOOT] wifi skipped (no ssid configured)"));
    }
#endif
    revov::sm_init();
    Serial.println(F("[BOOT] state machine initialized"));
    revov::eb_init();
    revov::webui_init();
    Serial.println(F("[BOOT] web ui initialized"));
    const bool wsStarted = revov::ws_connect();
    Serial.print(F("[BOOT] ws start ok="));
    Serial.println(wsStarted ? 1 : 0);
    printHeader();
    Serial.println(F("Type help for commands."));
}

void loop() {
    wifiKeepAlive();
    revov::ws_maintain();
    revov::checkBackendContact();
    revov::webui_service();
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
            const char* error = revov::sm_submitOrder(
                incoming[i].shelfId, incoming[i].quantity,
                incoming[i].startSlot, incoming[i].orderId);
            if (error) {
                Serial.print(F("[NET] order rejected: "));
                Serial.println(error);
            }
        }
    }
    handleSerialInput();
    revov::sm_update();
    if (millis() - lastStatusMs >= revov::STATUS_LOG_INTERVAL_MS) {
        lastStatusMs = millis();
        printStatus();
    }
    if (millis() - lastHeapLog >= revov::HEAP_LOG_INTERVAL_MS) {
        lastHeapLog = millis();
        logHeap();
    }
}
#endif
#endif
