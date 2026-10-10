#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#include <WiFi.h>
#endif

#include "../../shared/RevoVProtocol.h"

namespace revov {

constexpr uint32_t READY_TIMEOUT_MS = 30000;
// Simulated Mega READY delay. The real manipulator settles in a variable
// window, so the simulation jitters inside this range instead of answering at
// a fixed offset; anything outside it means the timing model drifted.
constexpr uint32_t SIM_READY_DELAY_MIN_MS = 120;
constexpr uint32_t SIM_READY_DELAY_MAX_MS = 210;
static_assert(SIM_READY_DELAY_MAX_MS >= SIM_READY_DELAY_MIN_MS,
              "simulated READY window is inverted");

// SIM_MODE-only pick-routine phase codes. The shared
// protocol carries PICK as one atomic primitive; the
// simulation expands it into the manipulator's internal
// sequence — extend, grip, retract — so the operator
// gates each phase with the READY button. Values above
// the 4-bit protocol range never reach a pin.
constexpr uint8_t SIM_Z_EXTEND = 16;
constexpr uint8_t SIM_GRIP_CLOSE = 17;
constexpr uint8_t SIM_Z_RETRACT = 18;
constexpr uint8_t ORDER_QUEUE_CAPACITY = 8;
constexpr uint8_t CMD_MAX_LENGTH = 160;
constexpr uint16_t MAX_ORDER_QUANTITY = 255;

// HTTP listener for the live execution log page.
constexpr uint16_t WEB_UI_PORT = 80;

// WebSocket broadcast port for the event bus. The
// sim console page subscribes here to receive one
// JSON message per trace event.
constexpr uint16_t WEB_WS_PORT = 81;

// Soft AP used in SIM_MODE so the log page is reachable without backend
// credentials. Production builds use the station interface instead.
constexpr const char* SIM_AP_SSID = "RevoV-SIM";
constexpr const char* SIM_AP_PASSWORD = "revov-sim";

// TODO: verify ESP pin assignments against system context.
constexpr uint8_t PIN_ESP_DIR = 0;
constexpr uint8_t PIN_ESP_DATA0 = 0;
constexpr uint8_t PIN_ESP_DATA1 = 0;
constexpr uint8_t PIN_ESP_DATA2 = 0;
constexpr uint8_t PIN_ESP_DATA3 = 0;
constexpr uint8_t PIN_ESP_STROBE = 0;
constexpr uint8_t PIN_ESP_READY = 0;
constexpr uint8_t PIN_LED = 2;

// Station credentials. The sim build joins this
// network alongside its own access point (AP+STA),
// so the board also has internet and is reachable
// from the LAN; the non-sim build uses it as the
// primary link.
constexpr const char* WIFI_SSID = "Ab";
constexpr const char* WIFI_PASSWORD = "Ab@12345";

// TODO: configure production network values in secrets.h.
constexpr const char* WEBSOCKET_HOST = "";
constexpr uint16_t WEBSOCKET_PORT = 443;
constexpr const char* WEBSOCKET_PATH = "/";
constexpr const char* WEBSOCKET_CA_CERT = "";
constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 2000;
// Station keep-alive owned by main.cpp: retry interval when an
// SSID is configured but the link is down.
constexpr uint32_t WIFI_KEEPALIVE_INTERVAL_MS = 5000;
constexpr uint32_t NETWORK_STATUS_INTERVAL_MS = 30000;
// Serial status summary cap: a drink flaps the state twice
// per step and every serial line blocks loop() ~8 ms at
// 115200, so the summary is capped while the live log page
// still receives every event over the WebSocket bus.
constexpr uint32_t STATUS_LOG_INTERVAL_MS = 500;
constexpr uint32_t BACKEND_CONTACT_TIMEOUT_MS = 900000;
constexpr uint32_t HEAP_LOG_INTERVAL_MS = 300000;
constexpr uint32_t ORDER_TIMEOUT_MS = 300000;
constexpr uint8_t DRINK_RETRY_LIMIT = 3;
constexpr bool HALT_ON_DRINK_FAILURE = true;

}  // namespace revov
