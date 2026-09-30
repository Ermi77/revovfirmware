# revovfirmware

RevoV vending machine firmware — **ESP32 (brain)** + **Arduino Mega (motion slave)**.

## TL;DR

| Board | Role | Talks to |
|---|---|---|
| **ESP32** (`esp32/`) | Cloud brain — WebSocket, order queue, trajectory planning | DB via Wi-Fi · Mega via 5+1 wires |
| **Mega** (`mega/`) | Dumb motion slave — steppers, gripper, door, conveyors | ESP only |

**One contract:** `shared/RevoVProtocol.h` — the only file both boards compile.
**Never change it on one side only.**

## How it works
DB ──WebSocket──► ESP32 ──DIR+DATA+STROBE──► Mega ──► motors
◄──────READY──────────────

text

- ESP sends **one command at a time**, waits for **READY**, sends next.
- Mega never decides anything — just executes and pulses READY when settled.
- Order = array of triplets: `(shelfId 1–5, quantity 1–10, startSlot 0–9)`.
- 8 commands per drink (see `Context_v3.md §5`).
- After 10 slots → ESP sends conveyor-advance command.

## Repo layout
revovfirmware/
├── README.md ← you are here
├── Context_v3.md ← system truth (ESP + Mega + DB)
├── Mega_Context_v1.md ← Mega-side truth (pins, motors, states)
├── esp_Context_v1.md ← ESP-side truth
├── shared/
│ └── RevoVProtocol.h ← compiled by BOTH boards
├── esp32/ ← PlatformIO project 1
│ ├── platformio.ini
│ └── src/{main.cpp, Config.h, Net/, Order/, Trajectory/, Protocol/, Core/}
└── mega/ ← PlatformIO project 2
├── platformio.ini
└── src/{main.cpp, Config.h, Protocol/, Motion/, EndEffector/, Shelf/, Homing/, Core/}

text

## Build

Open the repo root in **VS Code + PlatformIO**. Both projects auto-detect.

```bash
pio run -e esp32dev          # build ESP
pio run -e megaatmega2560    # build Mega
pio run -e esp32dev -t upload
pio run -e megaatmega2560 -t upload
Start coding
Every session begins with:

Read Context_v3.md (system) and Mega_Context_v1.md (Mega),
then implement <module>.

Milestones (Mega): M1 PinReader → M2 StepperAxis → M3 Axes → M4 Gripper →
M5 Door → M6 Conveyor → M7 Homing → M8 StateMachine → M9 Integration.

Hard rules
❌ No delay() in Mega loop() — everything non-blocking via micros().

❌ Mega never parses orders, JSON, or "quantity".

✅ Every command ends with exactly one READY pulse.

✅ ESP gives up after READY_TIMEOUT_MS = 5000; Mega times out sooner.

✅ Change shared/RevoVProtocol.h only in lockstep with both boards.

Key numbers (locked)
Shelves	5 (Y 0–4)
Slots per shelf	10 (X 0–9)
Columns per shelf	10 (conveyor 0–9)
Drop shelf	3 (0-indexed)
Drop slot	0
Steps/drink cycle	8 (+1 optional conveyor)
Status
☑ Repo scaffolded
□ M1 PinReader
□ M2 StepperAxis
□ M3 Axes
□ M4 Gripper
□ M5 Door
□ M6 Conveyor
□ M7 Homing
□ M8 StateMachine
□ M9 Integration
□ ESP side: WebSocket, OrderQueue, Planner, StateMachine
