#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <stdint.h>

namespace revov {

// One planned primitive. The planner builds a fixed
// eight-entry plan per drink; the smart planner flags
// entries that are no-ops at the tracked position so
// the dispatcher can drop them without renumbering the
// plan — the trace keeps stable step positions and the
// effective count is known before the first dispatch.
struct Cmd {
    char axis;          // 'X' or 'Y'
    int8_t data;        // target slot/shelf, or action code
    const char* label;
    bool skip;          // smart planner: no-op at tracked position
};

}  // namespace revov
