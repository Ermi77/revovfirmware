#include "WebSocket.h"

#include "../Config.h"

#ifdef ARDUINO
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <WiFi.h>
#include <string.h>
#endif

namespace revov {
namespace {

#ifdef ARDUINO
WebSocketsClient socket;
#endif

Order pending[ORDER_QUEUE_CAPACITY]{};
uint8_t pendingCount = 0;
bool connected = false;
bool configured = false;
bool lastWifiConnected = false;
uint32_t lastWifiAttempt = 0;
uint32_t lastNetworkLog = 0;
uint32_t lastContact = 0;
bool diagnosticsPrinted = false;
bool credentialsRejected = false;
char currentOrderId[32] = "";

#ifdef ARDUINO
void copyOrderId(JsonVariantConst value, char (&target)[32]) {
    target[0] = '\0';
    if (!value.is<const char*>()) return;
    strncpy(target, value.as<const char*>(), sizeof(target) - 1);
    target[sizeof(target) - 1] = '\0';
}

bool credentialsMatch(JsonObjectConst object) {
    JsonVariantConst idValue = object["MACHINE_ID"];
    JsonVariantConst tokenValue = object["MACHINE_TOKEN"];
    if (idValue.isNull()) idValue = object["machineId"];
    if (tokenValue.isNull()) tokenValue = object["machineToken"];
    const char* id = idValue | "";
    const char* token = tokenValue | "";
    return strcmp(id, revov::MACHINE_ID) == 0 &&
           strcmp(token, revov::MACHINE_TOKEN) == 0;
}

void rejectCredentials() {
    if (credentialsRejected) return;
    credentialsRejected = true;
    Serial.println(F("[NET] rejected: bad machine credentials"));
}

bool parseOrder(JsonObjectConst object, Order& order, const char* orderId) {
    if (!object["shelfId"].is<int>() || !object["quantity"].is<int>() ||
        !object["startSlot"].is<int>()) {
        return false;
    }
    const int shelf = object["shelfId"].as<int>();
    const int quantity = object["quantity"].as<int>();
    const int startSlot = object["startSlot"].as<int>();
    if (shelf < 1 || shelf > SHELF_COUNT || quantity < 1 ||
        quantity > MAX_ORDER_QUANTITY || startSlot < 0 ||
        startSlot >= SLOTS_PER_SHELF) {
        return false;
    }
    order = {};
    order.shelfId = static_cast<uint8_t>(shelf);
    order.quantity = static_cast<uint8_t>(quantity);
    order.startSlot = static_cast<uint8_t>(startSlot);
    if (orderId) {
        strncpy(order.orderId, orderId, sizeof(order.orderId) - 1);
        order.orderId[sizeof(order.orderId) - 1] = '\0';
    }
    return true;
}

void onSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
    if (type != WStype_DISCONNECTED) lastContact = millis();
    if (type == WStype_CONNECTED) {
        connected = true;
        diagnosticsPrinted = false;
        credentialsRejected = false;
        Serial.println(F("[NET] WebSocket connected"));
        return;
    }
    if (type == WStype_DISCONNECTED) {
        connected = false;
        Serial.println(F("[NET] WebSocket disconnected"));
        diagnoseConnectionError();
        return;
    }
    if (type != WStype_TEXT || !payload || length == 0) return;

    StaticJsonDocument<1024> document;
    const DeserializationError error = deserializeJson(document, payload, length);
    if (error) {
        Serial.print(F("[NET] JSON rejected: "));
        Serial.println(error.c_str());
        return;
    }
    JsonVariantConst value = document.as<JsonVariantConst>();
    const char* orderId = "";
    if (value.is<JsonObjectConst>()) {
        JsonObjectConst root = value.as<JsonObjectConst>();
        if (!credentialsMatch(root)) {
            rejectCredentials();
            return;
        }
        orderId = root["orderId"] | "";
        JsonArrayConst orders = root["orders"].as<JsonArrayConst>();
        if (!orders.isNull()) {
            if (orders.size() > ORDER_QUEUE_CAPACITY ||
                pendingCount + orders.size() > ORDER_QUEUE_CAPACITY) {
                Serial.println(F("[NET] order rejected: pending buffer full"));
                return;
            }
            for (JsonVariantConst item : orders) {
                if (!item.is<JsonObjectConst>()) {
                    Serial.println(F("[NET] order batch rejected"));
                    return;
                }
                Order parsed{};
                if (!parseOrder(item.as<JsonObjectConst>(), parsed, orderId)) {
                    Serial.println(F("[NET] order batch rejected"));
                    return;
                }
                pending[pendingCount++] = parsed;
            }
            return;
        }
        if (pendingCount >= ORDER_QUEUE_CAPACITY) {
            Serial.println(F("[NET] order rejected: pending buffer full"));
            return;
        }
        Order parsed{};
        if (!parseOrder(root, parsed, orderId)) {
            Serial.println(F("[NET] order rejected: invalid schema"));
            return;
        }
        pending[pendingCount++] = parsed;
        return;
    } else if (value.is<JsonArrayConst>()) {
        JsonArrayConst orders = value.as<JsonArrayConst>();
        if (orders.isNull() || orders.size() == 0 ||
            !orders[0].is<JsonObjectConst>() ||
            !credentialsMatch(orders[0].as<JsonObjectConst>())) {
            rejectCredentials();
            return;
        }
        orderId = orders[0]["orderId"] | "";
        if (orders.size() > ORDER_QUEUE_CAPACITY ||
            pendingCount + orders.size() > ORDER_QUEUE_CAPACITY) {
            Serial.println(F("[NET] order rejected: pending buffer full"));
            return;
        }
        for (JsonVariantConst item : orders) {
            if (!item.is<JsonObjectConst>()) {
                Serial.println(F("[NET] order batch rejected"));
                return;
            }
            Order parsed{};
            if (!parseOrder(item.as<JsonObjectConst>(), parsed, orderId)) {
                Serial.println(F("[NET] order batch rejected"));
                return;
            }
            pending[pendingCount++] = parsed;
        }
        return;
    } else {
        Serial.println(F("[NET] order rejected: expected object or array"));
        return;
    }
}
#endif

}  // namespace

bool ws_connect() {
#ifdef ARDUINO
    configured = WIFI_SSID[0] != '\0' && WIFI_PASSWORD[0] != '\0' &&
                 WEBSOCKET_HOST[0] != '\0';
    if (!configured || WiFi.status() != WL_CONNECTED) return false;
    if (WEBSOCKET_CA_CERT[0] != '\0') {
        socket.beginSslWithCA(WEBSOCKET_HOST, WEBSOCKET_PORT, WEBSOCKET_PATH,
                              WEBSOCKET_CA_CERT);
    } else {
#ifdef SIM_MODE
        Serial.println(F("[NET] WARNING: WSS certificate validation disabled in SIM_MODE"));
        socket.beginSSL(WEBSOCKET_HOST, WEBSOCKET_PORT, WEBSOCKET_PATH);
#else
        Serial.println(F("[NET] ERROR: WEBSOCKET_CA_CERT is required"));
        return false;
#endif
    }
    socket.onEvent(onSocketEvent);
    socket.setReconnectInterval(WIFI_RECONNECT_INTERVAL_MS);
    return true;
#else
    return false;
#endif
}

void ws_maintain() {
#ifdef ARDUINO
    const uint32_t now = millis();
    const bool wifiConnected = WiFi.status() == WL_CONNECTED;
    if (wifiConnected != lastWifiConnected) {
        lastWifiConnected = wifiConnected;
        Serial.println(wifiConnected ? F("[NET] Wi-Fi connected")
                                     : F("[NET] Wi-Fi disconnected"));
    }
    if (!wifiConnected) {
        connected = false;
        if (configured &&
            static_cast<uint32_t>(now - lastWifiAttempt) >=
                WIFI_RECONNECT_INTERVAL_MS) {
            lastWifiAttempt = now;
            WiFi.disconnect();
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
            Serial.println(F("[NET] Wi-Fi reconnect requested"));
        }
        return;
    }
    if (configured && !connected &&
        static_cast<uint32_t>(now - lastNetworkLog) >=
            NETWORK_STATUS_INTERVAL_MS) {
        lastNetworkLog = now;
        ws_connect();
    }
    if (configured) socket.loop();
#endif
}

void checkBackendContact() {
#ifdef ARDUINO
    if (!configured || !connected) return;
    const uint32_t now = millis();
    if (static_cast<uint32_t>(now - lastContact) < BACKEND_CONTACT_TIMEOUT_MS) return;
    Serial.println(F("[NET] backend contact watchdog expired; reconnecting"));
    connected = false;
    socket.disconnect();
    ws_connect();
#endif
}

void diagnoseConnectionError() {
#ifdef ARDUINO
    if (diagnosticsPrinted) return;
    diagnosticsPrinted = true;
    Serial.println(F("[NET] connection diagnostics:"));
    Serial.println(F("[NET] - verify Wi-Fi credentials and signal"));
    Serial.println(F("[NET] - verify WSS host, port, and path"));
    Serial.println(F("[NET] - verify WEBSOCKET_CA_CERT and device clock"));
    Serial.println(F("[NET] - verify firewall, DNS, and server availability"));
#endif
}

bool ws_pollOrder(Order out[], uint8_t& count) {
    count = 0;
    if (!out) return false;
    while (pendingCount && count < ORDER_QUEUE_CAPACITY) {
        out[count++] = pending[0];
        for (uint8_t i = 1; i < pendingCount; ++i) pending[i - 1] = pending[i];
        --pendingCount;
    }
    return count != 0;
}

bool ws_isConnected() { return connected; }
bool ws_isConfigured() { return configured; }
uint32_t ws_lastContact() { return lastContact; }

bool ws_publishDrinkOutcome(const char* orderId, uint8_t drinkIndex,
                            const char* status) {
#ifdef ARDUINO
    if (!connected || !orderId || !status) return false;
    StaticJsonDocument<256> document;
    document["orderId"] = orderId;
    document["drinkIndex"] = drinkIndex;
    document["status"] = status;
    char buf[512];
    const size_t length = serializeJson(document, buf, sizeof(buf));
    if (!length) return false;
    return socket.sendTXT(reinterpret_cast<const uint8_t*>(buf), length);
#else
    (void)orderId;
    (void)drinkIndex;
    (void)status;
    return false;
#endif
}

}  // namespace revov
