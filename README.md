# RevoV Firmware

**RevoV vending machine firmware — ESP32 (brain) + Arduino Mega (motion slave).**

## TL;DR

| Board | Role | Talks to |
|---|---|---|
| **ESP32** (`esp/`) | Cloud brain — WebSocket, order queue, trajectory planning | DB via Wi-Fi · Mega via 5+1 wires |
| **Mega** (`mega/`) | Dumb motion slave — steppers, gripper, door, conveyors | ESP only |

**One contract:** `shared/RevoVProtocol.h` — the only file both boards compile.  
**Never change it on one side only.**

## System Overview

```
DB ──WebSocket──► ESP32 ──DIR+DATA+STROBE──► Mega ──► motors
              ◄──────READY──────────────
```

- ESP sends **one command at a time**, waits for **READY**, sends next.
- Mega never decides anything — just executes and pulses READY when settled.
- Order = array of triplets: `(shelfId 1–5, quantity 1–255, startSlot 1–10)`.
- 8 commands per drink (see `Context_v3.md §5`).
- After 10 slots → ESP sends conveyor-advance command.

## Repository Layout

```
revovfirmware/
├── README.md                      ← this file
├── platformio.ini                 ← root aggregator (esp + mega envs)
├── Context_v3.md                  ← system truth (ESP + Mega + DB)
├── Mega_Context_v1.md             ← Mega-side truth (pins, motors, states)
├── esp_Context_v1.md              ← ESP-side truth
├── FOLDER_TREE.txt                ← maintained source tree
├── SIM_GUIDE.md                   ← simulation mode guide
├── shared/
│   └── RevoVProtocol.h            ← compiled by BOTH boards
├── esp/                           ← PlatformIO project 1
│   ├── platformio.ini
│   └── src/
│       ├── main.cpp               ← entry point, boot, loop
│       ├── Config.h               ← ESP constants, pins, timeouts
│       ├── Net/
│       │   ├── WebSocket.h/.cpp   ← WSS client, JSON orders, reconnect
│       ├── Web/
│       │   ├── Ui.h/.cpp          ← HTTP + WS server (sim console)
│       │   ├── Pages.h            ← embedded HTML (gzipped)
│       │   ├── PagesGz.h          ← gzipped page bytes
│       │   └── Brand.h            ← embedded brand images
│       ├── Order/
│       │   ├── OrderTypes.h       ← Order, OrderItem structs
│       │   ├── OrderQueue.h/.cpp  ← ring buffer (capacity 8)
│       │   ├── Inventory.h/.cpp   ← nextSlot per shelf, conveyor rule
│       ├── Trajectory/
│       │   ├── ActionStep.h       ← Cmd struct (axis, data, label, skip)
│       │   ├── Planner.h/.cpp     ← 8-step plan + smart no-op filter
│       ├── Protocol/
│       │   ├── PinDriver.h/.cpp   ← DIR/DATA/STROBE out, READY in
│       │   ├── ReadyHandler.h/.cpp← 30 s timeout, sim jitter, stall
│       └── Core/
│           ├── StateMachine.h/.cpp← order FSM, dispatch, retry, halt
│           ├── Scheduler.h/.cpp   ← ws_maintain + rh_update
│           ├── EventBus.h/.cpp    ← trace + JSON fan-out to web UI
│           ├── StateTracker.h/.cpp← mirrored physical state (x,y,z,door,led)
│           └── Diag.h             ← diagnostics helpers
└── mega/                          ← PlatformIO project 2
    ├── platformio.ini
    └── src/
        ├── main.cpp               ← entry point, init all modules
        ├── Config.h               ← Mega pins, speeds, accel, servo angles
        ├── Protocol/
        │   ├── PinReader.h/.cpp   ← DIR/DATA/STROBE in, READY pulse out
        ├── Motion/
        │   ├── StepperAxis.h/.cpp ← accel/decel stepper (micros)
        │   ├── Axes.h/.cpp        ← X + dual Y, homing, limit switches
        │   ├── MultiStepper.h/.cpp← bench-mode AccelStepper x8
        │   ├── ServoController.h/.cpp← 3 servos, speed-limited (millis)
        ├── EndEffector/
        │   ├── Gripper.h/.cpp     ← Z + gripper sequence (pick/place)
        │   ├── Door.h/.cpp        ← door servo open/close
        ├── Shelf/
        │   ├── Conveyor.h/.cpp    ← 5× stepper, one per shelf
        ├── Homing/
        │   ├── Home.h/.cpp        ← Z retract → X home → Y home
        └── Core/
            ├── StateMachine.h/.cpp← serial bench console, READY handshake
            ├── Scheduler.h/.cpp   ← updates all motion + limits each loop
```

---

## ESP32 — Detailed Module Logic

### `esp/src/main.cpp` — Entry Point & Main Loop

**Boot sequence:**
1. Disables Bluetooth
2. Starts Wi-Fi (AP+STA in `SIM_MODE`, STA otherwise)
3. Initializes `StateMachine`, `EventBus`, `WebUI`, `WebSocket`
4. Calls `sm_init()`, `pd_init()`, `st_updateLed()`, `sched_init()`

**Loop (non-blocking, runs every iteration):**
1. `wifiKeepAlive()` — reconnects Wi-Fi / announces mDNS + sim page URL
2. `ws_maintain()` — WebSocket reconnect throttling, backend-contact watchdog
3. `checkBackendContact()` — tears down stale WSS after 15 min silence
4. `webui_service()` — serves sim console page + WS event bus
5. `ws_pollOrder()` — drains validated network orders into `OrderQueue`
6. `handleSerialInput()` — line-buffered serial commands → `sm_handleCommand()`
7. `sm_update()` — advances order FSM, dispatches primitives, handles READY/timeout/retry
8. Periodic status log (500 ms) + heap log (5 min)

**Key design:** Everything is non-blocking. No `delay()` calls. Uses `millis()` for all timing.

### `esp/src/Config.h` — ESP Constants

| Constant | Value | Purpose |
|---|---|---|
| `READY_TIMEOUT_MS` | 30000 | Max wait for Mega READY |
| `SIM_READY_DELAY_MIN/MAX_MS` | 120/210 | Jittered sim READY delay |
| `ORDER_QUEUE_CAPACITY` | 8 | Ring buffer size |
| `MAX_ORDER_QUANTITY` | 255 | Per-item drink limit |
| `WEB_UI_PORT` | 80 | HTTP sim console |
| `WEB_WS_PORT` | 81 | WS event broadcast |
| `SIM_AP_SSID/PASSWORD` | "RevoV-SIM"/"revov-sim" | SIM soft AP |
| `WIFI_SSID/PASSWORD` | "Ab"/"Ab@12345" | Station credentials |
| `WEBSOCKET_HOST/PORT/PATH/CA_CERT` | (empty) | Production WSS config |
| `WIFI_RECONNECT_INTERVAL_MS` | 2000 | Wi-Fi retry interval |
| `BACKEND_CONTACT_TIMEOUT_MS` | 900000 | 15 min WSS silence → reconnect |
| `ORDER_TIMEOUT_MS` | 300000 | 5 min active-order timeout |
| `DRINK_RETRY_LIMIT` | 3 | Max retries per drink |
| `HALT_ON_DRINK_FAILURE` | true | Halt machine on exhausted retries |

**Pins (ESP → Mega parallel 4-bit):**
- `PIN_ESP_DIR` = GPIO4 (Mega pin 47)
- `PIN_ESP_DATA0..3` = GPIO5,16,17,18 (Mega 48–51)
- `PIN_ESP_STROBE` = GPIO19 (Mega 52)
- `PIN_ESP_READY` = GPIO21 (Mega 53, input from Mega)
- `PIN_LED` = GPIO2 (onboard LED)

**Sensors:** `PIN_VOLTAGE_SENSOR`=34, `PIN_TEMP_SENSOR`=35 (ADC1)
**GSM:** TX=1, RX=3, PWRKEY=25, RST=26, DTR=27 (not yet integrated)

### `esp/src/Net/WebSocket.*` — WSS Client

**`ws_connect()`**: TLS with CA cert (required in production; `SIM_MODE` allows insecure fallback logged explicitly).

**`ws_maintain()`**: 
- Wi-Fi reconnect if down
- WSS reconnect with backoff
- Calls `socket.loop()` for incoming frames

**`onSocketEvent()`** — parses JSON frames:
- Validates `MACHINE_ID` / `MACHINE_TOKEN` (edge-triggered rejection logging — one message per failure episode)
- Accepts single order object, `orders` array, or bare array
- Supports **multi-item** format (`items[]` with `shelfId`/`quantity`/`startSlot`) and legacy single-item
- Enqueues validated orders into `pending[]` ring buffer

**`ws_pollOrder()`**: Moves `pending[]` → caller array for StateMachine.

**`ws_publishDrinkOutcome()`**: Sends `{"orderId","drinkIndex","status":"ok|failed"}`.

**Backend contact watchdog**: 15 min silence → forced reconnect.

### `esp/src/Web/Ui.*` — Sim Console (HTTP + WS)

**HTTP** (`WEB_UI_PORT = 80`): Serves gzipped `index.html` (embedded in `PagesGz.h`), brand images from `Brand.h`.

**WebSocket** (`WEB_WS_PORT = 81`): Broadcasts EventBus JSON to all connected clients.

**Inbound commands** (JSON `type`):
| Type | Payload | Action |
|---|---|---|
| `order` | `{shelfId,qty,startSlot}` | `sm_submitOrder()` |
| `trajectory` | `{shelfId,startSlot}` | `sm_submitOrder(qty=1)` |
| `axis` | `{dir:"X\|Y",data}` | `sm_manualStep()` |
| `z` | `{action:"pick\|place\|extend\|retract"}` | `sm_startPickRoutine()` (sim) or `sm_manualStep()` |
| `door` | `{action:"open\|close"}` | `sm_manualStep()` (guards redundant) |
| `led` | `{mode:"on\|off\|auto"}` | `st_setLedMode()` |
| `ready` | — | `sm_signalReady()` |
| `reset` | — | `sm_reset()` |

**Outbound**: Every EventBus line → serial + WS broadcast; state snapshot on connect.

### `esp/src/Order/OrderTypes.h` — Order Data Model

```cpp
struct OrderItem { uint8_t shelfId, quantity, startSlot; };

struct Order {
  // Legacy single-item fields
  uint8_t shelfId, quantity, startSlot;
  char orderId[32];
  // Multi-item
  OrderItem items[MAX_ORDER_ITEMS];  // 5
  uint8_t itemCount = 0;
  bool isMultiItem = false;
  // Runtime
  uint8_t currentItemIndex = 0;
  uint8_t currentItemDrinkIndex = 0;
};
```

**Helpers:**
- `orderIsMultiItem()`, `orderTotalDrinks()`, `orderCurrentItem()`
- `orderEffectiveShelf/Quantity/StartSlot()` — unified access
- `orderFromLegacy()`, `orderFromItems()` — builders
- `orderResetRuntime()` — clears runtime indices
- `orderAdvanceDrink()` — advances to next item/drink
- `orderIsComplete()` — checks completion

### `esp/src/Order/OrderQueue.*` — Ring Buffer (capacity 8)

- `oq_enqueue()` — legacy triplet enqueue
- `oq_enqueueOrder()` / `oq_submitOrder()` — full `Order` struct enqueue
- `oq_hasNext()`, `oq_peek()`, `oq_markDispensed()`, `oq_clear()`, `oq_size()`

### `esp/src/Order/Inventory.*` — Per-Shelf Slot Counter

- `nextSlots[5]` initialized to `SLOT_FIRST (1)`
- `inv_nextSlot(shelf)` — returns current counter (1..11)
- `inv_shouldAdvance(shelf)` — **`>`** comparison: `nextSlot > SLOTS_PER_SHELF (10)`
- `inv_setNextSlot(shelf, slot)` — allows transient `11` (after slot 10 dispensed)
- `inv_resetSlots(shelf)` — wraps to `1` after conveyor advance
- `inv_resetAll()` — full reset

**Logic:** Row serves slots 1–10. Counter reaches 11 transiently, then conveyor advances and resets to 1. Unused slots remain available to next order.

### `esp/src/Trajectory/ActionStep.h` — Planned Primitive

```cpp
struct Cmd {
  char axis;           // 'X' or 'Y'
  int8_t data;         // target slot/shelf, or action code
  const char* label;   // human readable
  bool skip;           // smart planner: no-op at tracked position
};
```

### `esp/src/Trajectory/Planner.*` — 8-Step Drink Plan + Smart Filter

**Fixed plan (per drink):**
```
0: (X, startSlot)           "move X to slot"
1: (Y, shelfId)             "move Y to shelf"
2: (Y, ACTION_PICK)         "PICK"
3: (Y, DISPENSE_SHELF=3)    "move Y to dispense shelf"
4: (X, DISPENSE_SLOT=0)     "move X to dispense slot"
5: (Y, ACTION_DOOR_OPEN)    "DOOR OPEN"
6: (Y, ACTION_PLACE)        "PLACE"
7: (Y, ACTION_DOOR_CLOSE)   "DOOR CLOSE"
```

**Legacy** `planOrder()` → emits all 8.

**Smart** `planOrderSmart()` (default `SMART_PLANNER=1`):
- Forward-simulates plan against tracked position (`pl_curXSlot()`, `pl_curYShelf()`)
- Flags `skip=true` for moves where target == current position
- Actions (PICK, PLACE, door) never skipped
- Returns count of **non-skipped** entries (dispatcher sends only those)

**Tracking** (updated only after READY):
- `pl_noteSettled(dir, data)` — updates `curX_slot` / `curY_shelf`
- `pl_noteConveyorSettled()` → `curX_slot = 0` (belt shifts columns)
- `pl_resetTracking()` → `-1` (unknown) on boot, home, E-stop

### `esp/src/Protocol/PinDriver.*` — ESP→Mega Physical Interface

**Production**: 5 wires — `DIR` + `DATA0..3` (4-bit) + `STROBE` pulse.

**`pd_send(dir, data)`**: Sets DIR pin, 4 DATA bits, pulses STROBE HIGH.

**`pd_waitReady(timeout)` / `pd_readyReceived()`**: Delegates to `ReadyHandler`.

**Simulation** (`SIM_MODE`): No pin toggling; `pd_signalReady()` arms simulated READY.

### `esp/src/Protocol/ReadyHandler.*` — READY Reception + Simulation

**Production**: Polls `PIN_ESP_READY` (HIGH = READY).

**Simulation**: Jittered delay `SIM_READY_DELAY_MIN..MAX_MS` after `rh_scheduleSimulation()`.

**Stall**: `rh_setStall(true)` cancels pending simulated READY (tests timeout/retry).

**Operator READY button** (sim console): `pd_signalReady()` → `rh_scheduleSimulation()`.

### `esp/src/Core/StateMachine.*` — Order FSM (Heart of ESP)

**States**: `IDLE`, `DISPATCHING`, `WAITING_FOR_READY`, `ERROR`.

**Key variables:**
- `activeOrder`, `drinkIndex`, `stepIndex`, `retryCount`, `conveyorPending`, `conveyorAdvances`
- `steps[8]` — current drink plan
- `manualPending`, `manualReadyPending` — manual primitive / operator-gated READY
- `routineActive` — `SIM_MODE` pick routine (extend → pick → retract)

**Flow:**
1. `startNextOrder()` — dequeues, sets `nextSlot` to item's `startSlot`, `drinkIndex=0`
2. `prepareDrink()` — calls planner, gets `stepCount` (8 or fewer if smart)
3. `dispatchCurrent()`:
   - If `conveyorPending` → sends `DIR_Y, conveyorCodeForShelf(shelf)`
   - Else sends next non-skipped `Cmd` from plan via `pd_send()`
   - Sets `state = WAITING_FOR_READY`, `readyDeadline = now + 30s`
4. **On READY** (`pd_readyReceived()`):
   - Manual primitive → notes in StateTracker, continues pick routine or returns IDLE
   - Planned primitive → `finishPrimitive()`:
     - Notes step in StateTracker + Planner tracking
     - Advances `stepIndex`; if drink complete (`stepIndex >= PLAN_SIZE`):
       - Increments `drinkIndex`, resets `retryCount`
       - Checks `inv_shouldAdvance()` → if true, sets `conveyorPending=true`
       - Else checks if item qty reached → advance item or complete order
     - If conveyor: sends conveyor command, waits READY, then `inv_resetSlots()`, `conveyorAdvances++`
5. **Timeout** (30 s):
   - Increments `retryCount` (max 3)
   - Resets `stepCount=0`, `stepIndex=0` → retries **whole drink from step 1 at same slot**
   - After 3 failures: publishes `failed`, halts if `HALT_ON_DRINK_FAILURE`
6. **Order timeout** (5 min no progress) → `ERROR` state, requires `reset`
7. **Auto-drain**: when `IDLE` and queue not empty → auto-starts next order

**Serial commands**: `help`, `order`, `status`, `run`, `step`, `reset`, `sim on/off`, `led on/off`, `stall on/off`.

### `esp/src/Core/StateTracker.*` — Mirrored Physical State

Tracks: `xSlot (0..10)`, `yPos (1..5)`, `zDown (bool)`, `zAction ("PICK"\|"PLACE"\|"EXTEND"\|...)`, `doorOpen (bool)`, `ledMode (ON\|OFF)`, `errorLatch`.

**`st_noteStep(dir, data)`** — called **only on settled READY** (timeouts never noted):
- `DIR_X` → `xSlot = data`
- `DIR_Y` shelf move → `yPos = data`
- `ACTION_PICK/PLACE` → `zDown=true`, `zAction="PICK"/"PLACE"`
- `ACTION_DOOR_OPEN/CLOSE` → `doorOpen=true/false`
- SIM pick routine phases → `zDown/zAction` transitions

**LED**: `errorLatch` forces red; else `ON`=green, `OFF`=off.

### `esp/src/Core/EventBus.*` — Trace + JSON Fan-Out

**Sink**: Single `EventSink` callback (WebUI registers WS broadcast).

**Events** (serial line + JSON):
| Event | Description |
|---|---|
| `order_start` | Banner + shelf/qty/slot |
| `drink_start` | "DRINK N/M (slot S)" |
| `step` | "ESP : [STEP n/m] DIR=X DATA=3 (Move X to slot 3)" |
| `step_skip` | Smart planner no-op trace |
| `ready` | "← READY received (Δ=123ms)" |
| `next_slot` | "→ nextSlot[3] = 4" |
| `conveyor_check` | "→ 11 > 10? YES — advance conveyor" |
| `drink_end` | "───..." |
| `order_done` | Banner + dispensed/conveyorAdvances/finalNextSlot |
| `warn` / `error` | Timeout/retry/halt |
| `log` | Arbitrary line (manual instructions, etc.) |
| `drink_ok` | JSON-only to backend |
| `state` | Change-only snapshot `{x,y,z,zAction,door,led,ledMode,machine}` |

`eb_pushState()` — forces snapshot on new WS client connect.

### `esp/src/Core/Scheduler.*` — Non-Blocking Services

- `sched_init()` → `rh_init()`
- `sched_update()` → `ws_maintain()` + `rh_update()` (called from `main.cpp` loop)

---

## Arduino Mega — Detailed Module Logic

### `mega/src/main.cpp` — Entry Point

```cpp
setup(): Serial 115200 → pr_init, ax_init, conv_init, door_init, grip_init, ms_init, sv_init, sm_init, sched_init
loop():  ms_handleSerial(), ms_update(), sv_update(), sm_update(), sched_update(micros())
```

### `mega/src/Config.h` — Mega Constants

**Pins**: All stepper step/dir/enable, servo pins, `PIN_UNUSED = 255` for all limit switches.

**Steppers:**
- `X_SPEED_MAX_HZ=800`, `X_ACCELERATION=1600`, `X_STEPS_PER_SLOT=200`
- `Y_SPEED_MAX_HZ=800`, `Y_STEPS_PER_SHELF=300` (dual Y in lockstep)
- `CONVEYOR_SPEED_MAX_HZ=800`, `CONVEYOR_STEPS_PER_COLUMN=400`

**Servos:** `Z/GRIP/DOOR_SPEED_MAX_HZ=90`, `*_DISTANCE_DEG=90`, `GRIP_HOLD_MS=200`

**READY**: `READY_PULSE_MS=100`

**Bench console**: `CMD_MAX_LENGTH=96`

### `mega/src/Protocol/PinReader.*` — ESP→Mega Input + READY Output

**Input**: Reads `DIR` + `DATA0..3` on STROBE rising edge (stub — wiring not installed).

**`pr_pulseReady()`**: Drives READY pin HIGH for `READY_PULSE_MS` (100 ms).

**`pr_update()`**: Times the READY pulse.

### `mega/src/Motion/StepperAxis.*` — Single Accel/Decel Stepper (micros)

- **Configure**: step/dir/enable pins, name
- **Profile**: `setSpeed(Hz)`, `setAcceleration(Hz/s)`, `setCurrent(mA)`
- **Move**: `move(steps)` relative, `moveTo(position)` absolute
- **`update(nowUs)`**: Velocity ramping (accel/decel), step pulse generation, position update
- **No `delay()`** — pure `micros()` timestamps
- **`isBusy()`**, `stop()`, `enable()`, `setPosition()`

### `mega/src/Motion/Axes.*` — X + Dual Y + Limit Switches

- **X**: Single `StepperAxis` (slot 0..10 → steps)
- **Y**: Two `StepperAxis` (Y1, Y2) driven in lockstep (shelf 1..5 → steps)
- **Homing**: `ax_homeX()` → move to 0; `ax_homeY()` → move both to 0
- **Zeroing**: `ax_zeroX/Y()` after homing settles
- **Limit monitoring** (`ax_updateLimits()`):
  - Edge-detect each switch (logs once per activation)
  - `X_HOME` → stops X only
  - `Y_TOP_LIMIT`, `Y_BOTTOM_LIMIT`, `Y_HOME` → stops both Y only
  - Shelf left/right limits (1..5) → stops X only
  - `DISPENSE_SWITCH` (shelf 3) → logs only
  - **Never aborts order** — only stops affected axis

### `mega/src/Motion/MultiStepper.*` — Bench-Mode AccelStepper x8

- Separate pin map (8 motors), `AccelStepper::DRIVER`
- Serial console: positional moves, continuous rotation, speed/dir, enable, stop, estop, status
- **Only compiled/used in `BENCH_MODE`**

### `mega/src/Motion/ServoController.*` — 3 Servos (Speed-Limited, millis)

- Pins 9,10,11. Per-servo min/max angle clamps.
- `sv_moveServo(index, speedDPS, direction, amount)` — relative move
- `sv_setPosition(index, angle)` — absolute
- `sv_update()` — called constantly, interpolates toward target at `speedDPS`

### `mega/src/EndEffector/Gripper.*` — Z + Gripper Sequence

**Hardware**: Z servo (pin 11), Gripper servo (pin 10).

**Phases** (state machine, `millis()` timed):
- `PICK`: `Z_UP` → `GRIP_CLOSE` → `HOLD (200ms)` → `Z_DOWN` → `IDLE`
- `PLACE`: `Z_UP` → `GRIP_OPEN` → `Z_DOWN` → `IDLE`
- `Z_ONLY`: Direct Z or gripper move

**Limits**: `Z_EXT_LIMIT`, `Z_RET_LIMIT`, `Z_HOME` (edge-detect, stops Z only).

**`grip_update()`** called from scheduler.

### `mega/src/EndEffector/Door.*` — Door Servo

- Single servo (pin 12). `door_open()` → `DOOR_OPEN_DEG`, `door_close()` → `DOOR_CLOSED_DEG`.
- Speed-limited via `DOOR_SPEED_MAX_HZ`. `door_update()` from scheduler.

### `mega/src/Shelf/Conveyor.*` — 5 Conveyor Steppers

- Array of 5 `StepperAxis` (pins from Config).
- `conv_advance(shelfId)` → `move(CONVEYOR_STEPS_PER_COLUMN)`
- `conv_update(nowUs)` → updates all 5.

### `mega/src/Homing/Home.*` — Homing Sequence

```
Z retract (grip_zDown) → X home (ax_homeX + ax_zeroX) → Y home (ax_homeY + ax_zeroY) → door_close
```

- `home_all()` starts sequence. `home_update()` advances phases when each axis settles.
- `home_isBusy()` → true during sequence.

### `mega/src/Core/StateMachine.*` — Bench Serial Console + READY Handshake

**Bench commands** (gated by `BENCH_MODE`):
```
x <slot>         # 0..10
xf/xb <steps>    # X forward/backward
y <shelf>        # 1..5 (15 = HOME ALL)
yf/yb <steps>    # Y forward/backward
z up/down
grip close/open
pick/place
door open/close
conv <1..5>      # advance conveyor
home             # full homing sequence
status           # full position dump
speed x|y|conv <hz>
stop             # emergency stop all
```

**Execution model**: Command sets `active=true`, `state=Running`, starts motion.

**`sm_update()`**: Polls serial, executes command, then waits for `settled()`:
- `settled()` = all axes/servos/conveyors/homing idle
- On settled → prints `OK <command>`, `active=false`, `state=Idle`

**`STOP`** → emergency stop all, `state=EmergencyStop`.

**Production**: Only DIR/DATA/STROBE from ESP via `PinReader`; bench console disabled.

### `mega/src/Core/Scheduler.*` — Central Update Loop

```cpp
sched_update(nowUs):
  ax_update(nowUs)        // steppers (micros)
  grip_update()           // Z + gripper (millis)
  door_update()           // door (millis)
  conv_update(nowUs)      // conveyors (micros)
  home_update()           // homing sequence
  ax_updateLimits()       // limit switches
  grip_updateLimits()     // Z limits
  pr_update()             // READY pulse timing
```

Called from `main.cpp` loop with `micros()`.

---

## Shared Protocol — `shared/RevoVProtocol.h`

```cpp
namespace revov {
  // Machine credentials (provision per-machine outside repo)
  constexpr const char* MACHINE_ID = "revov-dev";
  constexpr const char* MACHINE_TOKEN = "REPLACE_ME";

  // Geometry
  constexpr uint8_t SHELF_COUNT = 5;
  constexpr uint8_t SLOTS_PER_SHELF = 10;
  constexpr uint8_t COLUMNS_PER_SHELF = 10;
  constexpr uint8_t SHELF_FIRST = 1, SHELF_LAST = 5;
  constexpr uint8_t SLOT_FIRST = 1, SLOT_LAST = 10;
  constexpr uint8_t DISPENSE_SHELF = 3;
  constexpr uint8_t DISPENSE_SLOT = 0;

  // DIR codes
  constexpr uint8_t DIR_X = 0;
  constexpr uint8_t DIR_Y = 1;

  // Y-axis DATA codes
  constexpr uint8_t ACTION_PICK = 6;
  constexpr uint8_t ACTION_PLACE = 7;
  constexpr uint8_t ACTION_DOOR_OPEN = 8;
  constexpr uint8_t ACTION_DOOR_CLOSE = 9;
  constexpr uint8_t ACTION_CONVEYOR_GENERAL = 5;
  constexpr uint8_t ACTION_CONVEYOR_FIRST = 10;  // shelf 1
  constexpr uint8_t ACTION_CONVEYOR_LAST = 14;   // shelf 5

  // Conveyor code for shelf
  constexpr uint8_t conveyorCodeForShelf(uint8_t shelfId) {
    return ACTION_CONVEYOR_FIRST + shelfId - 1;
  }
}
```

- **Shelf 5 conveyor = DATA 14**. **DATA 15 reserved for HOME ALL** (bench).
- **Compiled by both boards** — single source of truth.

---

## Build & Run

### PlatformIO Environments

| Env | Board | Purpose |
|---|---|---|
| `esp32dev` | ESP32 DevKit | Production ESP build |
| `megaatmega2560` | Arduino Mega 2560 | Production Mega build (`BENCH_MODE`) |
| `mega_sim` | Arduino Mega 2560 | Mega with `SIM_MODE` (no ESP needed) |

```bash
# From repo root
pio run -e esp32dev          # build ESP
pio run -e megaatmega2560    # build Mega
pio run -e mega_sim          # build Mega with SIM_MODE
pio run -e esp32dev -t upload
pio run -e megaatmega2560 -t upload
```

### Open in VS Code

1. **Repo root** — root `platformio.ini` aggregates both (pick env in status bar)
2. **Workspace** — `revovfirmware.code-workspace` loads `esp/` and `mega/` as two projects
3. **Single project** — File → Open Folder → `esp/` or `mega/`

---

## Serial Commands

### ESP (115200 baud)

```
help
order <shelfId> <quantity> <startSlot>
order 5 2 4 ; 2 1 1 ; 3 3 7    # multi-item
status
run          # auto-drain queue
step         # single primitive
reset        # full machine reset
sim on/off   # toggle simulation
led on/off   # LED mode
stall on/off # suppress sim READY (test timeout)
```

### Mega Bench Console (115200 baud, `BENCH_MODE`)

```
help
x <slot>         # 0..10
xf <steps>       # X forward
xb <steps>       # X backward
y <shelf>        # 1..5 (15 = HOME ALL)
yf <steps>       # Y forward
yb <steps>       # Y backward
z up/down
grip close/open
pick/place
door open/close
conv <1..5>      # advance conveyor
home             # full homing sequence
status           # full position dump
speed x|y|conv <hz>
stop             # emergency stop all
```

---

## Key Invariants (Locked)

| Parameter | Value |
|---|---|
| Shelves | 5 (Y 1–5) |
| Slots per shelf | 10 (X 1–10) |
| Columns per shelf | 10 (conveyor 1–10) |
| Drop shelf | 3 (third shelf) |
| Drop slot | 0 (dedicated dispensing area) |
| Steps per drink cycle | 8 (+1 optional conveyor) |
| ESP READY timeout | 30 s |
| Drink retry limit | 3 |
| Mega READY pulse | 100 ms |

---

## Hard Rules

- ❌ **No `delay()` in Mega `loop()`** — everything non-blocking via `micros()`/`millis()`
- ❌ **Mega never parses orders, JSON, or "quantity"** — only executes primitives
- ✅ **Every command ends with exactly one READY pulse**
- ✅ **ESP gives up after `READY_TIMEOUT_MS = 30000`; Mega times out sooner**
- ✅ **Change `shared/RevoVProtocol.h` only in lockstep with both boards**

---

## Milestones (Mega)

| Milestone | Module | Status |
|---|---|---|
| M1 | PinReader | ☐ |
| M2 | StepperAxis | ☐ |
| M3 | Axes | ☐ |
| M4 | Gripper | ☐ |
| M5 | Door | ☐ |
| M6 | Conveyor | ☐ |
| M7 | Homing | ☐ |
| M8 | StateMachine | ☐ |
| M9 | Integration | ☐ |

---

## Not Yet Implemented / Hardware-Validated

- HMAC-SHA256 verification of WSS orders
- Nonce replay protection
- Duplicate `orderId`/idempotency enforcement
- Finalized backend canonical JSON and signature schema
- Serial PIN authentication for physical-console orders
- Production secrets provisioning outside source configuration
- Real WSS server integration testing
- Assignment and electrical validation of the physical Mega pin map
- Real endstop wiring and bench validation (all switches `PIN_UNUSED`)
- Driver-specific Vref/current programming
- Calibration of step counts, speeds, acceleration, servo angles, dwell
- Full ESP-to-Mega hardware-in-the-loop testing
- Mechanical obstruction and safety interlocks

---

## Simulation Mode (`SIM_MODE`)

- **ESP**: `PinDriver` replaced by fake driver. Prints each DIR/DATA, waits for operator READY button (web console) or auto-jittered READY.
- **Mega**: `mega_sim` env compiles with `SIM_MODE` — stubs hardware, allows bench testing without actuators.
- **Web Console**: Served from ESP soft AP (`RevoV-SIM` / `revov-sim`) at `http://<AP_IP>/` — order form, trajectory preview, live log, READY button, LED control, reset.

---

## File Tree (from `FOLDER_TREE.txt`)

```
revovfirmware/
├── Context_v3.md
├── esp_Context_v1.md
├── Mega_Context_v1.md
├── FOLDER_TREE.txt
├── README.md
├── .gitignore
├── SIM_GUIDE.md
├── platformio.ini
├── shared/
│   └── RevoVProtocol.h
├── esp/
│   ├── platformio.ini
│   └── src/
│       ├── main.cpp
│       ├── Config.h
│       ├── Net/
│       │   ├── WebSocket.h
│       │   └── WebSocket.cpp
│       ├── Web/
│       │   ├── Ui.h
│       │   ├── Ui.cpp
│       │   ├── Pages.h
│       │   ├── PagesGz.h
│       │   └── Brand.h
│       ├── Order/
│       │   ├── OrderTypes.h
│       │   ├── OrderQueue.h
│       │   ├── OrderQueue.cpp
│       │   ├── Inventory.h
│       │   └── Inventory.cpp
│       ├── Trajectory/
│       │   ├── ActionStep.h
│       │   ├── Planner.h
│       │   └── Planner.cpp
│       ├── Protocol/
│       │   ├── PinDriver.h
│       │   ├── PinDriver.cpp
│       │   ├── ReadyHandler.h
│       │   └── ReadyHandler.cpp
│       └── Core/
│           ├── StateMachine.h
│           ├── StateMachine.cpp
│           ├── Scheduler.h
│           ├── Scheduler.cpp
│           ├── EventBus.h
│           ├── EventBus.cpp
│           ├── StateTracker.h
│           ├── StateTracker.cpp
│           └── Diag.h
└── mega/
    ├── platformio.ini
    ├── revovfirmware.code-workspace
    ├── .gitignore
    └── src/
        ├── main.cpp
        ├── Config.h
        ├── Protocol/
        │   ├── PinReader.h
        │   └── PinReader.cpp
        ├── Motion/
        │   ├── StepperAxis.h
        │   ├── StepperAxis.cpp
        │   ├── Axes.h
        │   ├── Axes.cpp
        │   ├── MultiStepper.h
        │   ├── MultiStepper.cpp
        │   ├── ServoController.h
        │   └── ServoController.cpp
        ├── EndEffector/
        │   ├── Gripper.h
        │   ├── Gripper.cpp
        │   ├── Door.h
        │   └── Door.cpp
        ├── Shelf/
        │   ├── Conveyor.h
        │   └── Conveyor.cpp
        ├── Homing/
        │   ├── Home.h
        │   └── Home.cpp
        └── Core/
            ├── StateMachine.h
            ├── StateMachine.cpp
            ├── Scheduler.h
            └── Scheduler.cpp
```