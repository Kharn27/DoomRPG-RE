# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for stable build guidance, [`DOCUMENTATION.md`](DOCUMENTATION.md) for the documentation index, and milestone documents for detailed hardware evidence.

## Latest merged hardware baseline

```text
PR   = #49 — first fail-closed native opcode execution
main = 6e43ef059db52783b7264e84579216cb2572a1e2
```

Current candidate:

```text
branch = agent/esp32-map1-native-ui-intent
base   = 6e43ef059db52783b7264e84579216cb2572a1e2
hardware-affecting head = 045b219dd7d6d06630eb446424e8d3d3fa3d249e
status = NATIVE STRING SPANS + UI INTENT FAMILY IMPLEMENTED; AWAITING REAL-CYD HARDWARE PASS
```

Detailed active milestone: [`MAP1_NATIVE_UI_INTENT.md`](MAP1_NATIVE_UI_INTENT.md).

Merged first-execution evidence: [`MAP1_NATIVE_OPCODE_EXEC1.md`](MAP1_NATIVE_OPCODE_EXEC1.md).

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
runtime ZIP dependency = forbidden
native backing store   = /DoomRPG-ESP32.pak
DoomRPG-RE desktop engine = executable specification/reference only
final CYD engine          = ESP32-native ownership
```

Current native direction:

```text
BSP source in native pack
 -> compact immutable EspMapRuntime
 -> allocation-free accessors
 -> mutable EspMapState
 -> tile/event lookup
 -> event descriptor + bytecode linkage
 -> mutable EspMapScriptState
 -> side-effect-free event filter
 -> fail-closed native opcode executor
 -> allocation-free native string spans
 -> compact native UI/player intents
 -> bounded native gameplay/UI owners
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

MAP_INTRO `/intro.bsp` / `Entrance`:

```text
source bytes = 21823
CRC32        = 623f34e4
nodes        = 223
lines        = 480
mapSprites   = 344
events       = 93
byteCodes    = 265
strings      = 94
stringData   = 7779 B
maxString    = 313 B
```

Event/bytecode topology:

```text
93 event tiles sorted + unique
265 command refs = 265 unique
0 overlaps / 0 gaps
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

Measured legacy structural allocation was `55341 B`; native ownership remains `40089 B` smaller (~72.4%).

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

Script-state ownership proof:

```text
payload       = 81 B
actual heap   = 100 B
initialFNV    = f9e3d9df
mutation test = f9e3d9df -> 99003167 -> f9e3d9df
```

## First native opcode execution — hardware validated

Supported state family:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

All other IDs still fail closed in `EspMapOpcodeExecutor_execute()`.

Real MAP_INTRO opcode corpus:

```text
2, 7, 8, 9, 10, 11, 13, 15,
16, 18, 19, 24, 26, 27, 40, 41
```

```text
refs           = 265
unique IDs     = 16
opcodeAuditFNV = 6f28df45
EV_CHANGESTATE = 41 refs
EV_NEXTSTATE   = 35 refs
EV_PREVSTATE   = 0 refs
```

First executed real BSP command:

```text
command index  = 50
opcode         = 19 / EV_NEXTSTATE
arg1           = 00000702
arg2           = 00000100
target tile    = 226
target event   = 16
state          = 0 -> 1
mutated        = yes
execFNV        = 646b565c
```

Rollback:

```text
scriptFNV f9e3d9df -> 9b636dec -> f9e3d9df
```

Fail-closed proofs:

```text
real command 0 / EV_CLOSELINE -> UNSUPPORTED, no mutation
malformed CHANGESTATE state 16 -> refused, no mutation
```

Integrity across first execution:

```text
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

## Current candidate — native UI/string intents

Recovered family:

```text
8  EV_DIALOG
24 EV_FORCEMESSAGE
26 EV_DIALOGNOBACK
40 EV_NOTE
```

Legacy behavior requires dialog presentation, forced status-bar text, or notebook mutation. This branch **does not call those legacy owners**.

Permanent allocation-free APIs:

```text
EspMapStrings_getRef()
EspMapUiIntent_supports()
EspMapUiIntent_build()
```

`EspMapStrings_getRef()` resolves one of the 94 length-prefixed map strings to:

```text
string index
BSP source payload offset
bounded payload length
```

String bytes remain on `/DoomRPG-ESP32.pak`; no map-wide text allocation is introduced.

UI translation produces caller-owned intents:

```text
DIALOG          -> string span + Back/no-Back + pause/resume metadata
FORCE_MESSAGE   -> string span + EMPTY_CLEARS semantic based on resolved text[0]
APPEND_NOTE     -> string span + recovered trailing "||" append semantic
```

The old state-mutating opcode executor remains unchanged and must continue refusing all UI-family bytecodes as `UNSUPPORTED`.

New persistent cost:

```text
0 B
```

Real-CYD probe must validate:

```text
all 94 string spans
payload sum = 7779 B
max length = 313 B
adjacent length-prefix topology
all real 8/24/26/40 command translations
first sample of each family
total/per-opcode reference counts
pause + FORCE_MESSAGE empty-clears semantic counts
zero-length FORCE_MESSAGE span count
spanFNV + intentFNV
state executor refuses every UI ref
0 heap/largest/frame/arena/map/script drift
Player.NotebookString unchanged
Hud.statBarMessage unchanged
Game continuation fields unchanged
entities=0 monsters=0 ST_PLAYING=no
```

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
 -> opcode inventory + real EV_NEXTSTATE execution/rollback
 -> string-span sweep
 -> real UI/string bytecodes -> native intents only
 -> PARK
```

Still forbidden:

```text
actual DoomCanvas dialog presentation
actual Hud mutation
actual Player notebook mutation
full native Game_runEvent execution loop
world/door/line/sprite mutation
map transitions
entity/monster activation
native gameplay rendering
ST_PLAYING
```

## Next action

Build/flash normal `esp32-cyd` from `agent/esp32-map1-native-ui-intent` and capture the `MAPSTRING`, `MAPUI`, `MAPUIPROBE` block plus a later stable `[ALIVE]` line.

If PASS, lock exact counts/fingerprints/samples in docs and verify all post-test commits are documentation-only before merge.
