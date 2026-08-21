# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for stable build guidance, [`DOCUMENTATION.md`](DOCUMENTATION.md) for the documentation index, and milestone archives for detailed evidence.

## Latest merged hardware baseline

```text
PR   = #52 — native FORCE_MESSAGE status owner
main = 40b61af5e2115266d4d03dddcc3175850538b0f5
hardware-tested firmware content = d782681c3cd267b9f16c290a593c1b6e5b34df1c
```

Detailed merged evidence: [`MAP1_NATIVE_STATUS_MESSAGE.md`](MAP1_NATIVE_STATUS_MESSAGE.md).

## Current candidate

```text
branch = agent/esp32-map1-native-dialog-owner
base   = 40b61af5e2115266d4d03dddcc3175850538b0f5
firmware candidate content = 85aa89c4218a819e7f18cbf77f64dfbef3c5bac9
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

Detailed active milestone: [`MAP1_NATIVE_DIALOG_OWNER.md`](MAP1_NATIVE_DIALOG_OWNER.md).

The candidate consumes only the real `EV_DIALOG` / `EV_DIALOGNOBACK` intents into a compact caller-owned pause/presentation state. It performs no presentation, no pack I/O, no legacy `DoomCanvas` or `Game_t` continuation mutation, and no world/entity/render mutation.

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
 -> explicit native effect owners
 -> native event/script loop
 -> ESP32-native gameplay + renderer
```

## Hardware-proven fingerprints through PR #52

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

`EspMapStatusMessageState` is an 8-byte caller-owned value. The dialog candidate adds a **12-byte caller-owned value type** only; its temporary probe allocates no persistent heap and copies no text.

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

## Native FORCE_MESSAGE owner — merged hardware baseline

Real CYD proof:

```text
refs                  = 3
set                   = 1
clear                 = 2
transition            = 1
ownerBytes            = 8
textCopyBytes         = 0
stateExecRefused      = 3
statusApplyFNV        = 52b25a5f
```

Canonical transitions:

```text
set   = cmd4 event2 off0 string1@11569+14 payloadFNV=f6da01bb
clear = cmd5 event2 off1 string2@11585+0
```

Atomic fail-closed:

```text
unsupported=1 badFlags=1 badRef=1 shortBuffer=1 nullIntent=1 closedPack=1
ownerAtomic=yes
```

Current tested build integrity:

```text
heap8 before/open/after = 68796 / 64432 / 68796
PAK transient cost      = 4364 B
largest8                = 36852 preserved
frameFNV                = faa62417 -> faa62417
arenaFNV                = c3882516 -> c3882516
mapStateFNV             = cd99b98e -> cd99b98e
scriptFNV               = f9e3d9df -> f9e3d9df
notebookFNV             = 4d7705c5 -> 4d7705c5
```

## Current DIALOG/NOBACK candidate contract

Recovered legacy behavior:

```text
DoomCanvas_startDialog(text, codeId == EV_DIALOG)
saveTileEvent = true
tileEvent     = event
skipAdvanceTurn = true

on close:
resume Game_runEvent(..., tileEventIndex + 1, tileEventFlags)
```

Permanent candidate state:

```text
EspMapDialogOwnerState
 = immutable EspMapStringRef
 + source event
 + source command offset
 + resume command offset
 + Back/pause/skip-turn flags
 + active bit
 = 12 B expected on classic ESP32 ABI
```

No persistent text is copied. No PAK read is required when capturing the owner because dialog semantics do not branch on text content. A future presenter reads `state.text` with `EspMapStrings_read()`.

The dynamic event activation flags corresponding to legacy `tileEventFlags` remain future event-loop context and are not invented by this milestone.

Expected real corpus:

```text
dialog refs       = 84
Back              = 76
noBack            = 8
pause             = 84
skipTurn          = 84
resumeExact       = 84
stateExecRefused  = 84
ownerBytes        = 12
textCopyBytes     = 0
packIO            = no
persistentHeap    = 0
```

Canonical samples already hardware-proven by the intent layer:

```text
Back:
cmd11 event6 off0 resume1 flags07 string25@13558+23

noBack:
cmd19 event6 off8 resume9 flags06 string30@13679+14
```

The candidate additionally validates intent provenance against the immutable event descriptor / bytecode before committing continuation metadata.

Real-CYD validation must establish the new `dialogApplyFNV` and prove atomic refusal of:

```text
unsupported FORCE_MESSAGE
bad flags
bad kind
mutated ref
bad source event
bad global command index
bad resume offset
NULL intent
```

Before/after integrity must preserve heap, largest block, framebuffer, arena, map state, script state, notebook, legacy HUD pointer and all legacy Game continuation fields.

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
 -> candidate: real DIALOG/NOBACK -> native pause owner
```

Still forbidden:

```text
actual DoomCanvas dialog presentation
legacy Game continuation mutation by native owner
legacy Hud mutation
actual Player notebook mutation
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

from `agent/esp32-map1-native-dialog-owner` and capture the `[MAPDIALOG]` / `[MAPDIALOGPROBE]` family plus a later stable `[ALIVE]` heartbeat.

Do not mark this branch merge-ready until the real classic CYD supplies the PASS.

## Next bounded milestone after a PASS + merge

Reread the then-current repository and legacy behavior before choosing. `EV_NOTE` is a likely next small explicit owner, but this is not pre-authorized as an implementation decision.
