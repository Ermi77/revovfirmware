#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#include <WiFi.h>
#endif

#include "../../shared/RevoVProtocol.h"

namespace revov {

constexpr uint8_t DISPENSE_SHELF = 3;
constexpr uint8_t DISPENSE_SLOT = 0;
constexpr uint32_t READY_TIMEOUT_MS = 30000;
constexpr uint32_t SIM_READY_DELAY_MS = 120;
constexpr uint8_t ORDER_QUEUE_CAPACITY = 8;
constexpr uint8_t CMD_MAX_LENGTH = 160;
constexpr uint16_t MAX_ORDER_QUANTITY = 255;

// TODO: verify ESP pin assignments against system context.
constexpr uint8_t PIN_ESP_DIR = 0;
constexpr uint8_t PIN_ESP_DATA0 = 0;
constexpr uint8_t PIN_ESP_DATA1 = 0;
constexpr uint8_t PIN_ESP_DATA2 = 0;
constexpr uint8_t PIN_ESP_DATA3 = 0;
constexpr uint8_t PIN_ESP_STROBE = 0;
constexpr uint8_t PIN_ESP_READY = 0;

// TODO: Wi-Fi SSID/password placeholders (move to secrets.h)
constexpr const char* WIFI_SSID = "";
constexpr const char* WIFI_PASSWORD = "";

// TODO: configure production network values in secrets.h.
constexpr const char* WEBSOCKET_HOST = "";
constexpr uint16_t WEBSOCKET_PORT = 443;
constexpr const char* WEBSOCKET_PATH = "/";
constexpr const char* WEBSOCKET_CA_CERT = "";
constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 2000;
constexpr uint32_t NETWORK_STATUS_INTERVAL_MS = 30000;
constexpr uint32_t BACKEND_CONTACT_TIMEOUT_MS = 900000;
constexpr uint32_t HEAP_LOG_INTERVAL_MS = 300000;
constexpr uint32_t ORDER_TIMEOUT_MS = 300000;
constexpr uint8_t DRINK_RETRY_LIMIT = 3;
constexpr bool HALT_ON_DRINK_FAILURE = true;

}  // namespace revov
