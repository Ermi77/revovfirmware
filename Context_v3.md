# RevoV System Context v3

This is the overall context for the RevoV firmware repository. The system is
split into an ESP32 controller, an Arduino Mega motion controller, and one
shared protocol header.

```text
esp/       ESP32 brain: orders, inventory, planning, WSS, and READY control
mega/      Mega 2560: physical motion primitives and limit monitoring
shared/    Protocol constants compiled by both projects
```

## 1. How the whole system works

The ESP32 receives an order from the 115200-baud serial console or from a
WebSocket Secure text frame. An order contains one or more triplets:

| Field | Range | Meaning |
| --- | --- | --- |
| `shelfId` | 1-5 | Drink type and source shelf |
| `quantity` | 1-255 | Number of drinks; conveyor rollover is automatic |
| `startSlot` | 0-9 | First X slot of the first drink only |

The ESP validates each triplet and places it in `OrderQueue`. The ESP state
machine starts one order, and `Planner` expands each drink into eight
primitive commands:

```text
(X, source slot)
(Y, shelfId - 1)
(Y, 6) PICK
(Y, 3) move to dispense shelf
(X, 0) move to dispense slot
(Y, 8) door open
(Y, 7) PLACE
(Y, 9) door close
```

The ESP sends only one primitive at a time. The Mega receives DIR/DATA,
executes it with non-blocking stepper and servo updates, and reports READY
only when the primitive is settled. The ESP then sends the next primitive.

After every drink, the ESP increments `nextSlot` for that shelf. When the
counter becomes greater than 9, it sends the shelf conveyor command
`(Y, conveyorCodeForShelf(shelfId))`, waits for READY, resets that shelf to
slot 0, and continues. This also happens after the final drink if that drink
used slot 9. Unused slots remain available to the next order.

The Mega performs homing in this order:

```text
Z retract -> X home -> Y home
```

Limit switches are currently declared as `PIN_UNUSED = 255`. Once real pins
are assigned, an active switch stops only its affected axis and logs the
switch name; it does not abort the order.

## 2. ESP32 responsibilities

The ESP32 project is [esp/](esp/). Its main loop:

1. Maintains Wi-Fi and WSS without blocking.
2. Polls validated network orders into `OrderQueue`.
3. Reads complete serial commands through `handleSerialInput()`.
4. Advances the state machine and READY timeout logic.
5. Prints changed-only status and periodic heap diagnostics.

The ESP uses `SIM_MODE` to exercise the complete order flow without a Mega,
actuators, or Wi-Fi. Simulation prints each command and generates a simulated
READY response.

WSS uses `WEBSOCKET_CA_CERT` in production. If the CA certificate is empty,
production connection is refused; only SIM_MODE permits the explicitly logged
insecure fallback. Every incoming frame refreshes the backend-contact timer.
Credential rejection is checked once per received object or array and logged
once per failure episode.

The ESP READY timeout is 30 seconds. A primitive timeout has a three-retry
budget; after exhaustion the ESP publishes a failed outcome and halts when
`HALT_ON_DRINK_FAILURE` is enabled.

## 3. Mega responsibilities

The Mega project is [mega/](mega/). It owns:

- X: one stepper along shelf slots.
- Y: two steppers in lockstep for shelf height.
- Z: servo, 0 degrees retracted and 90 degrees extended.
- Gripper: servo, 0 degrees open and 90 degrees closed by default.
- Door: servo, 0 degrees closed and 90 degrees open.
- Five conveyor steppers, one per shelf.

Stepper timing uses `micros()`. Servo sequencing uses `millis()`. The loop
contains no `delay()`. `Scheduler` updates motion, homing, switch monitoring,
READY handling, and the serial state machine every iteration.

The Mega accepts direct bench commands when `BENCH_MODE` is defined. `(Y,15)`
is interpreted as HOME ALL. The `status` and live `speed` commands are gated
by `BENCH_MODE`.

## 4. Serial commands

ESP:

```text
help
order <shelfId> <quantity> <startSlot>
order 5 2 4 ; 2 1 0 ; 3 3 7
status
run
step
reset
sim on
sim off
```

Mega bench console at 115200 baud:

```text
help
x <slot>
xf <steps>
xb <steps>
y <shelf>
yf <steps>
yb <steps>
z up
z down
grip close
grip open
pick
place
door open
door close
conv <1..5>
home
status
speed x|y|conv <hz>
stop
```

## 5. Shared protocol

Yes. [shared/RevoVProtocol.h](shared/RevoVProtocol.h) is included by both
projects through their PlatformIO include paths. It defines shelf and slot
counts, DIR values, PICK/PLACE/door action codes, conveyor codes, and the
machine credential constants used by ESP WSS validation.

`conveyorCodeForShelf(5)` returns 14. DATA 15 is reserved by the Mega command
interface for HOME ALL and is not a shelf-5 conveyor code.

## 6. Repository tree

The maintained source tree is listed in [FOLDER_TREE.txt](FOLDER_TREE.txt).

## 7. Items not yet written or not yet hardware-validated

- HMAC-SHA256 verification of WSS orders.
- Nonce replay protection.
- Duplicate `orderId`/idempotency enforcement.
- A finalized backend canonical JSON and signature schema.
- Serial PIN authentication for physical-console orders.
- Production secrets provisioning outside source configuration.
- Real WSS server integration testing.
- Assignment and electrical validation of the physical Mega pin map.
- Real endstop wiring and bench validation; all switch constants remain
  `PIN_UNUSED`.
- Driver-specific Vref/current programming; current fields are metadata.
- Calibration of step counts, speeds, acceleration, servo angles, and dwell.
- Full ESP-to-Mega hardware-in-the-loop testing.
- Mechanical obstruction and safety interlocks.
