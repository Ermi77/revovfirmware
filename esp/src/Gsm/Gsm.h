#pragma once

#include <stdint.h>

namespace revov {

enum class GsmState : uint8_t { OFF, INITIALIZING, READY, ERROR, SLEEPING };

struct GsmStatus {
    GsmState state = GsmState::OFF;
    int signalQuality = 0;      // 0-31 (99 = unknown)
    bool networkRegistered = false;
    char operatorName[32] = "";
    char ipAddress[16] = "";
    uint32_t lastUpdateMs = 0;
    bool hasError = false;
    char errorMessage[64] = "";
};

void gsm_init();
void gsm_update();
void gsm_powerOn();
void gsm_powerOff();
void gsm_sleep();
void gsm_wake();
const GsmStatus& gsm_getStatus();
bool gsm_isReady();
bool gsm_sendSms(const char* phoneNumber, const char* message);
bool gsm_sendHttpGet(const char* url, char* response, size_t responseSize);
bool gsm_sendHttpPost(const char* url, const char* contentType, const char* body, char* response, size_t responseSize);
void gsm_sendLowVoltageAlert(float voltage);
void gsm_sendTemperatureAlert(float tempC);

}  // namespace revov