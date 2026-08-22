# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for stable build/flash guidance, [`DOCUMENTATION.md`](DOCUMENTATION.md) for the documentation index, and milestone archives for detailed hardware evidence.

## Latest merged hardware baseline

```text
PR   = #58 — native EV_UNLOCK world state
main = 7503b379185db3f05713eb34f1762173edb977d0
hardware-tested firmware content = e423093c8e17dda1345bebecf721dedf4bbb2002
```

Detailed merged evidence: [`MAP1_NATIVE_UNLOCK_STATE.md`](MAP1_NATIVE_UNLOCK_STATE.md).

## Current merge-ready milestone

```text
branch = agent/esp32-map1-native-givemap-state
base   = 7503b379185db3f05713eb34f1762173edb977d0
hardware-tested firmware content = 2e0f8f5de93f806380ee254a8dab59a817c73f5d
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Detailed evidence: [`MAP1_NATIVE_GIVEMAP_STATE.md`](MAP1_NATIVE_GIVEMAP_STATE.md).

The milestone owns only `9 / EV_GIVEMAP`. It adds a packed 103-byte line/sprite automap-reveal owner and reuses the existing 1024-byte `EspMapState` for `BIT_AM_VISITED` on entrance cells. Legacy Render/Entity state remains untouched.

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
DoomRPG-RE desktop/J2ME = executable behavior/format reference only
final engine             = ESP32-native ownership
```

Current native direction:

```text
native BSP/pack
 -> compact immutable EspMapRuntime
 -> compact tile/script state
 -> event lookup + descriptors
 -> native opcode/effect owners
 -> compact native world overlays
 -> native event/script loop
 -> native gameplay/effect consumers
 -> ESP32-native renderer/gameplay
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
automapStateFNV      = 669b1aa7
giveMapFNV           = 98c7ac59
giveMapMutatedMapFNV = e21edbce
giveMapMutatedAutoFNV= 9d03ca2d
```

## Persistent native RAM ownership

Hardware-proven persistent heap:

```text
immutable arena         = 14112 B actual heap
mutable tile state      =  1040 B actual heap
mutable script state    =   100 B actual heap
mutable line state      =   136 B actual heap
mutable texture state   =    76 B actual heap
mutable automap state   =   120 B actual heap (103 B payload + 17 B allocator)
------------------------------------------
current proven total    = 15584 B
```

Hardware-proven caller/value ABIs:

```text
EspMapStatusMessageState    =   8 B
EspMapDialogOwnerState      =  12 B
EspMapNotebookState         = 514 B
EspMapKeyGateResult         =  12 B
EspMapPasswordOwnerState    =  20 B
EspMapPasswordSubmitResult  =  12 B
EspMapLineDoorResult        =  16 B
EspMapLineUnlockResult      =  20 B
EspMapGiveMapResult         =  20 B
```

## Hardware-proven world owners

Line OPEN/LOCK state:

```text
480 lines
openBits=60 B lockedBits=60 B
payload=120 B / actual heap=136 B
initialOpen=0 initialLocked=7
lineStateFNV=e5e74861
```

Line texture 9/10 state:

```text
payload=60 B / actual heap=76 B
variants=6 initialTexture10=0
lineTextureStateFNV=f1fc1875
```

Automap state:

```text
480 line reveal bits   = 60 B
344 sprite reveal bits = 43 B
payload                = 103 B
actual heap            = 120 B
initialLineRevealed    = 0
initialSpriteRevealed  = 0
automapStateFNV        = 669b1aa7
```

`EspMapState` remains 1024 bytes; GIVEMAP adds no duplicate tile storage.

## Hardware-proven opcode/effect families

State-only executor remains deliberately limited to:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

Dedicated proven families include:

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

GIVEMAP:
  refs=1 mutated=1 noMutation=0 removable=0 stateExecRefused=1
  resultBytes=20
  lineTargets=430 spriteTargets=344 entranceTargets=4
  lineMutTotal=430 spriteMutTotal=344 tileMutTotal=4
  giveMapFNV=98c7ac59
  rollback=1/1 idempotentHandled=1
```

Canonical real GIVEMAP:

```text
cmd43 event14 off1
lines 430 mutated
sprites 344 mutated
entrance tiles 4 mutated
handled=1 removeIfHandled=0
automapStateFNV 669b1aa7 -> 9d03ca2d -> rollback 669b1aa7
mapStateFNV     cd99b98e -> e21edbce -> rollback cd99b98e
```

## Latest tested-build integrity

Same-build allocations before GIVEMAP:

```text
line state:    cost 136 B
texture state: cost  76 B
```

GIVEMAP allocation:

```text
heap8             = 68384 -> 68264
persistentHeapCost= 120 B
payload           = 103 B
allocatorOverhead = 17 B
largest8          = 34804 -> 34804
frameFNV          = 453f0d5c -> 453f0d5c
```

Post-audit integrity:

```text
arenaFNV            = c3882516 -> c3882516
mapStateFNV         = cd99b98e -> cd99b98e
scriptFNV           = f9e3d9df -> f9e3d9df
lineStateFNV        = e5e74861
lineTextureStateFNV = f1fc1875
legacyNotebookFNV   = 4d7705c5 -> 4d7705c5
legacyKeys          = 00000000 -> 00000000
hudFNV              = 505b1255 -> 505b1255
passwordCanvasFNV   = 214171cf -> 214171cf
continuationFNV     = e2ba14a5 -> e2ba14a5
packIO              = no
legacyRuntimeClear  = yes
entities=0 monsters=0 noGameplay=yes
```

Stable PARK heartbeats:

```text
25135 ms: heap=134028 heap8=68264 largest8=34804 all reported subsystems ready
30136 ms: heap=134028 heap8=68264 largest8=34804 all reported subsystems ready
35137 ms: heap=134028 heap8=68264 largest8=34804 all reported subsystems ready
```

Absolute heap/frame values may differ between firmware builds. Hardware acceptance uses same-build stability plus exact canonical fingerprints.

## GIVEMAP fail-closed / atomicity proof

```text
notReady=1
unsupported=1
badOffset=1
badDescriptor=1
nullDescriptor=1
nullResult=1
badLineIndex=1
badSpriteIndex=1
badRevealValue=1
badVisitedIndex=1
badVisitedValue=1
stateAtomic=yes
worldRestored=yes
```

Final PARK:

```text
nativeAutomapState=yes
nativeGiveMapExec=yes
storageBytes=103
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
 -> packed line OPEN/LOCK state
 -> OPENLINE/CLOSELINE mutation
 -> packed texture-9/10 state
 -> EV_UNLOCK mutation
 -> packed line/sprite automap state
 -> EV_GIVEMAP automap + tile VISITED mutation
 -> PARK + stable heartbeat
```

Still forbidden:

```text
legacy Render line/sprite mutation
legacy Entity/EntityDef mutation
actual door/lock sound playback
actual view refresh from deferred effects
full native Game_runEvent loop
sprite SHOW/HIDE mutation
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
18 EV_HIDE
27 EV_SAVEGAME
```

SHOW/HIDE remain intentionally deferred until a compact native entity/topology owner exists; legacy behavior is not sprite-visibility-only. CHANGEMAP and SAVEGAME remain larger ownership boundaries.

## Merge recommendation

**MERGE `agent/esp32-map1-native-givemap-state`.**

Hardware-tested firmware content:

```text
2e0f8f5de93f806380ee254a8dab59a817c73f5d
```

All later commits must remain documentation-only unless another firmware is flashed.

After merge, recover the true new `main`, reread this file, `DOCUMENTATION.md`, the merged GIVEMAP archive and exact legacy behavior before selecting the next family.