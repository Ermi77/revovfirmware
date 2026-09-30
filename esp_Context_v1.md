# RevoV ESP32 Context v1

## Role

The ESP32 is the system brain. It accepts and validates orders, tracks shelf
inventory, plans motion primitives, sends one command at a time to the Mega,
and waits for READY before continuing. It never directly drives the motors or
servos.

## Order model

```cpp
struct Order {
    uint8_t shelfId;    // 1..5
    uint8_t quantity;   // 1..255
    uint8_t startSlot;  // 0..9
    char orderId[32];   // network acknowledgement identity
};
```

Serial input is 115200 baud:

```text
order 5 2 4
order 5 2 4 ; 2 1 0 ; 3 3 7
help
status
run
step
reset
sim on
sim off
```

The parser rejects invalid shelf IDs, quantities, and start slots. Quantity is
not capped by the ten slots in one row: the planner inserts conveyor advances
whenever the shelf slot counter passes 9.

## End-to-end runtime

`main.cpp` maintains WSS, polls network orders, handles serial lines, calls
`sm_update()`, prints changed-only status, and logs heap usage every five
minutes. `StateMachine.cpp` starts queued orders, dispatches planned
primitives, waits for READY, updates inventory, and completes orders.

One drink expands to:

```text
(X, slot)
(Y, shelfId - 1)
(Y, 6) PICK
(Y, 3)
(X, 0)
(Y, 8) DOOR OPEN
(Y, 7) PLACE
(Y, 9) DOOR CLOSE
```

For shelf 5, a row rollover emits `(Y,14)`. If the final drink ends at slot 9,
the rollover is emitted after that drink so the next order starts at slot 0.

## Network and reliability behavior

`Net/WebSocket.cpp` provides WSS connection management, reconnect throttling,
backend-contact watchdog support, JSON parsing, credential checks, and drink
outcome publishing. Credentials are checked once per incoming order object or
array. Invalid credentials produce one edge-triggered rejection message per
failure episode, and the guard resets after WSS connects successfully.

Production WSS requires `WEBSOCKET_CA_CERT`. An empty certificate is refused
outside SIM_MODE. JSON uses fixed-size buffers. Bluetooth is disabled during
ESP32 setup. The state machine uses a 30-second READY timeout, three retries
per drink, and a five-minute active-order timeout.

Successful and failed network drinks can publish:

```json
{"orderId":"...", "drinkIndex":1, "status":"ok"}
{"orderId":"...", "drinkIndex":1, "status":"failed"}
```

## Simulation

`SIM_MODE` replaces the Mega interface with a fake driver. It prints each
DIR/DATA command, waits for a simulated READY interval, and then allows the
state machine to continue. This makes the complete order planner testable
without a Mega, actuators, or network service.

## ESP files

```text
esp/
├── platformio.ini
└── src/
    ├── main.cpp
    ├── Config.h
    ├── Net/WebSocket.h/.cpp
    ├── Order/OrderTypes.h
    ├── Order/OrderQueue.h/.cpp
    ├── Order/Inventory.h/.cpp
    ├── Trajectory/ActionStep.h
    ├── Trajectory/Planner.h/.cpp
    ├── Protocol/PinDriver.h/.cpp
    ├── Protocol/ReadyHandler.h/.cpp
    └── Core/StateMachine.h/.cpp, Scheduler.h/.cpp
```

## Not yet written or validated

- HMAC-SHA256 and nonce replay protection.
- Duplicate `orderId`/idempotency enforcement.
- Final backend signing schema and secrets provisioning.
- Serial PIN gate.
- Real WSS server integration test.
- Physical ESP-to-Mega test with READY wiring.
- Production validation of timeout and retry behavior under faults.
