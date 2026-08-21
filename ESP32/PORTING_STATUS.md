# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for stable build guidance, [`DOCUMENTATION.md`](DOCUMENTATION.md) for the documentation index, and milestone archives for detailed hardware evidence.

## Latest merged hardware baseline

```text
PR   = #53 — native DIALOG/NOBACK pause owner
main = 395418510207bf24ac45ddbb4c4c15db3ddc8998
hardware-tested firmware content = 85aa89c4218a819e7f18cbf77f64dfbef3c5bac9
```

Detailed merged evidence: [`MAP1_NATIVE_DIALOG_OWNER.md`](MAP1_NATIVE_DIALOG_OWNER.md).

## Current candidate

```text
branch = agent/esp32-map1-native-notebook-owner
base   = 395418510207bf24ac45ddbb4c4c15db3ddc8998
firmware candidate content = f619aefc85402d28c4de6edab5ca32ea1eb514dd
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

Detailed active milestone: [`MAP1_NATIVE_NOTEBOOK.md`](MAP1_NATIVE_NOTEBOOK.md).

The candidate consumes only the seven real `EV_NOTE` intents into a bounded caller-owned native map notebook. It reads individual note strings through the native pack, but does not mutate legacy `Player.NotebookString`, Hud, Game continuation, world/entities/rendering or enter gameplay.

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
 -> bounded pack-backed one-string reader
 -> explicit native effect/player owners
 -> native event/script loop
 -> ESP32-native gameplay + renderer
```

## Hardware-proven fingerprints through PR #53

```text
source BSP FNV   = d5cc751f
arenaFNV         = c3882516
decodedFNV       = a426dd18
mapStateFNV      = cd99b98e
lookupFNV        = 63430151
descriptorFNV    = 27115328
linkageFNV       = 5727902c
scriptFNV        = f9e3d9df
filterFNV        = a5923b21
resumeFNV        = b98452da
opcodeAuditFNV   = 6f28df45
firstExecFNV     = 646b565c
stringSpanFNV    = 713188eb
uiIntentFNV      = 7fdd6a79
stringContentFNV = e995ee51
statusApplyFNV   = 52b25a5f
dialogApplyFNV   = d0254f3d
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

## Persistent native RAM ownership

Hardware-proven persistent heap ownership remains:

```text
immutable arena       = 14112 B actual heap
mutable tile state    =  1040 B actual heap
mutable script state  =   100 B actual heap
opcode executor       =     0 B persistent heap
string spans/intents  =     0 B persistent heap
bounded string reader =     0 B persistent heap
effect-owner probes   =     0 B persistent heap
-----------------------------------------
current proven total  = 15252 B
largest8              = 36852 preserved
```

Caller-owned value types already proven:

```text
EspMapStatusMessageState =   8 B
EspMapDialogOwnerState   =  12 B
```

Current notebook candidate value type:

```text
EspMapNotebookState
 = char text[512]
 + uint16_t length
 = 514 B expected on classic ESP32 ABI
```

Its temporary probe keeps that value on stack and therefore should add 0 persistent heap bytes. A future permanent native player owner embedding it must account the 514 bytes explicitly.

Measured legacy structural allocation was `55341 B`; hardware-proven native structural/script heap ownership remains `40089 B` smaller (~72.4%).

## Hardware-proven event/UI boundary

State-only native opcode executor supports only:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

All other opcodes remain fail-closed there.

Real MAP_INTRO UI/string corpus:

```text
EV_DIALOG       = 76
EV_FORCEMESSAGE = 3
EV_DIALOGNOBACK = 8
EV_NOTE         = 7
total UI refs   = 94
pause intents   = 84
uiIntentFNV     = 7fdd6a79
```

Bounded string reader remains hardware-proven:

```text
strings          = 94
payload          = 7779 B
zeroLength       = 1
max              = 313 B
spanFNV          = 713188eb
contentFNV       = e995ee51
packPayloadReads = 93
```

Canonical payload fingerprints include:

```text
string 1  / FORCE_MESSAGE = f6da01bb
string 25 / DIALOG        = 84f743cf
string 30 / DIALOGNOBACK  = 3692ac94
string 85 / NOTE          = ee639dc1
```

## Native FORCE_MESSAGE owner — hardware validated

```text
refs             = 3
set              = 1
clear            = 2
ownerBytes       = 8
textCopyBytes    = 0
statusApplyFNV   = 52b25a5f
ownerAtomic      = yes
```

## Native DIALOG/NOBACK pause owner — hardware validated

```text
refs             = 84
Back             = 76
noBack           = 8
pause            = 84
skipTurn         = 84
resumeExact      = 84
stateExecRefused = 84
ownerBytes       = 12
textCopyBytes    = 0
packIO           = no
persistentHeap   = 0
dialogApplyFNV   = d0254f3d
```

Canonical samples:

```text
Back   cmd11 event6 off0 resume1 flags07 string25@13558+23
noBack cmd19 event6 off8 resume9 flags06 string30@13679+14
```

Atomic fail-closed proof:

```text
unsupported=1 badFlags=1 badKind=1 badRef=1 badEvent=1 badGlobal=1 badResume=1 nullIntent=1
ownerAtomic=yes reset=1
```

Latest hardware-tested build integrity (dialog firmware):

```text
heap8       = 68780 -> 68780
largest8    = 36852 -> 36852
frameFNV    = ef79123a -> ef79123a
arenaFNV    = c3882516 -> c3882516
mapStateFNV = cd99b98e -> cd99b98e
scriptFNV   = f9e3d9df -> f9e3d9df
notebookFNV = 4d7705c5 -> 4d7705c5
packIO      = no
```

A complete post-PARK heartbeat remained stable at `heap=134544`, `heap8=68780`, `largest8=36852`.

## Current NOTE notebook candidate contract

Recovered legacy state:

```text
Player.NotebookString[512]
reset on Player_setup()
EV_NOTE appends map string + "||"
Menu_setNotes later splits on '|'
```

Permanent native candidate:

```text
EspMapNotebookState
text capacity  = 512 B
max payload    = 511 B + NUL
length field   = uint16_t
owner size     = 514 B expected
heap allocation= 0
```

The native owner performs a deterministic bounded append equivalent to the intended legacy format:

```text
existing text + source C-string + "||"
truncate at 511 payload bytes
always terminate with NUL
```

It validates NOTE intent provenance and the canonical string ref before reading. State is committed only after the proven `EspMapStrings_read()` succeeds.

Inherited real corpus:

```text
NOTE refs          = 7
stateExecRefused   = 7 expected
separator semantics= 7 expected
```

Canonical sample:

```text
cmd103 event40 off8 string85@18964+54 payloadFNV=ee639dc1
```

The probe additionally proves controlled bounds independent of the corpus:

```text
empty + sample -> exact trailing "||"
510-byte payload + sample -> 511-byte payload + NUL
full 511-byte payload + sample -> unchanged
```

Expected fail-closed set:

```text
unsupported DIALOG
bad NOTE flags
bad intent kind
mutated string ref
bad source event
bad global command index
short scratch
NULL intent
closed native pack
```

All must preserve the notebook owner atomically.

Real-CYD validation must establish rather than predeclare:

```text
noteApplyFNV
seven-note source byte total
final notebook length
final notebook active-content FNV
final 512-byte storage FNV
new-build heap/framebuffer absolute values
```

Legacy `Player.NotebookString` must remain exactly:

```text
FNV over 512 bytes = 4d7705c5
```

before and after the new probe.

## Current hardware-proven execution path

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
 -> real EV_NEXTSTATE execution/rollback
 -> string spans + UI intents
 -> bounded native-pack string reader
 -> real EV_FORCEMESSAGE -> native status owner
 -> real EV_DIALOG/NOBACK -> native pause owner
 -> candidate: real EV_NOTE -> bounded native notebook owner
```

Still forbidden:

```text
actual DoomCanvas dialog presentation
legacy Game continuation mutation by native owner
legacy Hud mutation
legacy Player.NotebookString mutation
actual notes-menu presentation
full native Game_runEvent execution loop
world/door/line/sprite mutation
map transitions
entity/monster activation
native gameplay rendering
ST_PLAYING
```

## Current validation target

Build/flash the normal optimized environment:

```text
esp32-cyd
```

from `agent/esp32-map1-native-notebook-owner` and capture the `[MAPNOTE]` / `[MAPNOTEPROBE]` family plus a later stable `[ALIVE]` heartbeat.

Do not mark this branch merge-ready until the real classic CYD supplies the PASS.

## Next bounded milestone after a PASS + merge

Do not preselect it now. Reread the then-current repository, recovery docs, merged NOTE milestone and exact remaining MAP_INTRO opcode behavior before choosing the next coherent boundary.
