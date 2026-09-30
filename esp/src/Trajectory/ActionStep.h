#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace revov {

struct ActionStep {
    uint8_t dir;
    uint8_t data;
    const char* label;
};

// TODO: implement (Milestone E1)
}  // namespace revov
