#include "Gsm.h"

#include "../Config.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <HardwareSerial.h>
#endif

namespace revov {
namespace {

GsmStatus gsmStatus;
HardwareSerial* gsmSerial = nullptr;

constexpr uint32_t BAUD_RATE = 115200;
constexpr uint32_t INIT_TIMEOUT_MS = 30000;
constexpr uint32_t CMD_TIMEOUT_MS = 5000;
constexpr uint32_t STATUS_UPDATE_INTERVAL_MS = 30000;

enum class InitPhase : uint8_t { NONE, POWER_ON, WAIT_READY, AT_TEST, DISABLE_ECHO, CHECK_SIM, CHECK_NETWORK, GET_OPERATOR, GET_SIGNAL, READY };
InitPhase initPhase = InitPhase::NONE;
uint32_t phaseStartTime = 0;
char rxBuffer[256];
size_t rxLength = 0;
bool waitingForResponse = false;
char expectedResponse[32] = "";
uint32_t cmdStartTime = 0;
uint32_t lastStatusUpdate = 0;

bool sendAtCommand(const char* cmd, const char* expected, uint32_t timeoutMs = CMD_TIMEOUT_MS);
void processGsmResponse();
void updateGsmStatus();
void setError(const char* msg);
void clearError();

}  // namespace

void gsm_init() {
#ifdef ARDUINO
    gsmSerial = new HardwareSerial(2);
    gsmSerial->begin(BAUD_RATE, SERIAL_8N1, PIN_GSM_RX, PIN_GSM_TX);
    pinMode(PIN_GSM_PWRKEY, OUTPUT);
    pinMode(PIN_GSM_RST, OUTPUT);
    pinMode(PIN_GSM_DTR, OUTPUT);
    digitalWrite(PIN_GSM_PWRKEY, HIGH);
    digitalWrite(PIN_GSM_RST, HIGH);
    digitalWrite(PIN_GSM_DTR, LOW);
#endif
    gsmStatus.state = GsmState::OFF;
    initPhase = InitPhase::NONE;
    clearError();
}

void gsm_powerOn() {
    if (gsmStatus.state != GsmState::OFF) return;
#ifdef ARDUINO
    digitalWrite(PIN_GSM_PWRKEY, LOW);
    delay(1000);
    digitalWrite(PIN_GSM_PWRKEY, HIGH);
    delay(2000);
    digitalWrite(PIN_GSM_RST, LOW);
    delay(100);
    digitalWrite(PIN_GSM_RST, HIGH);
#endif
    gsmStatus.state = GsmState::INITIALIZING;
    initPhase = InitPhase::WAIT_READY;
    phaseStartTime = millis();
    clearError();
}

void gsm_powerOff() {
#ifdef ARDUINO
    if (gsmSerial) {
        gsmSerial->print("AT+CPOWD=1\r\n");
        delay(1000);
    }
    digitalWrite(PIN_GSM_PWRKEY, LOW);
    delay(1500);
    digitalWrite(PIN_GSM_PWRKEY, HIGH);
#endif
    gsmStatus.state = GsmState::OFF;
    gsmStatus.networkRegistered = false;
    gsmStatus.signalQuality = 0;
    initPhase = InitPhase::NONE;
}

void gsm_sleep() {
    if (gsmStatus.state != GsmState::READY) return;
#ifdef ARDUINO
    digitalWrite(PIN_GSM_DTR, HIGH);
    gsmSerial->print("AT+CSCLK=1\r\n");
#endif
    gsmStatus.state = GsmState::SLEEPING;
}

void gsm_wake() {
    if (gsmStatus.state != GsmState::SLEEPING) return;
#ifdef ARDUINO
    digitalWrite(PIN_GSM_DTR, LOW);
    delay(50);
    gsmSerial->print("AT\r\n");
#endif
    gsmStatus.state = GsmState::READY;
}

void gsm_update() {
    uint32_t now = millis();

#ifdef ARDUINO
    while (gsmSerial && gsmSerial->available()) {
        char c = gsmSerial->read();
        if (rxLength < sizeof(rxBuffer) - 1) {
            rxBuffer[rxLength++] = c;
        }
        if (c == '\n') {
            rxBuffer[rxLength] = '\0';
            processGsmResponse();
            rxLength = 0;
        }
    }
#endif

    if (gsmStatus.state == GsmState::INITIALIZING) {
        if (static_cast<uint32_t>(now - phaseStartTime) > INIT_TIMEOUT_MS) {
            setError("GSM init timeout");
            gsmStatus.state = GsmState::ERROR;
            return;
        }

        switch (initPhase) {
            case InitPhase::WAIT_READY:
                if (static_cast<uint32_t>(now - phaseStartTime) > 5000) {
                    initPhase = InitPhase::AT_TEST;
                    phaseStartTime = now;
                    sendAtCommand("AT", "OK");
                }
                break;
            case InitPhase::AT_TEST:
                if (!waitingForResponse) {
                    initPhase = InitPhase::DISABLE_ECHO;
                    sendAtCommand("ATE0", "OK");
                }
                break;
            case InitPhase::DISABLE_ECHO:
                if (!waitingForResponse) {
                    initPhase = InitPhase::CHECK_SIM;
                    sendAtCommand("AT+CPIN?", "READY");
                }
                break;
            case InitPhase::CHECK_SIM:
                if (!waitingForResponse) {
                    initPhase = InitPhase::CHECK_NETWORK;
                    sendAtCommand("AT+CREG?", "+CREG: 0,1");
                }
                break;
            case InitPhase::CHECK_NETWORK:
                if (!waitingForResponse) {
                    if (strstr(rxBuffer, "+CREG: 0,1") || strstr(rxBuffer, "+CREG: 0,5")) {
                        gsmStatus.networkRegistered = true;
                        initPhase = InitPhase::GET_OPERATOR;
                        sendAtCommand("AT+COPS?", "+COPS:");
                    } else {
                        phaseStartTime = now;
                        sendAtCommand("AT+CREG?", "+CREG: 0,1");
                    }
                }
                break;
            case InitPhase::GET_OPERATOR:
                if (!waitingForResponse) {
                    initPhase = InitPhase::GET_SIGNAL;
                    sendAtCommand("AT+CSQ", "+CSQ:");
                }
                break;
            case InitPhase::GET_SIGNAL:
                if (!waitingForResponse) {
                    initPhase = InitPhase::READY;
                    gsmStatus.state = GsmState::READY;
                    clearError();
                }
                break;
            default:
                break;
        }
    }

    if (static_cast<uint32_t>(now - lastStatusUpdate) >= STATUS_UPDATE_INTERVAL_MS) {
        lastStatusUpdate = now;
        if (gsmStatus.state == GsmState::READY) {
            updateGsmStatus();
        }
    }
}

const GsmStatus& gsm_getStatus() {
    return gsmStatus;
}

bool gsm_isReady() {
    return gsmStatus.state == GsmState::READY && gsmStatus.networkRegistered;
}

bool gsm_sendSms(const char* phoneNumber, const char* message) {
    if (!gsm_isReady()) return false;
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"", phoneNumber);
    if (!sendAtCommand(cmd, ">")) return false;
#ifdef ARDUINO
    gsmSerial->print(message);
    gsmSerial->write(0x1A);  // Ctrl+Z
#endif
    return sendAtCommand("", "OK", 10000);
}

bool gsm_sendHttpGet(const char* url, char* response, size_t responseSize) {
    if (!gsm_isReady()) return false;
    if (!sendAtCommand("AT+HTTPINIT", "OK")) return false;
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"", url);
    if (!sendAtCommand(cmd, "OK")) return false;
    if (!sendAtCommand("AT+HTTPACTION=0", "OK", 10000)) return false;
    if (!sendAtCommand("AT+HTTPREAD", "+HTTPREAD:")) return false;
    if (response && responseSize > 0) {
        strncpy(response, rxBuffer, responseSize - 1);
        response[responseSize - 1] = '\0';
    }
    sendAtCommand("AT+HTTPTERM", "OK");
    return true;
}

bool gsm_sendHttpPost(const char* url, const char* contentType, const char* body, char* response, size_t responseSize) {
    if (!gsm_isReady()) return false;
    if (!sendAtCommand("AT+HTTPINIT", "OK")) return false;
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"", url);
    if (!sendAtCommand(cmd, "OK")) return false;
    snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"CONTENT\",\"%s\"", contentType);
    if (!sendAtCommand(cmd, "OK")) return false;
    snprintf(cmd, sizeof(cmd), "AT+HTTPDATA=%d,5000", strlen(body));
    if (!sendAtCommand(cmd, "DOWNLOAD")) return false;
#ifdef ARDUINO
    gsmSerial->print(body);
#endif
    if (!sendAtCommand("AT+HTTPACTION=1", "OK", 10000)) return false;
    if (!sendAtCommand("AT+HTTPREAD", "+HTTPREAD:")) return false;
    if (response && responseSize > 0) {
        strncpy(response, rxBuffer, responseSize - 1);
        response[responseSize - 1] = '\0';
    }
    sendAtCommand("AT+HTTPTERM", "OK");
    return true;
}

void gsm_sendLowVoltageAlert(float voltage) {
    char msg[160];
    snprintf(msg, sizeof(msg),
        "ALERT: RevoV battery critically low: %.1fV. Please charge or restore power. Machine paused.",
        voltage);
    const char* adminPhone = "+1XXXXXXXXXX";  // Configure in production
    gsm_sendSms(adminPhone, msg);
}

void gsm_sendTemperatureAlert(float tempC) {
    char msg[160];
    snprintf(msg, sizeof(msg),
        "ALERT: RevoV internal temperature high: %.1fC. Check cooling.",
        tempC);
    const char* adminPhone = "+1XXXXXXXXXX";  // Configure in production
    gsm_sendSms(adminPhone, msg);
}

namespace {

bool sendAtCommand(const char* cmd, const char* expected, uint32_t timeoutMs) {
    if (!gsmSerial) return false;
    waitingForResponse = true;
    strncpy(expectedResponse, expected, sizeof(expectedResponse) - 1);
    expectedResponse[sizeof(expectedResponse) - 1] = '\0';
    cmdStartTime = millis();
    gsmSerial->print(cmd);
    gsmSerial->print("\r\n");
    return true;
}

void processGsmResponse() {
    if (!waitingForResponse) return;

    if (strstr(rxBuffer, expectedResponse) || strstr(rxBuffer, "OK") || strstr(rxBuffer, "ERROR")) {
        waitingForResponse = false;

        if (strstr(rxBuffer, "+CSQ:")) {
            int rssi = 0, ber = 0;
            sscanf(rxBuffer, "+CSQ: %d,%d", &rssi, &ber);
            gsmStatus.signalQuality = rssi;
        }
        if (strstr(rxBuffer, "+COPS:")) {
            char* start = strstr(rxBuffer, "\"");
            if (start) {
                start++;
                char* end = strstr(start, "\"");
                if (end) {
                    size_t len = end - start;
                    if (len < sizeof(gsmStatus.operatorName)) {
                        strncpy(gsmStatus.operatorName, start, len);
                        gsmStatus.operatorName[len] = '\0';
                    }
                }
            }
        }
        if (strstr(rxBuffer, "+CIFSR:")) {
            char* start = strstr(rxBuffer, "+CIFSR:");
            if (start) {
                start += 7;
                while (*start == ' ' || *start == '"') start++;
                char* end = start;
                while (*end && *end != '"' && *end != '\r' && *end != '\n') end++;
                size_t len = end - start;
                if (len < sizeof(gsmStatus.ipAddress)) {
                    strncpy(gsmStatus.ipAddress, start, len);
                    gsmStatus.ipAddress[len] = '\0';
                }
            }
        }
    }
}

void updateGsmStatus() {
    sendAtCommand("AT+CSQ", "+CSQ:");
    sendAtCommand("AT+COPS?", "+COPS:");
}

void setError(const char* msg) {
    gsmStatus.hasError = true;
    strncpy(gsmStatus.errorMessage, msg, sizeof(gsmStatus.errorMessage) - 1);
    gsmStatus.errorMessage[sizeof(gsmStatus.errorMessage) - 1] = '\0';
}

void clearError() {
    gsmStatus.hasError = false;
    gsmStatus.errorMessage[0] = '\0';
}

}  // namespace

}  // namespace revov