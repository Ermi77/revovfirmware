# Mega serial simulation guide

The simulation uses the normal Mega source modules. `SIM_MODE` is only a build
flag; there is no separate simulation module. With the flag enabled, the real
motion modules use virtual positions and print their activity to Serial. The
parallel DIR/DATA input, 5 ms command debounce, command dispatch,
non-blocking timing, and 100 ms READY handshake remain active.

## Build and upload

From the repository root:

```text
pio run -d mega -e mega_sim
pio run -d mega -e mega_sim -t upload
pio device monitor -d mega -e mega_sim -b 115200
```

The simulation uses the same Arduino Mega board definition and source files as
the normal firmware. Connect the ESP link and USB serial while testing. Do not
connect motor power or motor drivers.

The normal target remains:

```text
pio run -d mega -e megaatmega2560
```

## Trace format

The Mega source writes one JSON object per line at 115200 baud when `SIM_MODE`
is enabled:

- `cmd`: a command that remained stable for `CMD_STABLE_MS`
- `dispatch`: the decoded action
- `motion_start`: virtual axis start and target positions
- `motion_done`: virtual axis completion
- `servo`: simulated gripper state
- `ready_pulse`: the single completion acknowledgement

All timing fields are `millis()` timestamps. Filter the serial capture as JSON
lines for replay or comparison with `Context_v3.md` section 2.

## Acceptance checklist

1. **T1 worked example:** feed `(X,4)`, `(Y,4)`, `(Y,6)`, `(Y,3)`,
   `(X,0)`, `(Y,8)`, `(Y,7)`, `(Y,9)`, then repeat the same sequence with
   `(X,5)`. Each command must produce one `ready_pulse`, in order.
2. **T2 overflow:** after a slot sequence crosses 9, send
   `(Y,10+shelfId)`. The trace must show one conveyor dispatch and motion,
   one READY pulse, then the next slot command.
3. **T3 unknown code:** `(Y,15)` must produce `cmd`, `dispatch` with
   `unknown`, and one `ready_pulse`, with no motion events.
4. **T4 debounce:** change DIR/DATA and change them again before 5 ms.
   No `dispatch` or `ready_pulse` may be produced for the transient value.
5. **T5 idle:** after the final READY, no further motion or READY event occurs
   until the input level changes to a new command.

## T1 trace shape

The numeric timestamps depend on the host clock. The event order should match
this abbreviated capture:

```json
{"ev":"cmd","dir":0,"data":4,"t":...}
{"ev":"dispatch","action":"moveX","t":...}
{"ev":"motion_start","axis":"X","from":0,"to":800,"t":...}
{"ev":"motion_done","axis":"X","t":...}
{"ev":"ready_pulse","t":...}
{"ev":"cmd","dir":1,"data":4,"t":...}
{"ev":"dispatch","action":"moveY","t":...}
{"ev":"ready_pulse","t":...}
{"ev":"cmd","dir":1,"data":6,"t":...}
{"ev":"dispatch","action":"pick","t":...}
{"ev":"motion_start","axis":"Z","from":0,"to":250,"t":...}
{"ev":"servo","state":"closed","t":...}
{"ev":"motion_start","axis":"Z","from":250,"to":0,"t":...}
{"ev":"ready_pulse","t":...}
```

The remaining six commands follow the same `cmd → dispatch → motion events →
ready_pulse` pattern. The second drink starts with `(X,5)`.
