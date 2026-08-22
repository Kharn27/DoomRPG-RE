# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for stable build/flash guidance, [`DOCUMENTATION.md`](DOCUMENTATION.md) for the documentation index, and milestone archives for detailed evidence.

## Latest merged hardware baseline

```text
PR   = #57 — native OPEN/CLOSE line world state
main = e4fb32f41b7074bbb433e64f4c824edb2167cf50
hardware-tested firmware content = 376f45bcdd12264d3cba1ee83e7197a52e248210
```

Detailed merged evidence: [`MAP1_NATIVE_LINE_DOOR_STATE.md`](MAP1_NATIVE_LINE_DOOR_STATE.md).

## Current merge-ready milestone

```text
branch = agent/esp32-map1-native-unlock-state
base   = e4fb32f41b7074bbb433e64f4c824edb2167cf50
hardware-tested firmware content = e423093c8e17dda1345bebecf721dedf4bbb2002
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Detailed evidence: [`MAP1_NATIVE_UNLOCK_STATE.md`](MAP1_NATIVE_UNLOCK_STATE.md).

The milestone supports only `13 / EV_UNLOCK`. It preserves the hardware-proven OPEN/LOCKED line owner unchanged and adds a packed 60-byte texture-9/10 world overlay. Valid UNLOCK always reports handled, clears the native lock bit and conditionally changes effective texture 9->10. Sound, special-entity definition synchronization and view refresh remain deferred metadata; legacy Render/Game/Entity state stays untouched.

## Permanent target / invariants

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

Permanent constraints:

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
 -> event lookup + descriptor/linkage
 -> mutable EspMapScriptState
 -> event filtering + fail-closed opcode owners
 -> strings/UI/effect owners
 -> dynamic gates + pause/input owners
 -> compact mutable native world overlays
 -> native event/script loop
 -> native gameplay/effect consumers
 -> ESP32-native gameplay + renderer
```

## Hardware-proven MAP_INTRO identity

```text
resource       = /intro.bsp / Entrance
source bytes   = 21823
CRC32          = 623f34e4
nodes          = 223
lines          = 480
mapSprites     = 344
events         = 93
byteCodes      = 265
strings        = 94
stringData     = 7779 B
maxString      = 313 B
```

Real opcode IDs:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
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
lineStateFNV         = e5e74861
lineDoorFNV          = b1c9d297
lineMutatedFNV       = 8f57d779
lineTextureStateFNV  = f1fc1875
unlockFNV            = 261d756a
unlockMutatedLineFNV = 8d5f89d8
unlockMutatedTexFNV  = 997459ec
```

## Persistent native RAM ownership

Hardware-proven persistent heap:

```text
immutable arena       = 14112 B actual heap
mutable tile state    =  1040 B actual heap
mutable script state  =   100 B actual heap
mutable line state    =   136 B actual heap (120 B payload + 16 B allocator)
mutable texture state =    76 B actual heap ( 60 B payload + 16 B allocator)
-----------------------------------------
current proven total  = 15464 B
```

Hardware-proven value/caller-local types:

```text
EspMapStatusMessageState    =   8 B
EspMapDialogOwnerState      =  12 B
EspMapNotebookState         = 514 B
EspMapKeyGateResult         =  12 B
EspMapPasswordOwnerState    =  20 B
EspMapPasswordSubmitResult  =  12 B
EspMapLineDoorResult        =  16 B
EspMapLineUnlockResult      =  20 B
```

Current line-world owners:

```text
lineCount        = 480
openBits         = 60 B
lockedBits       = 60 B
line storage     = 120 B payload / 136 B actual heap
initialOpen      = 0
initialLocked    = 7
lineStateFNV     = e5e74861

texture9/10 bits = 60 B payload / 76 B actual heap
variantCount     = 6
initialTexture10 = 0
textureStateFNV  = f1fc1875
```

## Hardware-proven opcode/effect families

State-only executor remains deliberately limited to:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

Dedicated hardware-proven families now include:

```text
FORCEMESSAGE:
  refs=3 set=1 clear=2 ownerBytes=8 statusApplyFNV=52b25a5f

DIALOG/NOBACK:
  refs=84 back=76 noBack=8 pause=84 skipTurn=84 resumeExact=84
  ownerBytes=12 dialogApplyFNV=d0254f3d

NOTE:
  refs=7 sourceBytes=256 finalLen=270 ownerBytes=514
  noteApplyFNV=43183162 contentFNV=599609e0 storageFNV=75cf54e0

CHECK_KEY:
  refs=1 scenarios=16 pass=8 blocked=8 resultBytes=12
  keyGateFNV=9ace79cd

PASSWORD:
  refs=2 ownerBytes=20 submitResultBytes=12
  correct=2 incorrect=2 emptySemantics=2
  passwordOwnerFNV=48f01689 passwordSubmitFNV=90e8c574

OPENLINE/CLOSELINE:
  refs=71 open=39 close=32
  mutated=29 locked=18 alreadyTarget=24 removable=12
  resultBytes=16 lineDoorFNV=b1c9d297
  rollback=29/29 idempotent=1 lockedGuard=1

UNLOCK:
  refs=6 mutated=6 lockMutated=6 textureMutated=6
  noMutation=0 removable=0 stateExecRefused=6
  resultBytes=20 unlockFNV=261d756a
  rollback=6/6 idempotentHandled=1
```

Canonical first OPEN/CLOSE world mutation:

```text
cmd3 event1 off2 line459 opcode15 / EV_OPENLINE
open=0->1 locked=0 sound=5063 effects=07 removeIfHandled=0
lineStateFNV e5e74861 -> 8f57d779 -> rollback e5e74861
```

Canonical first UNLOCK world mutation:

```text
cmd18 event6 off7 line400 opcode13 / EV_UNLOCK
locked=1->0 texture=9->10
lockMutated=1 textureMutated=1
sound=5067 effects=07 handled=1 removeIfHandled=0
lineStateFNV    e5e74861 -> 8d5f89d8 -> rollback e5e74861
textureStateFNV f1fc1875 -> 997459ec -> rollback f1fc1875
```

## Current tested-build integrity

The OPEN/CLOSE stage in the UNLOCK firmware remained semantically canonical. Its same-build allocation witness was:

```text
heap8             = 68652 -> 68516
persistentHeapCost= 136 B
payload           = 120 B
allocatorOverhead = 16 B
largest8          = 34804 -> 34804
frameFNV          = 64347226 -> 64347226
```

UNLOCK then added its permanent texture owner:

```text
heap8             = 68516 -> 68440
persistentHeapCost= 76 B
payload           = 60 B
allocatorOverhead = 16 B
largest8          = 34804 -> 34804
```

Post-UNLOCK audit integrity:

```text
frameFNV          = 64347226 -> 64347226
arenaFNV          = c3882516 -> c3882516
mapStateFNV       = cd99b98e -> cd99b98e
scriptFNV         = f9e3d9df -> f9e3d9df
lineStateFNV      = e5e74861 -> e5e74861
legacyNotebookFNV = 4d7705c5 -> 4d7705c5
legacyKeys        = 00000000 -> 00000000
hudFNV            = 505b1255 -> 505b1255
passwordCanvasFNV = 214171cf -> 214171cf
continuationFNV   = e2ba14a5 -> e2ba14a5
packIO            = no
legacyRuntimeClear= yes
entities=0 monsters=0 noGameplay=yes
```

Stable PARK heartbeats:

```text
25201 ms: heap=134204 heap8=68440 largest8=34804 all reported subsystems ready
30202 ms: heap=134204 heap8=68440 largest8=34804 all reported subsystems ready
```

Absolute heap/frame values may differ between firmware builds. Hardware acceptance uses same-build stability plus exact canonical native-state fingerprints and bounded owner allocations.

## EV_UNLOCK hardware proof

Real texture topology:

```text
variants         = 6
initialTexture10 = 0
textureStateFNV  = f1fc1875
```

Complete corpus:

```text
refs             = 6
mutated          = 6
lockMutated      = 6
textureMutated   = 6
noMutation       = 0
removable        = 0
stateExecRefused = 6
unlockFNV        = 261d756a
elapsed          = 9 ms
```

Every real UNLOCK target begins locked and at texture 9, so all six real commands atomically mutate both world owners.

Repeated handled proof:

```text
first apply  -> handled=1 + mutation
second apply -> handled=1, no mutation, no sound/effects
idempotentHandled=1
```

Fail-closed / atomicity proof:

```text
notReady=1
unsupported=1
badOffset=1
badDescriptor=1
nullDescriptor=1
nullResult=1
badTextureIndex=1
badTextureValue=1
nonVariant=1
stateAtomic=yes
worldRestored=yes
```

Final PARK:

```text
nativeLineState=yes
nativeDoorExec=yes
nativeLineTextureState=yes
nativeUnlockExec=yes
textureStorageBytes=60
resultBytes=20
worldMutationProven=yes
worldRestored=yes
legacyWorldMutation=no
framebufferMutation=no
entities=0
monsters=0
noGameplay=yes
```

## Current hardware-proven execution path

```text
validated intro disposal
 -> native BSP inventory
 -> compact immutable runtime
 -> mutable tile/script state
 -> events + descriptors + filtering
 -> state-opcode execution/rollback
 -> strings/UI/effect owners
 -> CHECK_KEY
 -> PASSWORD
 -> packed native line OPEN/LOCKED state
 -> real OPENLINE/CLOSELINE world mutation + rollback
 -> packed native texture-9/10 state
 -> real EV_UNLOCK two-owner world mutation + rollback
 -> PARK + stable heartbeat
```

Still forbidden:

```text
legacy Render line mutation
legacy Entity/EntityDef mutation
actual door/lock sound playback
actual view refresh from world effects
EV_LOCK / EV_TOGGLELOCK
full native Game_runEvent loop
sprite SHOW/HIDE mutation
GIVEMAP automap mutation
map transitions
savegame mutation
entity/monster activation
native gameplay rendering
ST_PLAYING
```

## Remaining MAP_INTRO families

Still unowned:

```text
2  EV_CHANGEMAP
7  EV_SHOW
9  EV_GIVEMAP
18 EV_HIDE
27 EV_SAVEGAME
```

## Merge recommendation

**MERGE `agent/esp32-map1-native-unlock-state`.**

Hardware-tested firmware content:

```text
e423093c8e17dda1345bebecf721dedf4bbb2002
```

All later commits must remain documentation-only unless another firmware is flashed.

## Next bounded milestone after merge

Reread the true new `main`, this recovery point, `DOCUMENTATION.md`, the merged UNLOCK archive and exact remaining legacy behavior before selecting the next family. Do not pre-authorize SHOW/HIDE/GIVEMAP/CHANGEMAP/SAVEGAME from conversation memory.
