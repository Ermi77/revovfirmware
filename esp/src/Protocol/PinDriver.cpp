#include "PinDriver.h"

#include "../../shared/RevoVProtocol.h"
#include "../Config.h"
#include "ReadyHandler.h"

namespace revov {
namespace {
bool simulation = false;

const char* directionName(uint8_t dir) { return dir == DIR_X ? "X" : "Y"; }

}

void pd_init() {
#ifdef ARDUINO
    pinMode(PIN_ESP_DIR, OUTPUT);
    pinMode(PIN_ESP_DATA0, OUTPUT);
    pinMode(PIN_ESP_DATA1, OUTPUT);
    pinMode(PIN_ESP_DATA2, OUTPUT);
    pinMode(PIN_ESP_DATA3, OUTPUT);
    pinMode(PIN_ESP_STROBE, OUTPUT);
    pinMode(PIN_ESP_READY, INPUT);
#endif
    rh_setSimulation(simulation);
}

void pd_setSimulation(bool enabled) {
    simulation = enabled;
    rh_setSimulation(enabled);
}

bool pd_isSimulation() { return simulation; }

void pd_send(uint8_t dir, uint8_t data) {
#ifdef SIM_MODE
    if (simulation) {
        Serial.print(F("[SIM] -> Mega: DIR="));
        Serial.print(directionName(dir));
        Serial.print(F(" DATA="));
        Serial.print(data);
        Serial.print(F(" ("));
        if (dir == DIR_X) {
            Serial.print(F("move X to slot "));
            Serial.print(data);
        } else if (data < SHELF_COUNT) {
            Serial.print(F("move Y to shelf "));
            Serial.print(data);
        } else if (data == ACTION_PICK) {
            Serial.print(F("PICK"));
        } else if (data == ACTION_PLACE) {
            Serial.print(F("PLACE"));
        } else if (data == ACTION_DOOR_OPEN) {
            Serial.print(F("DOOR OPEN"));
        } else if (data == ACTION_DOOR_CLOSE) {
            Serial.print(F("DOOR CLOSE"));
        } else if (data >= ACTION_CONVEYOR_FIRST && data <= ACTION_CONVEYOR_LAST) {
            Serial.print(F("ADVANCE CONVEYOR"));
            Serial.print(F(" SHELF "));
            Serial.print(data - ACTION_CONVEYOR_FIRST + 1);
        } else {
            Serial.print(F("move Y"));
        }
        Serial.println(F(")"));
        rh_scheduleSimulation();
        return;
    }
#endif
#ifdef ARDUINO
    digitalWrite(PIN_ESP_DIR, dir != 0 ? HIGH : LOW);
    digitalWrite(PIN_ESP_DATA0, (data >> 0) & 1);
    digitalWrite(PIN_ESP_DATA1, (data >> 1) & 1);
    digitalWrite(PIN_ESP_DATA2, (data >> 2) & 1);
    digitalWrite(PIN_ESP_DATA3, (data >> 3) & 1);
    digitalWrite(PIN_ESP_STROBE, HIGH);
#else
    (void)dir;
    (void)data;
#endif
}

bool pd_waitReady(uint32_t timeoutMs) {
    const uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        rh_update();
        if (pd_readyReceived()) return true;
    }
    return false;
}

bool pd_readyReceived() {
    rh_update();
    return rh_received();
}

}  // namespace revov
