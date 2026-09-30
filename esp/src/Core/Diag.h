#pragma once

#include <Arduino.h>
#include <string.h>

namespace revov {

inline void printMasked(const char* label, const char* value) {
    Serial.print(label);
    Serial.print('=');
    if (!value || !*value) {
        Serial.println(F("<empty>"));
        return;
    }
    const size_t length = strlen(value);
    if (length <= 3) {
        Serial.println(F("***"));
        return;
    }
    Serial.write(value, 2);
    for (size_t i = 2; i + 1 < length; ++i) Serial.print('*');
    Serial.println(value[length - 1]);
}

}  // namespace revov
