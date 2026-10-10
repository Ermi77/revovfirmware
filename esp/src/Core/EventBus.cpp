#include "EventBus.h"

#include "StateMachine.h"
#include "StateTracker.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "../../shared/RevoVProtocol.h"

namespace revov {
namespace {

// Box-drawing rule (U+2550) framing the order-received
// and order-complete banners. Five lines of ten plus a
// final five: 55 characters wide.
const char ORDER_BANNER[] =
    "\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550"
    "\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550"
    "\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550"
    "\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550"
    "\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550"
    "\u2550\u2550\u2550";

// The banner is the longest rendered line at 55
// characters (165 UTF-8 bytes); the JSON envelope
// adds the type, the escaped line, and the
// structured fields of the block's first line.
char lineBuf[192];
char jsonBuf[320];
char escapedLine[200];
char fieldBuf[96];

EventSink sink = nullptr;

// Last state event pushed to the sink. The state
// event is change-only so a dashboard is not
// flooded with identical snapshots. The snapshot is
// assembled from the state tracker and the state
// machine, which own the values.
struct Snapshot {
    uint8_t x;
    uint8_t y;
    const char* z;
    const char* zAction;
    const char* door;
    const char* led;
    const char* ledMode;
    const char* machine;
};
Snapshot lastSnapshot{};
bool haveSnapshot = false;

// Renders the human label for a primitive. Movement
// steps carry their target; action steps are named
// constants.
void stepLabel(uint8_t dir, uint8_t data, char* out, size_t outSize) {
    if (dir == DIR_X) {
        snprintf(out, outSize, "Move X to slot %u",
                 static_cast<unsigned>(data));
    } else if (data >= SHELF_FIRST && data <= SHELF_LAST) {
        snprintf(out, outSize, "Move Y to shelf %u",
                 static_cast<unsigned>(data));
    } else if (data == ACTION_PICK) {
        snprintf(out, outSize, "PICK");
    } else if (data == ACTION_PLACE) {
        snprintf(out, outSize, "PLACE");
    } else if (data == ACTION_DOOR_OPEN) {
        snprintf(out, outSize, "DOOR OPEN");
    } else if (data == ACTION_DOOR_CLOSE) {
        snprintf(out, outSize, "DOOR CLOSE");
    } else if (data >= ACTION_CONVEYOR_FIRST &&
               data <= ACTION_CONVEYOR_LAST) {
        snprintf(out, outSize, "ADVANCE CONVEYOR SHELF %u",
                 static_cast<unsigned>(data - ACTION_CONVEYOR_FIRST + 1));
    } else {
        snprintf(out, outSize, "Move Y %u",
                 static_cast<unsigned>(data));
    }
}

// Copies src into out as a JSON string literal,
// escaping the characters that would break the
// enclosing message.
void jsonEscape(const char* src, char* out, size_t outSize) {
    size_t used = 0;
    if (outSize < 2) {
        if (outSize != 0) out[0] = '\0';
        return;
    }
    if (src == nullptr) src = "";
    out[used++] = '"';
    for (const char* p = src; *p != '\0' && used + 7 < outSize; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        switch (c) {
        case '"':
            out[used++] = '\\';
            out[used++] = '"';
            break;
        case '\\':
            out[used++] = '\\';
            out[used++] = '\\';
            break;
        case '\n':
            out[used++] = '\\';
            out[used++] = 'n';
            break;
        case '\r':
            out[used++] = '\\';
            out[used++] = 'r';
            break;
        case '\t':
            out[used++] = '\\';
            out[used++] = 't';
            break;
        default:
            if (c < 0x20) {
                used += static_cast<size_t>(
                    snprintf(out + used, outSize - used, "\\u%04x", c));
            } else {
                out[used++] = static_cast<char>(c);
            }
            break;
        }
    }
    out[used++] = '"';
    out[used] = '\0';
}

// Fan-out. The line is printed to the serial monitor
// once; the JSON message is handed to the registered
// sink. Events without a line skip the monitor.
void emitEvent(const char* type, const char* line,
               const char* fields) {
    if (line != nullptr) Serial.println(line);
    if (sink == nullptr) return;
    jsonEscape(line, escapedLine, sizeof(escapedLine));
    snprintf(jsonBuf, sizeof(jsonBuf),
             "{\"type\":\"%s\",\"line\":%s%s}",
             type, escapedLine, fields != nullptr ? fields : "");
    sink(jsonBuf);
}

// Assembles the state snapshot from the state
// tracker and the state machine, which own the
// values, and pushes it when anything moved.
void emitState() {
    if (sink == nullptr) return;
    const char* machine = sm_state() == State::ERROR
                              ? "HALTED"
                              : sm_hasActiveOrder()
                                    ? "DISPENSING"
                                    : "IDLE";
    lastSnapshot.x = st_x();
    lastSnapshot.y = st_y();
    lastSnapshot.z = st_zName();
    lastSnapshot.zAction = st_zActionName();
    lastSnapshot.door = st_doorName();
    lastSnapshot.led = st_ledColorName();
    lastSnapshot.ledMode = st_ledModeName();
    lastSnapshot.machine = machine;
    haveSnapshot = true;
    snprintf(jsonBuf, sizeof(jsonBuf),
             "{\"type\":\"state\",\"x\":%u,\"y\":%u,\"z\":\"%s\","
             "\"zAction\":\"%s\",\"door\":\"%s\",\"led\":\"%s\","
             "\"ledMode\":\"%s\",\"machine\":\"%s\"}",
             static_cast<unsigned>(st_x()),
             static_cast<unsigned>(st_y()),
             st_zName(), st_zActionName(), st_doorName(),
             st_ledColorName(), st_ledModeName(), machine);
    sink(jsonBuf);
}

}  // namespace

void eb_init() { haveSnapshot = false; }

void eb_registerSink(EventSink handler) { sink = handler; }

void eb_orderStart(uint8_t shelfId, uint8_t quantity,
                   uint8_t startSlot) {
    snprintf(lineBuf, sizeof(lineBuf), "%s", ORDER_BANNER);
    snprintf(fieldBuf, sizeof(fieldBuf),
             ",\"shelfId\":%u,\"quantity\":%u,\"startSlot\":%u",
             static_cast<unsigned>(shelfId),
             static_cast<unsigned>(quantity),
             static_cast<unsigned>(startSlot));
    emitEvent("order_start", lineBuf, fieldBuf);
}

void eb_drinkStart(uint8_t drink, uint8_t of, uint8_t slot) {
    snprintf(lineBuf, sizeof(lineBuf), "DRINK %u/%u  (slot %u)",
             static_cast<unsigned>(drink),
             static_cast<unsigned>(of),
             static_cast<unsigned>(slot));
    snprintf(fieldBuf, sizeof(fieldBuf),
             ",\"drink\":%u,\"of\":%u,\"slot\":%u",
             static_cast<unsigned>(drink),
             static_cast<unsigned>(of),
             static_cast<unsigned>(slot));
    emitEvent("drink_start", lineBuf, fieldBuf);
}

void eb_step(uint8_t step, uint8_t of, uint8_t dir, uint8_t data) {
    char label[48];
    stepLabel(dir, data, label, sizeof(label));
    if (of != 0) {
        snprintf(lineBuf, sizeof(lineBuf),
                 "ESP : [STEP %u/%u] DIR=%s DATA=%u  (%s)",
                 static_cast<unsigned>(step),
                 static_cast<unsigned>(of),
                 dir == DIR_X ? "X" : "Y",
                 static_cast<unsigned>(data), label);
        snprintf(fieldBuf, sizeof(fieldBuf),
                 ",\"step\":%u,\"of\":%u,\"dir\":\"%s\",\"data\":%u,"
                 "\"label\":\"%s\"",
                 static_cast<unsigned>(step),
                 static_cast<unsigned>(of),
                 dir == DIR_X ? "X" : "Y",
                 static_cast<unsigned>(data), label);
    } else {
        snprintf(lineBuf, sizeof(lineBuf),
                 "ESP : [STEP %u] DIR=%s DATA=%u  (%s)",
                 static_cast<unsigned>(step),
                 dir == DIR_X ? "X" : "Y",
                 static_cast<unsigned>(data), label);
        snprintf(fieldBuf, sizeof(fieldBuf),
                 ",\"step\":%u,\"dir\":\"%s\",\"data\":%u,"
                 "\"label\":\"%s\"",
                 static_cast<unsigned>(step),
                 dir == DIR_X ? "X" : "Y",
                 static_cast<unsigned>(data), label);
    }
    emitEvent("step", lineBuf, fieldBuf);
}

void eb_stepSkip(uint8_t step, uint8_t of, char axis,
                    int8_t data) {
    // The tracked position equals the target — that
    // is why the move was filtered out.
    char detail[48];
    if (axis == 'X') {
        snprintf(detail, sizeof(detail),
                 "already at X slot %d, target slot %d",
                 static_cast<int>(data), static_cast<int>(data));
    } else {
        snprintf(detail, sizeof(detail),
                 "already at Y shelf %d, target shelf %d",
                 static_cast<int>(data), static_cast<int>(data));
    }
    snprintf(lineBuf, sizeof(lineBuf),
             "ESP : [STEP %u/%u] SKIPPED  (%s)",
             static_cast<unsigned>(step),
             static_cast<unsigned>(of), detail);
    snprintf(fieldBuf, sizeof(fieldBuf),
             ",\"step\":%u,\"of\":%u,\"axis\":\"%c\","
             "\"data\":%d",
             static_cast<unsigned>(step),
             static_cast<unsigned>(of), axis,
             static_cast<int>(data));
    emitEvent("step_skip", lineBuf, fieldBuf);
}

void eb_ready(uint32_t deltaMs) {
    snprintf(lineBuf, sizeof(lineBuf),
             "  ← READY received (Δ=%lums)",
             static_cast<unsigned long>(deltaMs));
    emitEvent("ready", lineBuf, nullptr);
}

void eb_nextSlot(uint8_t shelf, uint8_t value) {
    snprintf(lineBuf, sizeof(lineBuf), "  → nextSlot[%u] = %u",
             static_cast<unsigned>(shelf),
             static_cast<unsigned>(value));
    snprintf(fieldBuf, sizeof(fieldBuf),
             ",\"shelf\":%u,\"value\":%u",
             static_cast<unsigned>(shelf),
             static_cast<unsigned>(value));
    emitEvent("next_slot", lineBuf, fieldBuf);
}

void eb_conveyorCheck(uint8_t value, uint8_t threshold,
                      bool advance) {
    snprintf(lineBuf, sizeof(lineBuf), "  → %u > %u? %s — %s",
             static_cast<unsigned>(value),
             static_cast<unsigned>(threshold),
             advance ? "YES" : "NO",
             advance ? "advance conveyor" : "skip conveyor");
    snprintf(fieldBuf, sizeof(fieldBuf),
             ",\"value\":%u,\"threshold\":%u,\"advance\":%s",
             static_cast<unsigned>(value),
             static_cast<unsigned>(threshold),
             advance ? "true" : "false");
    emitEvent("conveyor_check", lineBuf, fieldBuf);
}

void eb_drinkEnd() {
    snprintf(lineBuf, sizeof(lineBuf), "%s", "───...");
    emitEvent("drink_end", lineBuf, nullptr);
}

void eb_orderDone(uint8_t shelfId, uint8_t dispensed,
                  uint32_t conveyorAdvances, uint8_t finalNextSlot) {
    snprintf(lineBuf, sizeof(lineBuf), "%s", ORDER_BANNER);
    snprintf(fieldBuf, sizeof(fieldBuf),
             ",\"shelfId\":%u,\"dispensed\":%u,"
             "\"conveyorAdvances\":%u,\"finalNextSlot\":%u",
             static_cast<unsigned>(shelfId),
             static_cast<unsigned>(dispensed),
             static_cast<unsigned>(conveyorAdvances),
             static_cast<unsigned>(finalNextSlot));
    emitEvent("order_done", lineBuf, fieldBuf);
}

void eb_warn(uint8_t step, uint8_t of, uint8_t retry,
             uint8_t limit) {
    if (of != 0) {
        snprintf(lineBuf, sizeof(lineBuf),
                 "⚠ READY timeout on step %u/%u — retry %u/%u",
                 static_cast<unsigned>(step),
                 static_cast<unsigned>(of),
                 static_cast<unsigned>(retry),
                 static_cast<unsigned>(limit));
    } else {
        snprintf(lineBuf, sizeof(lineBuf),
                 "⚠ READY timeout on step %u — retry %u/%u",
                 static_cast<unsigned>(step),
                 static_cast<unsigned>(retry),
                 static_cast<unsigned>(limit));
    }
    emitEvent("warn", lineBuf, nullptr);
}

void eb_error(uint8_t retries, bool halted) {
    snprintf(lineBuf, sizeof(lineBuf),
             "✗ DRINK FAILED after %u retries — %s",
             static_cast<unsigned>(retries),
             halted ? "machine HALTED (send reset)"
                    : "skipping drink");
    emitEvent("error", lineBuf, nullptr);
}

void eb_log(const char* line) {
    snprintf(lineBuf, sizeof(lineBuf), "%s",
             line != nullptr ? line : "");
    emitEvent("log", lineBuf, nullptr);
}

void eb_drinkOk(const char* orderId, uint8_t drink) {
    if (sink == nullptr) return;
    char id[80];
    jsonEscape(orderId != nullptr ? orderId : "", id, sizeof(id));
    snprintf(jsonBuf, sizeof(jsonBuf),
             "{\"type\":\"drink_ok\",\"orderId\":%s,\"drink\":%u}",
             id, static_cast<unsigned>(drink));
    sink(jsonBuf);
}

void eb_state() {
    if (haveSnapshot &&
        lastSnapshot.x == st_x() &&
        lastSnapshot.y == st_y() &&
        lastSnapshot.z == st_zName() &&
        lastSnapshot.zAction == st_zActionName() &&
        lastSnapshot.door == st_doorName() &&
        lastSnapshot.led == st_ledColorName() &&
        lastSnapshot.ledMode == st_ledModeName() &&
        lastSnapshot.machine ==
            (sm_state() == State::ERROR
                 ? "HALTED"
                 : sm_hasActiveOrder() ? "DISPENSING" : "IDLE")) {
        return;
    }
    emitState();
}

void eb_pushState() { emitState(); }

void eb_stepLabel(uint8_t dir, uint8_t data, char* out,
                  size_t outSize) {
    stepLabel(dir, data, out, outSize);
}

}  // namespace revov
