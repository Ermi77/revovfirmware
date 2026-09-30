# RevoV Mega Context v1

## Role

The Arduino Mega 2560 is the motion controller. It receives one DIR/DATA
primitive from the ESP32, executes that primitive non-blockingly, and emits
READY only after all associated motion has settled. The Mega does not know
orders, inventory, WSS, or backend state.

## Hardware model

| Axis | Hardware | Meaning |
| --- | --- | --- |
| X | One stepper | Shelf slot position, 0-9 |
| Y | Two steppers in lockstep | Shelf height, 0-4 |
| Z | Servo | 0 degrees retract, 90 degrees extend |
| Gripper | Servo | 0 degrees release, 90 degrees grip by default |
| Door | Servo | 0 degrees close, 90 degrees open |
| Conveyors | Five steppers | One column advance per shelf |

All motion pins and calibration values are in `mega/src/Config.h`. Every
switch pin currently uses:

```cpp
constexpr uint8_t PIN_UNUSED = 255;
```

The switch names are present and ready for the final pin map:

```text
X_HOME
Y_TOP_LIMIT, Y_BOTTOM_LIMIT, Y_HOME
Z_EXT_LIMIT, Z_RET_LIMIT, Z_HOME
SHELF1_L_LIMIT through SHELF5_L_LIMIT
SHELF1_R_LIMIT through SHELF5_R_LIMIT
DISPENSE_SWITCH
```

## Protocol

| DIR | DATA | Meaning |
| ---: | ---: | --- |
| 0 | 0-9 | Move X to slot |
| 1 | 0-4 | Move Y to zero-indexed shelf |
| 1 | 6 | PICK |
| 1 | 7 | PLACE |
| 1 | 8 | Door open |
| 1 | 9 | Door close |
| 1 | 10-14 | Advance conveyor for shelves 1-5 |
| 1 | 15 | HOME ALL |

Shelf 5 conveyor DATA is 14. DATA 15 is the home-all command in the Mega
command path.

## Motion and homing behavior

`StepperAxis` uses `micros()` timestamps. Servo movement and multi-stage pick,
place, and homing sequences use `millis()` timestamps. `Scheduler` updates
motion every loop; there is no `delay()` in the loop or update paths.

Homing order is:

```text
Z retract -> X home -> Y home
```

X and Y positions are zeroed after their homing stages settle. The switch
monitor is non-blocking and axis-local:

- X home or shelf left/right limits stop X only.
- Y top/bottom/home limits stop both Y motors only.
- Z limits stop Z motion only.
- A triggered switch is logged once per activation edge.
- A limit event does not abort the current order.

`DISPENSE_SWITCH` is reserved for shelf 3 and currently has no physical pin.

## Bench console

When `BENCH_MODE` is enabled, the Mega accepts these 115200-baud commands:

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

`status` and live speed changes are bench-only diagnostics. `stop` stops all
axes. Every motion reports `OK <command>` only after it is settled.

## Mega files

```text
mega/
├── platformio.ini
└── src/
    ├── main.cpp
    ├── Config.h
    ├── Protocol/PinReader.h/.cpp
    ├── Motion/StepperAxis.h/.cpp, Axes.h/.cpp
    ├── EndEffector/Gripper.h/.cpp, Door.h/.cpp
    ├── Shelf/Conveyor.h/.cpp
    ├── Homing/Home.h/.cpp
    └── Core/StateMachine.h/.cpp, Scheduler.h/.cpp
```

## Not yet written or hardware-validated

- Final physical pin map and endstop wiring.
- Bench validation of switch polarity and debounce requirements.
- Driver-specific Vref/current programming.
- Calibration of speed, acceleration, step distances, servo angles, and grip
  dwell.
- Mechanical obstruction and safety interlocks.
- Full integrated ESP-to-Mega electrical and motion test.
