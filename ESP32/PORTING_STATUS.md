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
hardware-tested firmware content = 3e07f1b0c4c6f609da8008f20dc667c7bbe58af6
status = REAL-CYD HARDWARE PASS; FIRST REAL NATIVE OPCODE EXECUTED + ROLLED BACK; MERGE-READY
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
 -> bounded native gameplay effects
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
opcodeAuditFNV = 6f28df45
firstExecFNV   = 646b565c
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

Persistent native RAM measured:

```text
immutable arena       = 14112 B actual heap
mutable tile state    =  1040 B actual heap
mutable script state  =   100 B actual heap
opcode executor       =     0 B persistent
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

Decision accounting is exact. The 1411 ms is exhaustive probe/oracle cost, not runtime event cost.

Script-state ownership proof:

```text
payload       = 81 B
actual heap   = 100 B
initialFNV    = f9e3d9df
mutation test = f9e3d9df -> 99003167 -> f9e3d9df
```

## First native opcode execution — hardware validated

Supported first executor family:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

All other opcodes fail closed as `UNSUPPORTED`.

### Real MAP_INTRO opcode corpus

The real CYD audited all 265 compact bytecodes:

```text
refs         = 265
unique IDs   = 16
idMaskLo     = 0d0daf84
idMaskHi     = 00000300
outOfRange   = 0
opcodeAuditFNV = 6f28df45
```

Exact IDs present:

```text
2, 7, 8, 9, 10, 11, 13, 15,
16, 18, 19, 24, 26, 27, 40, 41
```

State-family references:

```text
EV_CHANGESTATE = 41
EV_NEXTSTATE   = 35
EV_PREVSTATE   = 0
supported refs = 76
```

### First executed real BSP command

```text
command index  = 50
opcode         = 19 / EV_NEXTSTATE
arg1           = 00000702
arg2           = 00000100
target tile    = 226
target event   = 16
prepared state = 0
state          = 0 -> 1
mutated        = yes
execFNV        = 646b565c
```

This is the first real Doom RPG opcode executed by the ESP32-native engine.

The effect is confined to `EspMapScriptState`; no immutable BSP/map bytes are changed.

Rollback proof:

```text
scriptFNV initial  = f9e3d9df
scriptFNV prepared = f9e3d9df
scriptFNV executed = 9b636dec
scriptFNV restored = f9e3d9df
rollback           = yes
```

Fail-closed proofs:

```text
real unsupported sample = command 0 / opcode 16 -> UNSUPPORTED, no mutation
malformed CHANGESTATE state 16 -> refused, no mutation
```

Hardware integrity across audit + execution + rollback:

```text
elapsed      = 1 ms
heap8        = 68828 -> 68828
largest8     = 36852 -> 36852
frameFNV     = 10f53ffb -> 10f53ffb
arenaFNV     = c3882516 -> c3882516
mapStateFNV  = cd99b98e -> cd99b98e
scriptFNV    = f9e3d9df -> f9e3d9df
entities     = 0
monsters     = 0
ST_PLAYING   = no
```

Later `[ALIVE]` remained stable at `heap8=68828`, `largest8=36852`.

## Execution path now proven

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
 -> full 265-bytecode opcode inventory
 -> ONE real EV_NEXTSTATE native execution
 -> exact state mutation
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

## Merge recommendation

**MERGE `agent/esp32-map1-native-opcode-exec1`.**

The real CYD has now validated both fail-closed dispatch and one actual reversible Doom RPG opcode execution. Any later commits must remain documentation-only to avoid requiring another flash.

## Next bounded milestone after merge

Use the measured real opcode corpus instead of guessing. Remaining real IDs are:

```text
2, 7, 8, 9, 10, 13, 15, 16,
18, 24, 26, 27, 40, 41
```

Audit their legacy semantics and group them by native ownership requirements. Add one coherent family at a time, preferring effects that can live in small native state/intent objects before touching entities or renderer-owned world mutation. Unsupported IDs continue to fail closed.
