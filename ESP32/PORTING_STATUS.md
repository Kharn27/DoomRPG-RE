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
hardware-tested firmware content = 045b219dd7d6d06630eb446424e8d3d3fa3d249e
status = REAL-CYD HARDWARE PASS; NATIVE STRING SPANS + UI INTENTS VALIDATED; MERGE-READY
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
 -> bounded native string reader / effect owners
 -> ESP32-native gameplay + renderer
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
stringSpanFNV  = 713188eb
uiIntentFNV    = 7fdd6a79
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
string spans/intents  =     0 B persistent
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

All other IDs fail closed in `EspMapOpcodeExecutor_execute()`.

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

## Native UI/string intents — hardware validated

Recovered family:

```text
8  EV_DIALOG
24 EV_FORCEMESSAGE
26 EV_DIALOGNOBACK
40 EV_NOTE
```

Permanent allocation-free APIs:

```text
EspMapStrings_getRef()
EspMapUiIntent_supports()
EspMapUiIntent_build()
```

String bytes remain on `/DoomRPG-ESP32.pak`; no map-wide `mapStringsIDs[]` allocation exists.

### String span proof

```text
strings         = 94
payload bytes   = 7779
zero-length     = 1
max length      = 313
first payload   = 11554
last payload    = 19512 + 7
spanFNV         = 713188eb
persistentBytes = 0
```

### Real UI corpus

```text
refs                 = 94
EV_DIALOG            = 76
EV_FORCEMESSAGE      = 3
EV_DIALOGNOBACK      = 8
EV_NOTE              = 7
pause intents        = 84
force-empty semantics= 3
zero-length force    = 2
state-exec refused   = 94
intentFNV            = 7fdd6a79
probe elapsed        = 1 ms
persistentBytes      = 0
```

Accounting is exact: `76 + 3 + 8 + 7 = 94`, and `76 + 8 = 84` dialog pause/resume intents.

Canonical samples:

```text
dialog: cmd11 event6 off0 resume1 flags07 string25@13558+23
force : cmd4  event2 off0 resume1 flags08 string1@11569+14
noBack: cmd19 event6 off8 resume9 flags06 string30@13679+14
note  : cmd103 event40 off8 resume9 flags10 string85@18964+54
```

The state-mutating opcode executor refused all 94 UI commands as `UNSUPPORTED`, preserving strict effect-family separation.

No legacy effect was applied:

```text
DoomCanvas dialog state = untouched
Hud.statBarMessage      = untouched
Player.NotebookString   = untouched
Game continuation fields= untouched
world/entities/render   = untouched
```

Hardware integrity across the UI/string sweep:

```text
heap8        = 68820 -> 68820
largest8     = 36852 -> 36852
frameFNV     = b8b39f0f -> b8b39f0f
arenaFNV     = c3882516 -> c3882516
mapStateFNV  = cd99b98e -> cd99b98e
scriptFNV    = f9e3d9df -> f9e3d9df
notebookFNV  = 4d7705c5 -> 4d7705c5
entities     = 0
monsters     = 0
ST_PLAYING   = no
```

Later `[ALIVE]` remained stable at `heap8=68820`, `largest8=36852`.

The absolute heap8 moved from `68828` on the previous build to `68820`; the current stage itself has exact zero drift, so this is treated as a build/layout difference rather than a runtime allocation.

## Current proven execution path

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
 -> allocation-free 94-string span sweep
 -> 94 real UI/string bytecodes -> caller-owned native intents
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

## Merge recommendation

**MERGE `agent/esp32-map1-native-ui-intent`.**

Hardware-tested firmware content is `045b219dd7d6d06630eb446424e8d3d3fa3d249e`. Post-test commits must remain documentation-only unless another flash is performed.

## Next bounded milestone after merge

Prefer a **bounded native string reader backed directly by `/DoomRPG-ESP32.pak`**:

```text
EspMapStringRef
 -> exact range read
 -> bounded caller buffer (MAP_INTRO max 313 B + terminator)
 -> validate real string bytes/content fingerprint
 -> no map-wide strings
 -> no ZIP runtime dependency
```

Only after that boundary is hardware-proven should a native dialog/status/notebook owner begin consuming UI intents.
