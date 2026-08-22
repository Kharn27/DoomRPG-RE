# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for stable build guidance, [`DOCUMENTATION.md`](DOCUMENTATION.md) for the documentation index, and milestone archives for detailed hardware evidence.

## Latest merged hardware baseline

```text
PR   = #55 — native CHECK_KEY dynamic gate
main = 03c4275f2abfd6671c8bf499c075435d7b61ab97
hardware-tested firmware content = 3b4844e8fa5d38d522e1adc70ffac646978f130d
```

Detailed merged evidence: [`MAP1_NATIVE_KEY_GATE.md`](MAP1_NATIVE_KEY_GATE.md).

## Current merge-ready milestone

```text
branch = agent/esp32-map1-native-password-owner
base   = 03c4275f2abfd6671c8bf499c075435d7b61ab97
hardware-tested firmware content = e2d12085712324444f26528b77ea5122c871d85b
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Detailed evidence: [`MAP1_NATIVE_PASSWORD_OWNER.md`](MAP1_NATIVE_PASSWORD_OWNER.md).

The branch supports only `10 / EV_PASSWORD` as a compact two-string-ref pause/continuation owner plus a bounded native submission evaluator. It does not present password UI, mutate legacy DoomCanvas/Game/Hud state, run Game continuation, mutate world/entities/rendering or enter gameplay.

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
 -> fail-closed native state-opcode executor
 -> allocation-free native string spans
 -> compact native UI/player intents
 -> bounded pack-backed one-string reader
 -> explicit native effect/player owners
 -> pure native dynamic gates
 -> bounded native pause/input owners
 -> native event/script loop
 -> explicit native world/render overlays
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
passwordOwnerFNV     = 48f01689
passwordSubmitFNV    = 90e8c574
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

Hardware-proven persistent heap ownership remains:

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

Caller-owned/value types now hardware-proven:

```text
EspMapStatusMessageState  =   8 B
EspMapDialogOwnerState    =  12 B
EspMapNotebookState       = 514 B
EspMapKeyGateResult       =  12 B
EspMapPasswordOwnerState  =  20 B
EspMapPasswordSubmitResult=  12 B
```

PASSWORD copies no password/prompt text into persistent owner state and adds no persistent heap.

## Hardware-proven execution/effect/control boundary

State-only native opcode executor still supports only:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

Hardware-proven families:

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
  refs=1 green/yellow/blue/red=0/1/0/0
  scenarios=16 pass=8 blocked=8 resultBytes=12
  stateExecRefused=1 keyGateFNV=9ace79cd

PASSWORD:
  refs=2 ownerBytes=20 submitResultBytes=12 stateExecRefused=2
  codeBytes=8 promptBytes=72 maxCodeLen=4 resumeExact=2
  correct=2 incorrect=2 emptySemantics=2
  correctResume=2 incorrectNoResume=2
  passwordOwnerFNV=48f01689 passwordSubmitFNV=90e8c574
```

Canonical PASSWORD sample:

```text
cmd17 event6 off6 resume7
arg1=00001d1c arg2=00040100
code=string28@13630+4 codeFNV=92444853
prompt=string29@13636+41 promptFNV=ddbe080a
codeLen=4
```

Recovered submit timing/output proved on hardware:

```text
matched expected length -> 300 ms feedback delay
early submitted shorter input -> 0 ms deliberate delay
correct -> "Correct code!" + resume at source offset + 1
non-empty wrong -> "Invalid code!" + no resume
empty wrong -> no forced message + no resume
guards=10/10
```

PASSWORD fail-closed proof:

```text
unsupported=1 badOffset=1 badDescriptor=1 nullDescriptor=1
nullOwner=1 badOwner=1 tooLong=1 shortBuffer=1
nullSubmitOwner=1 nullSubmitResult=1 closedPack=1
ownerAtomic=yes reset=1
```

## Current tested-build integrity

CHECK_KEY and PASSWORD stages on the hardware-tested PASSWORD firmware both use this stable build baseline:

```text
heap8             = 68748 -> 68748
largest8          = 36852 -> 36852
frameFNV          = 7a95b5b5 -> 7a95b5b5
arenaFNV          = c3882516 -> c3882516
mapStateFNV       = cd99b98e -> cd99b98e
scriptFNV         = f9e3d9df -> f9e3d9df
legacyNotebookFNV = 4d7705c5 -> 4d7705c5
legacyKeys        = 00000000 -> 00000000
hudFNV            = 505b1255 -> 505b1255
```

Additional PASSWORD integrity witnesses:

```text
passwordCanvasFNV = 214171cf -> 214171cf
gamePassCodeStable= yes
legacyPasswordMutation = no
legacyGameContinuationMutation = no
worldMutation = no
framebufferMutation = no
entities=0
monsters=0
noGameplay=yes
```

PASSWORD pack access:

```text
heapOpen           = 64384
transientHeapCost  = 4364 B
largestOpen        = 36852
packIO             = yes
persistentHeapBytes= 0
```

The pack is fully closed/recovered before PARK.

Complete post-PARK heartbeat:

```text
uptime=170149 ms
heap=134512
heap8=68748
largest8=36852
all reported subsystems = ready
```

A later heartbeat at `175150 ms` began with the same heap values but was truncated after `VIDEO=rea`; the preceding complete heartbeat is sufficient steady-state evidence.

Absolute heap/frame values may vary across firmware builds; acceptance is based on exact before/after stability plus unchanged canonical structural fingerprints.

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
 -> real EV_PASSWORD -> two-ref pause owner + bounded submit evaluator
 -> PARK + stable post-PARK heartbeat
```

Still forbidden:

```text
actual DoomCanvas dialog/password presentation
legacy Game continuation mutation by native code
legacy Hud mutation
legacy Player key/notebook mutation
legacy DoomCanvas password mutation
legacy Game.passCode mutation
actual password/input sound playback
full native Game_runEvent execution loop
world/door/line/sprite mutation
map transitions
savegame mutation
entity/monster activation
native gameplay rendering
ST_PLAYING
```

## Remaining MAP_INTRO families

The bounded non-world UI/control families are now exhausted. Still unowned/executed natively:

```text
2  EV_CHANGEMAP
7  EV_SHOW
9  EV_GIVEMAP
13 EV_UNLOCK
15 EV_OPENLINE
16 EV_CLOSELINE
18 EV_HIDE
27 EV_SAVEGAME
```

The next recovery should select the first explicit native world/render overlay from SHOW/HIDE/GIVEMAP/UNLOCK/OPENLINE/CLOSELINE. `EV_CHANGEMAP` and `EV_SAVEGAME` remain larger later boundaries.

## Merge recommendation

**MERGE `agent/esp32-map1-native-password-owner`.**

Hardware-tested firmware content:

```text
e2d12085712324444f26528b77ea5122c871d85b
```

All commits after that firmware-bearing SHA must remain documentation-only unless another firmware is flashed.

## Next bounded milestone after merge

Reread the true new `main`, `PORTING_STATUS.md`, `DOCUMENTATION.md`, merged PASSWORD milestone and exact remaining legacy opcode behavior. Only then select the first bounded world/render overlay family.
