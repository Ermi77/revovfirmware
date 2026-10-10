#include "Ui.h"

#include "../Config.h"
#include "../Core/EventBus.h"
#include "../Core/StateMachine.h"
#include "../Core/StateTracker.h"
#include "../Protocol/PinDriver.h"

#ifdef ARDUINO
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <stdio.h>
#include <string.h>
#endif

#include "Pages.h"
#include "PagesGz.h"
#include "Brand.h"

namespace revov {
namespace {

#ifdef ARDUINO
bool serving = false;

// Constructed on first use inside webui_init(), after Wi-Fi
// bring-up, so no static initializer touches the network stack.
WebServer& httpServer() {
    static WebServer instance(static_cast<int>(WEB_UI_PORT));
    return instance;
}

WebSocketsServer& wsServer() {
    static WebSocketsServer instance(static_cast<uint16_t>(WEB_WS_PORT));
    return instance;
}

// EventBus fan-out. Every event the state machine emits is
// already formatted on the server — the line for the serial
// monitor and the JSON message for the console — so the sink
// only hands the message to every connected client. The bus
// never knows who is listening.
void eventSink(const char* json) {
    if (wsServer().connectedClients() > 0) {
        wsServer().broadcastTXT(json);
    }
}

// ---- outbound replies -------------------------------------------

void sendTo(uint8_t num, const char* json, size_t length) {
    if (length) wsServer().sendTXT(num, json, length);
}

void sendAck(uint8_t num, const char* message) {
    StaticJsonDocument<160> document;
    document["type"] = "ack";
    document["message"] = message;
    char buffer[256];
    sendTo(num, buffer, serializeJson(document, buffer, sizeof(buffer)));
}

void sendError(uint8_t num, const char* message) {
    StaticJsonDocument<160> document;
    document["type"] = "error";
    document["message"] = message;
    char buffer[256];
    sendTo(num, buffer, serializeJson(document, buffer, sizeof(buffer)));
}

// ---- inbound commands -------------------------------------------

// Range-checks the decimal fields as longs first so an
// out-of-range value can never wrap through the uint8_t
// funnel parameters. The strings match the funnel's own, so
// the page shows the identical errors the serial console
// prints for the same triplet.
void submitOrderCommand(uint8_t num, JsonObjectConst root) {
    if (!root["shelfId"].is<int>() || !root["quantity"].is<int>() ||
        !root["startSlot"].is<int>()) {
        sendError(num, "order requires shelfId, quantity, and startSlot");
        return;
    }
    const long shelf = root["shelfId"].as<int>();
    const long quantity = root["quantity"].as<int>();
    const long startSlot = root["startSlot"].as<int>();
    const char* error = nullptr;
    if (shelf < 1 || shelf > SHELF_COUNT) {
        error = "shelfId must be 1..5";
    } else if (quantity < 1 || quantity > MAX_ORDER_QUANTITY) {
        error = "quantity must be 1..255";
    } else if (startSlot < SLOT_FIRST || startSlot > SLOT_LAST) {
        error = "startSlot must be 1..10";
    } else {
        error = sm_submitOrder(static_cast<uint8_t>(shelf),
                               static_cast<uint8_t>(quantity),
                               static_cast<uint8_t>(startSlot), "web");
    }
    if (error) {
        sendError(num, error);
    } else {
        char line[96];
        snprintf(line, sizeof(line),
                 "Instruction from ESP: ORDER shelf=%u "
                 "quantity=%u startSlot=%u",
                 static_cast<unsigned>(shelf),
                 static_cast<unsigned>(quantity),
                 static_cast<unsigned>(startSlot));
        eb_log(line);
        sendAck(num, "OK order groups=1");
    }
}

// A trajectory is a single drink through the identical intake
// funnel, logged identically, tagged web-traj.
void submitTrajectoryCommand(uint8_t num, JsonObjectConst root) {
    if (!root["shelfId"].is<int>() || !root["slot"].is<int>()) {
        sendError(num, "trajectory requires shelfId and slot");
        return;
    }
    const long shelf = root["shelfId"].as<int>();
    const long slot = root["slot"].as<int>();
    const char* error = nullptr;
    if (shelf < 1 || shelf > SHELF_COUNT) {
        error = "shelfId must be 1..5";
    } else if (slot < SLOT_FIRST || slot > SLOT_LAST) {
        error = "startSlot must be 1..10";
    } else {
        error = sm_submitOrder(static_cast<uint8_t>(shelf), 1,
                               static_cast<uint8_t>(slot), "web-traj");
    }
    if (error) sendError(num, error);
    else sendAck(num, "OK order groups=1");
}

// Manual axis move from the control panel. The panel sends
// an absolute target plus the travel direction; the machine
// executes the primitive through the same READY handshake
// as a planned step. The X panel carries a single row
// target; the Y panel carries the shelf level.
void axisCommand(uint8_t num, JsonObjectConst root) {
    const char* axis = root["axis"] | "";
    const bool isX = strcmp(axis, "x") == 0;
    const bool isY = strcmp(axis, "y") == 0;
    if (!isX && !isY) {
        sendError(num, "axis expects x or y");
        return;
    }
    if (!root["dir"].is<int>()) {
        sendError(num, "axis requires dir");
        return;
    }
    const long dir = root["dir"].as<int>();
    if (dir != -1 && dir != 1) {
        sendError(num, "dir must be -1 or 1");
        return;
    }
    long target = -1;
    long row = -1;
    if (isX) {
        if (!root["row"].is<int>()) {
            sendError(num, "x axis requires row");
            return;
        }
        row = root["row"].as<int>();
        target = row;
        // The X row carries a slot index: 0 is the
        // dispensing area and 1..SLOTS_PER_SHELF
        // are the pickup slots.
        if (target < 0 || target > SLOT_LAST) {
            sendError(num, "row must be 0..10");
            return;
        }
    } else {
        if (!root["position"].is<int>()) {
            sendError(num, "y axis requires position");
            return;
        }
        target = root["position"].as<int>();
        if (target < SHELF_FIRST || target > SHELF_LAST) {
            sendError(num, "position must be 1..5");
            return;
        }
    }
    const char* error = sm_manualStep(isX ? DIR_X : DIR_Y,
                                      static_cast<uint8_t>(target));
    if (error) {
        sendError(num, error);
        return;
    }
    char line[96];
    char ack[64];
    if (isX) {
        snprintf(line, sizeof(line),
                 "Instruction from ESP: X AXIS %s to row %u",
                 dir > 0 ? "FORWARD" : "BACKWARD",
                 static_cast<unsigned>(target));
        snprintf(ack, sizeof(ack), "OK x axis %s dispatched",
                 dir > 0 ? "forward" : "backward");
    } else {
        snprintf(line, sizeof(line),
                 "Instruction from ESP: Y AXIS %s to level %u",
                 dir > 0 ? "UP" : "DOWN",
                 static_cast<unsigned>(target));
        snprintf(ack, sizeof(ack), "OK y axis %s dispatched",
                 dir > 0 ? "up" : "down");
    }
    eb_log(line);
    sendAck(num, ack);
}

// Z-axis direct action: PICK or PLACE, dispatched as a
// single primitive outside an order.
void zCommand(uint8_t num, JsonObjectConst root) {
    const char* action = root["action"] | "";
    uint8_t data;
    const char* name;
    if (strcmp(action, "pick") == 0) {
        data = ACTION_PICK;
        name = "PICK";
    } else if (strcmp(action, "place") == 0) {
        data = ACTION_PLACE;
        name = "PLACE";
    } else {
        sendError(num, "z expects pick or place");
        return;
    }
    // In simulation the pick routine expands the atomic
    // primitive into extend, pick, retract; every phase
    // logs its own instruction as it dispatches.
    // Production keeps the single atomic primitive.
    const bool routine = strcmp(action, "pick") == 0 && pd_isSimulation();
    const char* error = routine ? sm_startPickRoutine()
                                : sm_manualStep(DIR_Y, data);
    if (error) {
        sendError(num, error);
        return;
    }
    if (!routine) {
        char line[64];
        snprintf(line, sizeof(line),
                 "Instruction from ESP: Z AXIS %s", name);
        eb_log(line);
    }
    char ack[32];
    snprintf(ack, sizeof(ack), "OK z %s dispatched", name);
    sendAck(num, ack);
}

// Door control: open or close, dispatched as a single
// primitive outside an order.
void doorCommand(uint8_t num, JsonObjectConst root) {
    JsonVariantConst openValue = root["open"];
    if (openValue.isNull()) {
        sendError(num, "door expects open");
        return;
    }
    const bool open = openValue.as<bool>();
    // Door status verification: the tracked door state is
    // checked before the instruction is sent, so a
    // redundant open or close never reaches the machine.
    const bool doorIsOpen = strcmp(st_doorName(), "open") == 0;
    if (open == doorIsOpen) {
        sendError(num, open ? "door already open" : "door already closed");
        return;
    }
    const char* error = sm_manualStep(
        DIR_Y, open ? ACTION_DOOR_OPEN : ACTION_DOOR_CLOSE);
    if (error) {
        sendError(num, error);
        return;
    }
    eb_log(open ? "Instruction from ESP: DOOR OPEN"
                : "Instruction from ESP: DOOR CLOSE");
    sendAck(num, open ? "OK door open dispatched"
                      : "OK door close dispatched");
}

void setLedCommand(uint8_t num, JsonObjectConst root) {
    JsonVariantConst onValue = root["on"];
    if (!onValue.isNull()) {
        const bool on = onValue.as<bool>();
        st_setLedMode(on ? LedMode::ON : LedMode::OFF);
        eb_log(on ? "Instruction from ESP: LED ON"
                  : "Instruction from ESP: LED OFF");
        sendAck(num, on ? "OK led on" : "OK led off");
    } else {
        sendError(num, "led expects on");
    }
}

void handleCommand(uint8_t num, uint8_t* payload, size_t length) {
    char text[256];
    const size_t copy = length < sizeof(text) - 1 ? length : sizeof(text) - 1;
    memcpy(text, payload, copy);
    text[copy] = '\0';
    StaticJsonDocument<256> document;
    if (deserializeJson(document, text)) {
        sendError(num, "invalid JSON");
        return;
    }
    JsonObjectConst root = document.as<JsonObjectConst>();
    const char* type = root["type"] | "";
    if (strcmp(type, "order") == 0) {
        submitOrderCommand(num, root);
    } else if (strcmp(type, "trajectory") == 0) {
        submitTrajectoryCommand(num, root);
    } else if (strcmp(type, "axis") == 0) {
        axisCommand(num, root);
    } else if (strcmp(type, "z") == 0) {
        zCommand(num, root);
    } else if (strcmp(type, "door") == 0) {
        doorCommand(num, root);
    } else if (strcmp(type, "led") == 0) {
        setLedCommand(num, root);
    } else if (strcmp(type, "ready") == 0) {
        // Mega Ready Signal: the operator presses READY to
        // release the primitive the machine is holding. In
        // simulation this arms the READY; the signal is
        // logged either way.
        eb_log("Mega Ready Signal Sent to ESP");
        sm_signalReady();
        sendAck(num, "OK mega ready signal");
    } else if (strcmp(type, "reset") == 0) {
        // Same path as the serial RESET command.
        eb_log("Instruction from ESP: RESET");
        sm_reset();
        sendAck(num, "OK reset");
    } else {
        sendError(num, "unknown command type");
    }
}

// ---- WebSocket wiring -------------------------------------------

void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    if (type == WStype_CONNECTED) {
        // Push the current state snapshot through the bus so
        // the fresh client renders the whole machine at once;
        // the fan-out sink reaches every connected client.
        eb_pushState();
    } else if (type == WStype_TEXT) {
        handleCommand(num, payload, length);
    }
}

void sendIndex() {
    Serial.println(F("[WEB] sim page request"));
    // The page is pre-gzipped at build time. A phone joined
    // to the sim AP has no internet, so it keeps its radio in
    // power-save and every TCP burst waits for a wake-up
    // (100-300 ms); sending 3.5x fewer bytes keeps the load
    // under a second. The cache header makes reloads instant.
    httpServer().sendHeader("Content-Encoding", "gzip");
    httpServer().sendHeader("Cache-Control", "max-age=86400");
    httpServer().send_P(200, "text/html; charset=utf-8",
                        (PGM_P)PAGE_GZ, PAGE_GZ_LEN);
}

// Brand imagery is embedded in firmware (Brand.h) so the
// page renders fully on devices joined to the sim AP,
// which has no internet access.
void sendBrand(const unsigned char* data, unsigned int length) {
    httpServer().sendHeader("Cache-Control", "max-age=86400");
    httpServer().send_P(200, "image/jpeg", (PGM_P)data, length);
}
#endif

}  // namespace

void webui_init() {
#ifdef ARDUINO
    WebServer& server = httpServer();
    server.on("/", sendIndex);
    // The browser probes /favicon.ico right after the page;
    // answering 204 saves a second power-save round trip (the
    // page also carries a data: icon link, so clients that
    // honor it never ask).
    server.on("/favicon.ico", HTTP_GET, []() { httpServer().send(204); });
    server.on("/brand/company.jpg", HTTP_GET,
              []() { sendBrand(BRAND_COMPANY, BRAND_COMPANY_LEN); });
    server.on("/brand/product.jpg", HTTP_GET,
              []() { sendBrand(BRAND_PRODUCT, BRAND_PRODUCT_LEN); });
    server.on("/brand/machine.jpg", HTTP_GET,
              []() { sendBrand(BRAND_MACHINE, BRAND_MACHINE_LEN); });
    // Anything else is a 404. Answering every path with
    // the page makes phones treat RevoV-SIM as a captive
    // portal: their /generate_204 and /hotspot-detect.html
    // probes would get 200 + HTML, popping "sign in"
    // sheets. "/" is the only page route.
    server.onNotFound([]() { httpServer().send(404); });
    server.begin();
    wsServer().onEvent(onWsEvent);
    wsServer().begin();
    eb_registerSink(eventSink);
    serving = true;
    Serial.print(F("[BOOT] page bytes="));
    Serial.println(sizeof(INDEX_PAGE));
    Serial.print(F("[BOOT] page gzip bytes="));
    Serial.println(PAGE_GZ_LEN);
#endif
}

void webui_service() {
#ifdef ARDUINO
    if (!serving) return;
    httpServer().handleClient();
    wsServer().loop();
#endif
}

bool webui_isServing() {
#ifdef ARDUINO
    return serving;
#else
    return false;
#endif
}

}  // namespace revov
