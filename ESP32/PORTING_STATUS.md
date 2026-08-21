# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for stable build guidance, [`DOCUMENTATION.md`](DOCUMENTATION.md) for the documentation index, and milestone archives for detailed hardware evidence.

## Latest merged hardware baseline

```text
PR   = #54 — native NOTE notebook owner
main = 03002f79eb03bdcb4c9e430c43e4693dab47e44b
hardware-tested firmware content = f619aefc85402d28c4de6edab5ca32ea1eb514dd
```

Detailed merged evidence: [`MAP1_NATIVE_NOTEBOOK.md`](MAP1_NATIVE_NOTEBOOK.md).

## Current merge-ready candidate

```text
branch = agent/esp32-map1-native-key-gate
base   = 03002f79eb03bdcb4c9e430c43e4693dab47e44b
hardware-tested firmware content = 3b4844e8fa5d38d522e1adc70ffac646978f130d
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Detailed active milestone: [`MAP1_NATIVE_KEY_GATE.md`](MAP1_NATIVE_KEY_GATE.md).

The candidate supports only real `41 / EV_CHECK_KEY` as a pure native script-control evaluator. It returns PASS/BLOCKED metadata from caller-supplied key bits and performs no persistent allocation, pack I/O or legacy Player/Hud/Sound/Game/world/render mutation.

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
 -> pure native dynamic gates
 -> native event/script loop
 -> ESP32-native gameplay + renderer
```

## Hardware-proven fingerprints

```text
source BSP FNV       = d5cc751f
arenaFNV             = c3882516
decodedFNV           = a426dd18
mapStateFNV          = cd99b98e
lookupFNV            = 63430151
descriptorFNV        = 27115328
linkageFNV           = 5727902c
scriptFNV            = f9e3d9df
filterFNV            = a5923b21
resumeFNV            = b98452da
opcodeAuditFNV       = 6f28df45
firstExecFNV         = 646b565c
stringSpanFNV        = 713188eb
uiIntentFNV          = 7fdd6a79
stringContentFNV     = e995ee51
statusApplyFNV       = 52b25a5f
dialogApplyFNV       = d0254f3d
noteApplyFNV         = 43183162
notebookContentFNV   = 599609e0
notebookStorageFNV   = 75cf54e0
keyGateFNV           = 9ace79cd
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

Real opcode IDs:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

## Persistent native RAM ownership

```text
immutable arena       = 14112 B actual heap
mutable tile state    =  1040 B actual heap
mutable script state  =   100 B actual heap
opcode executor       =     0 B persistent heap
string spans/intents  =     0 B persistent heap
bounded string reader =     0 B persistent heap
effect/gate probes    =     0 B persistent heap
-----------------------------------------
current proven total  = 15252 B
largest8              = 36852 preserved
```

Caller-owned values proven on classic CYD:

```text
EspMapStatusMessageState =   8 B
EspMapDialogOwnerState   =  12 B
EspMapNotebookState      = 514 B
EspMapKeyGateResult      =  12 B
```

CHECK_KEY has no persistent owner and no persistent heap.

## Hardware-proven native execution/effect boundary

State-only opcode executor still supports only:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

Hardware-proven effect/player families:

```text
FORCEMESSAGE:
  refs=3 set=1 clear=2 ownerBytes=8 statusApplyFNV=52b25a5f

DIALOG/NOBACK:
  refs=84 Back=76 noBack=8 pause=84 skipTurn=84 resumeExact=84
  ownerBytes=12 dialogApplyFNV=d0254f3d

NOTE:
  refs=7 sourceBytes=256 finalLen=270 ownerBytes=514
  noteApplyFNV=43183162 contentFNV=599609e0 storageFNV=75cf54e0

CHECK_KEY:
  refs=1 green=0 yellow=1 blue=0 red=0
  scenarios=16 pass=8 blocked=8 resultBytes=12
  stateExecRefused=1 keyGateFNV=9ace79cd
```

Canonical CHECK_KEY sample:

```text
cmd38 event11 off0 key=1 mask=02 arg2=00000100
missingMessage="Need Yellow Key"
sound=5065
saveOffset=current
```

Truth-table proof:

```text
perRef=16 passEach=8 blockedEach=8
messages=4/4
extraBitsIgnored=1
PASS effect=none
BLOCKED effect=message+sound+stop+saveCurrent
```

Fail-closed hardware proof:

```text
unsupported=1 badOffset=1 badDescriptor=1 nullDescriptor=1 nullResult=1
```

## Current tested-build integrity

CHECK_KEY stage:

```text
heap8             = 68756 -> 68756
largest8          = 36852 -> 36852
frameFNV          = c56f998b -> c56f998b
arenaFNV          = c3882516 -> c3882516
mapStateFNV       = cd99b98e -> cd99b98e
scriptFNV         = f9e3d9df -> f9e3d9df
legacyNotebookFNV = 4d7705c5 -> 4d7705c5
legacyKeys        = 00000000 -> 00000000
hudFNV            = 505b1255 -> 505b1255
persistentBytes   = 0
pack I/O          = none
```

Preceding NOTE stage on the same firmware:

```text
heap8             = 68756 -> 68756
largest8          = 36852 -> 36852
frameFNV          = c56f998b -> c56f998b
heapOpen          = 64384
transientHeapCost = 4372 B
persistentHeap    = 0 B
```

Absolute heap/frame values vary slightly between firmware builds. Every stage has exact before/after stability and inherited structural fingerprints remain canonical, so these are build-layout/content differences rather than persistent allocation or framebuffer mutation.

Complete post-PARK heartbeat:

```text
uptime=25410 ms
heap=134520
heap8=68756
largest8=36852
all reported subsystems = ready
```

## Current hardware-proven execution path

```text
validated intro disposal
 -> native BSP inventory
 -> compact resident arena
 -> allocation-free access
 -> mutable tile state
 -> tile/event lookup
 -> descriptor/linkage
 -> compact script state
 -> exhaustive event filter
 -> real state-opcode execution/rollback
 -> string spans + UI intents
 -> bounded native-pack string reader
 -> real EV_FORCEMESSAGE -> native status owner
 -> real EV_DIALOG/NOBACK -> native pause owner
 -> real EV_NOTE -> bounded native notebook owner
 -> real EV_CHECK_KEY -> pure native dynamic gate
 -> PARK + stable post-PARK heartbeat
```

Still forbidden:

```text
actual DoomCanvas dialog/password presentation
legacy Game continuation mutation by native code
legacy Hud mutation
legacy Player key/notebook mutation
actual sound playback from native gate
full native Game_runEvent execution loop
world/door/line/sprite mutation
map transitions
savegame mutation
entity/monster activation
native gameplay rendering
ST_PLAYING
```

## Remaining MAP_INTRO families

Not yet owned/executed natively:

```text
2  EV_CHANGEMAP
7  EV_SHOW
9  EV_GIVEMAP
10 EV_PASSWORD
13 EV_UNLOCK
15 EV_OPENLINE
16 EV_CLOSELINE
18 EV_HIDE
27 EV_SAVEGAME
```

PASSWORD remains a bounded pause/input candidate. SHOW/HIDE/GIVEMAP/UNLOCK/OPENLINE/CLOSELINE cross into the first explicit world/render overlays. CHANGEMAP and SAVEGAME remain larger later boundaries.

## Merge recommendation

**MERGE `agent/esp32-map1-native-key-gate`.**

Hardware-tested firmware content:

```text
3b4844e8fa5d38d522e1adc70ffac646978f130d
```

All commits after that firmware-bearing SHA must remain documentation-only unless another flash is performed.

## Next bounded milestone after merge

Do not preselect it. Reread the then-current repository, `PORTING_STATUS.md`, merged CHECK_KEY milestone and exact remaining MAP_INTRO legacy behavior before choosing the next coherent boundary.
