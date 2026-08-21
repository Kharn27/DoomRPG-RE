# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for stable build guidance, [`DOCUMENTATION.md`](DOCUMENTATION.md) for the documentation index, and milestone documents for detailed hardware evidence.

## Latest merged hardware baseline

```text
PR   = #48 — native script state + side-effect-free Game_runEvent filtering
main = 0c8a52549ebb436139f7cd5c8b4ee63bdd175907
```

Current candidate:

```text
branch = agent/esp32-map1-native-opcode-exec1
base   = 0c8a52549ebb436139f7cd5c8b4ee63bdd175907
hardware-affecting head = 3e07f1b0c4c6f609da8008f20dc667c7bbe58af6
status = OPCODE INVENTORY + FIRST FAIL-CLOSED NATIVE EXECUTOR IMPLEMENTED; AWAITING REAL-CYD HARDWARE PASS
```

Detailed active milestone: [`MAP1_NATIVE_OPCODE_EXEC1.md`](MAP1_NATIVE_OPCODE_EXEC1.md).

Merged filter evidence: [`MAP1_NATIVE_EVENT_FILTER.md`](MAP1_NATIVE_EVENT_FILTER.md).

## Permanent target / ownership

```text
board        = ESP32-2432S028R classic CYD
MCU          = ESP32-D0WD-V3 dual-core 240 MHz
flash        = 4 MB
PSRAM        = none
display      = ILI9341 320x240
touch        = XPT2046
storage      = microSD
framebuffer  = 160x120 RGB565 = 38400 B
presentation = nearest-neighbor 2x
audio        = deferred
```

Permanent invariants:

```text
shapeData   == NULL
mediaTexels == NULL
DoomRPG-RE desktop engine = executable specification/reference only
final CYD engine          = ESP32-native ownership
```

Current native direction:

```text
BSP source
 -> compact immutable EspMapRuntime
 -> allocation-free accessors
 -> mutable EspMapState
 -> tile/event lookup
 -> event descriptor + bytecode linkage
 -> mutable EspMapScriptState
 -> side-effect-free event filter
 -> fail-closed native opcode executor
 -> later bounded gameplay effects
```

## Hardware-proven foundation

```text
source BSP FNV = d5cc751f
arenaFNV       = c3882516
decodedFNV     = a426dd18
mapStateFNV    = cd99b98e
lookupFNV      = 63430151
descriptorFNV  = 27115328
linkageFNV     = 5727902c
scriptFNV      = f9e3d9df
filterFNV      = a5923b21
resumeFNV      = b98452da
```

MAP_INTRO structure:

```text
nodes       = 223
lines       = 480
mapSprites  = 344
events      = 93
byteCodes   = 265
strings     = 94
```

Event/bytecode topology:

```text
93 event tiles are sorted + unique
265 command refs = 265 unique
0 overlaps
0 gaps
command count range = 1..14
max command end = 265
all initial event states = 0
event flag values = {0,1}
```

Persistent native RAM already measured:

```text
immutable arena       = 14112 B actual heap
mutable tile state    =  1040 B actual heap
mutable script state  =   100 B actual heap
-----------------------------------------
current total         = 15252 B
largest8              = 36852 preserved
```

Measured legacy structural allocation was `55341 B`, so the current native foundation remains `40089 B` smaller (~72.4%).

## Side-effect-free event filter — hardware validated

```text
contexts         = 142848
evaluations      = 407040
eligible         = 4784
eventBlocked     = 2048
stateMismatch    = 379392
keyMismatch      = 0
flagsMismatch    = 20816
blockInputEvents = 8
filterFNV        = a5923b21
resumeFNV        = b98452da
probe elapsed    = 1411 ms
```

Decision accounting is exact: all `407040` evaluations fall into one class. The 1411 ms is exhaustive probe/oracle cost, not runtime event cost.

Script-state ownership proof:

```text
payload       = 81 B
actual heap   = 100 B
initialFNV    = f9e3d9df
mutation test = f9e3d9df -> 99003167 -> f9e3d9df
```

## Current candidate — first native opcode execution

Recovered first safe executor family:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

Legacy semantics target an event by:

```text
targetTile = (arg1 & 0xff) + (((arg1 >> 8) & 0xff) * 32)
```

and mutate only its current event state. Native execution applies that mutation only to `EspMapScriptState`.

Permanent API:

```text
EspMapOpcodeExecutor_supports()
EspMapOpcodeExecutor_execute()
```

All other opcode IDs return `UNSUPPORTED` with no mutation. Malformed state values outside the native 4-bit `0..15` domain are refused.

New persistent cost:

```text
0 B
```

The real-CYD probe must:

```text
audit all 265 actual opcode IDs
require every ID inside recovered legacy range 1..42
report unique ID count + 0..63 masks
count real refs to 11 / 19 / 20
select one real supported BSP command
execute it against the 81 B script overlay
prove exact target/state mutation
rollback to scriptFNV=f9e3d9df
refuse a real unsupported command
refuse malformed CHANGESTATE state 16
keep heap/largest/frame/arena/map-state unchanged
PARK with entities=0 monsters=0 ST_PLAYING=no
```

If MAP_INTRO contains no valid real command in this first safe family, the probe must fail closed rather than execute a more dangerous opcode.

## Current candidate execution path

```text
validated intro disposal
 -> native BSP inventory
 -> compact resident arena
 -> accessor sweep
 -> mutable tile state
 -> tile/event lookup
 -> descriptor/linkage sweep
 -> 81 B script-state build
 -> exhaustive event-filter proof
 -> opcode inventory
 -> ONE reversible real state-opcode execution
 -> rollback
 -> PARK
```

Still forbidden:

```text
full native Game_runEvent execution loop
non-state opcode families
world/door/line/sprite mutation
HUD/dialog/sound effects
player/stat/inventory mutation
entity/monster activation
map transitions
native gameplay rendering
ST_PLAYING
```

## Next action

Build/flash normal `esp32-cyd` from `agent/esp32-map1-native-opcode-exec1` and capture the `MAPOPCODE` / `MAPOPCODEPROBE` block plus a later stable `[ALIVE]` line.

If the hardware pass succeeds, record the opcode inventory, `auditFNV`, executed real command, `execFNV`, rollback fingerprints and exact timing before merge.
