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

## Current candidate

```text
branch = agent/esp32-map1-native-givemap-state
base   = 7503b379185db3f05713eb34f1762173edb977d0
firmware candidate content = 2e0f8f5de93f806380ee254a8dab59a817c73f5d
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

Detailed active milestone: [`MAP1_NATIVE_GIVEMAP_STATE.md`](MAP1_NATIVE_GIVEMAP_STATE.md).

The candidate owns only `9 / EV_GIVEMAP`. It adds a packed 103-byte line/sprite automap-reveal owner and reuses the existing 1024-byte `EspMapState` for `BIT_AM_VISITED` on entrance cells. It does not instantiate entities, mutate legacy Render objects, render gameplay or enter `ST_PLAYING`.

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

## Hardware-proven fingerprints through PR #58

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

Current GIVEMAP candidate will establish:

```text
automapStateFNV        = pending
giveMapFNV             = pending
mutatedAutomapStateFNV = pending
mutatedMapStateFNV     = pending
```

## Persistent native RAM ownership

Hardware-proven persistent heap entering this candidate:

```text
immutable arena        = 14112 B actual heap
mutable tile state     =  1040 B actual heap
mutable script state   =   100 B actual heap
mutable line state     =   136 B actual heap
mutable texture state  =    76 B actual heap
-----------------------------------------
proven total           = 15464 B
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
```

Current candidate target:

```text
line reveal bitset      = 60 B
sprite reveal bitset    = 43 B
EspMapAutomapState      = 103 B payload
EspMapGiveMapResult     = 20 B expected caller-local
actual incremental heap = hardware pending; accepted 103..167 B
```

`EspMapState` remains 1024 bytes. The candidate adds only `EspMapState_setVisited()` and no new tile allocation.

## Hardware-proven current world owners

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

OPEN/CLOSE hardware corpus:

```text
refs=71 open=39 close=32
mutated=29 locked=18 alreadyTarget=24 removable=12
resultBytes=16 lineDoorFNV=b1c9d297
rollback=29/29 idempotent=1 lockedGuard=1
```

UNLOCK hardware corpus:

```text
refs=6 mutated=6 lockMutated=6 textureMutated=6
noMutation=0 removable=0 stateExecRefused=6
resultBytes=20 unlockFNV=261d756a
rollback=6/6 idempotentHandled=1
```

## Latest tested-build integrity

UNLOCK firmware:

```text
line-state allocation:
  heap8 68652 -> 68516
  cost=136 B payload=120 B overhead=16 B

texture-state allocation:
  heap8 68516 -> 68440
  cost=76 B payload=60 B overhead=16 B

largest8 = 34804 stable
frameFNV = 64347226 stable
```

Inherited fingerprints stayed exact:

```text
arenaFNV          c3882516
mapStateFNV       cd99b98e
scriptFNV         f9e3d9df
lineStateFNV      e5e74861
lineTextureStateFNV f1fc1875
legacyNotebookFNV 4d7705c5
packIO=no
legacyRuntimeClear=yes
entities=0 monsters=0 noGameplay=yes
```

Stable PARK heartbeats:

```text
25201 ms: heap=134204 heap8=68440 largest8=34804 all reported subsystems ready
30202 ms: heap=134204 heap8=68440 largest8=34804 all reported subsystems ready
```

Absolute heap/frame values may differ between firmware builds. Hardware acceptance uses same-build stability plus canonical state fingerprints.

## Current GIVEMAP recovered contract

Legacy behavior:

```text
for each line:
  if !(flags & 0x20): flags |= 0x80
for every map sprite:
  info |= 0x10000000
for each map tile:
  if BIT_AM_ENTRANCE: set BIT_AM_VISITED
return handled=true
```

Native ownership:

```text
line reveal bits   -> new packed owner
sprite reveal bits -> new packed owner
VISITED tile bits  -> existing EspMapState
```

Repeated valid GIVEMAP remains handled even when all mutation counts become zero. `removeCommandIfHandled` mirrors source `arg2 & 0x200`; `EspMapScriptState` remains unchanged by this owner.

## Current real-CYD validation target

The probe discovers rather than guesses:

```text
initial line/sprite reveal counts
automapStateFNV
line/sprite/entrance target counts
real GIVEMAP ref count
mutated/no-mutation refs
remove-if-handled refs
line/sprite/tile mutation totals
giveMapFNV
first mutated automap FNV
first mutated map-state FNV
actual 103-byte-owner heap cost
new-build heap/frame absolute values
```

Acceptance requires:

```text
refs > 0
mutatedRefs > 0
mutated + noMutation = refs
stateExecRefused = refs
resultBytes = 20
rollbackProofs = mutatedRefs
idempotentHandled = 1
```

Fail-closed target:

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

Final integrity must restore:

```text
mapStateFNV          = cd99b98e
automap state        = initial hardware FNV
lineStateFNV         = e5e74861
lineTextureStateFNV  = f1fc1875
arena/script/legacy witnesses unchanged
pack closed
legacy Render runtime clear
entities=0 monsters=0
ST_PLAYING not reached
```

## Remaining MAP_INTRO families after current candidate

If GIVEMAP passes, still unowned:

```text
2  EV_CHANGEMAP
7  EV_SHOW
18 EV_HIDE
27 EV_SAVEGAME
```

SHOW/HIDE remain intentionally deferred until a compact native entity/topology owner exists; their legacy behavior is not sprite-visibility-only.

## Validation target

Build/flash normal optimized:

```text
esp32-cyd
```

from `agent/esp32-map1-native-givemap-state` and capture `[MAPAUTOMAP]`, `[MAPGIVEMAP]`, `[MAPGIVEMAPPROBE]` plus a stable `[ALIVE]` heartbeat.

Firmware candidate:

```text
2e0f8f5de93f806380ee254a8dab59a817c73f5d
```

No CI status is published. The local execution container could not resolve GitHub, so no local build result exists; this is not a compile failure. Do not mark merge-ready until the real classic CYD supplies the PASS.
