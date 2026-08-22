# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for stable build guidance, [`DOCUMENTATION.md`](DOCUMENTATION.md) for the documentation index, and milestone archives for detailed evidence.

## Latest merged hardware baseline

```text
PR   = #57 — native OPEN/CLOSE line world state
main = e4fb32f41b7074bbb433e64f4c824edb2167cf50
hardware-tested firmware content = 376f45bcdd12264d3cba1ee83e7197a52e248210
```

Detailed merged evidence: [`MAP1_NATIVE_LINE_DOOR_STATE.md`](MAP1_NATIVE_LINE_DOOR_STATE.md).

## Current candidate

```text
branch = agent/esp32-map1-native-unlock-state
base   = e4fb32f41b7074bbb433e64f4c824edb2167cf50
firmware candidate content = e423093c8e17dda1345bebecf721dedf4bbb2002
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

Detailed active milestone: [`MAP1_NATIVE_UNLOCK_STATE.md`](MAP1_NATIVE_UNLOCK_STATE.md).

The candidate supports only `13 / EV_UNLOCK`. It preserves the hardware-proven 120-byte OPEN/LOCKED line owner unchanged and adds one packed 60-byte 9/10 door-texture overlay. Valid UNLOCK clears the native lock bit and, only when current texture is 9, changes native texture 9->10 and returns deferred sound/special-entity/view effects. Legacy Render/Game/Entity state remains untouched.

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

Native direction:

```text
BSP source in native pack
 -> compact immutable EspMapRuntime
 -> allocation-free accessors
 -> mutable EspMapState
 -> tile/event lookup
 -> descriptor + bytecode linkage
 -> mutable EspMapScriptState
 -> side-effect-free event filter
 -> fail-closed state-opcode executor
 -> native strings/intents/effect owners
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

Hardware-proven fingerprints:

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
```

Current UNLOCK candidate will establish new hardware canons:

```text
lineTextureStateFNV = pending
unlockFNV           = pending
sample mutated line FNV    = pending
sample mutated texture FNV = pending
```

## Persistent native RAM ownership

Hardware-proven entering this candidate:

```text
immutable arena       = 14112 B actual heap
mutable tile state    =  1040 B actual heap
mutable script state  =   100 B actual heap
mutable line state    =   136 B actual heap (120 B payload + 16 B allocator)
-----------------------------------------
current proven total  = 15388 B
largest8              = 36852 preserved
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
```

Current UNLOCK candidate target:

```text
EspMapLineTextureState payload = 60 B
EspMapLineUnlockResult          = 20 B expected
incremental persistent heap     = hardware pending, accepted 60..124 B
```

The existing line owner must remain structurally unchanged:

```text
lineCount      = 480
openBits       = 60 B
lockedBits     = 60 B
storage        = 120 B
initialOpen    = 0
initialLocked  = 7
lineStateFNV   = e5e74861
actual heap    = 136 B
```

## Hardware-proven execution/effect/control boundary

State-only executor still supports only:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

Dedicated hardware-proven families:

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
  refs=1 green/yellow/blue/red=0/1/0/0
  scenarios=16 pass=8 blocked=8 resultBytes=12
  stateExecRefused=1 keyGateFNV=9ace79cd

PASSWORD:
  refs=2 ownerBytes=20 submitResultBytes=12 stateExecRefused=2
  codeBytes=8 promptBytes=72 maxCodeLen=4 resumeExact=2
  correct=2 incorrect=2 emptySemantics=2
  correctResume=2 incorrectNoResume=2
  passwordOwnerFNV=48f01689 passwordSubmitFNV=90e8c574

OPENLINE/CLOSELINE:
  refs=71 open=39 close=32
  mutated=29 locked=18 alreadyTarget=24 removable=12
  stateExecRefused=71 resultBytes=16
  lineStateFNV=e5e74861 lineDoorFNV=b1c9d297 mutatedFNV=8f57d779
  rollback=29/29 idempotent=1 lockedGuard=1
```

First hardware-proven native world mutation:

```text
cmd3 event1 off2 line459 opcode15 / EV_OPENLINE
open=0->1 locked=0 sound=5063 effects=07 removeIfHandled=0
lineStateFNV e5e74861 -> 8f57d779 -> exact rollback e5e74861
```

## Latest tested-build integrity

OPEN/CLOSE firmware before line-state allocation:

```text
heap8       = 68700
largest8    = 36852
frameFNV    = 5a979d01
```

After permanent line state:

```text
heap8             = 68700 -> 68564
persistentHeapCost= 136 B
payload           = 120 B
allocatorOverhead = 16 B
largest8          = 36852 -> 36852
```

Post-audit integrity:

```text
frameFNV          = 5a979d01 -> 5a979d01
arenaFNV          = c3882516 -> c3882516
mapStateFNV       = cd99b98e -> cd99b98e
scriptFNV         = f9e3d9df -> f9e3d9df
legacyNotebookFNV = 4d7705c5 -> 4d7705c5
legacyKeys        = 00000000 -> 00000000
hudFNV            = 505b1255 -> 505b1255
passwordCanvasFNV = 214171cf -> 214171cf
continuationFNV   = e2ba14a5 -> e2ba14a5
packIO            = no
legacyRuntimeClear= yes
entities=0 monsters=0 noGameplay=yes
```

Stable heartbeats:

```text
30084 ms: heap=134328 heap8=68564 largest8=36852 all reported subsystems ready
35085 ms: heap=134328 heap8=68564 largest8=36852 all reported subsystems ready
```

## Current EV_UNLOCK contract

Recovered legacy setup:

```text
Game_setLineLocked(lineIndex, false, false)
```

Permanent native semantics:

```text
valid command:
  legacyReturnValue = 1 always
  lockedAfter = 0
  lockMutated = lockedBefore != 0

if current effective texture == 9:
  texture 9 -> 10
  textureMutated=1
  soundId=5067
  deferred effects:
    PLAY_SOUND
    SPECIAL_ENTITY_DEF_SYNC subtype 2
    REFRESH_IF_ENTITY
else:
  texture unchanged
  textureMutated=0
  sound/effects=none

removeCommandIfHandled = source arg2 & 0x200
```

The new texture owner is a single 480-bit bitset. For source texture 9/10, bit 0/1 represents effective texture 9/10; all other textures remain immutable source values.

The 120-byte OPEN/LOCKED owner is not resized or reformatted.

## Current real-CYD validation target

The probe discovers the real UNLOCK corpus rather than predeclaring counts.

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

Hardware will establish:

```text
texture-variant line count
initial texture-10 count
lineTextureStateFNV
UNLOCK refs
mutated / lockMutated / textureMutated / noMutation counts
removable handled refs
first real mutated sample
unlockFNV
sample mutated line FNV
sample mutated texture FNV
incremental persistent heap cost + allocator overhead
new-build heap/frame absolute values
```

Repeated UNLOCK proof:

```text
first real mutating apply -> handled=1 + mutation
second apply without rollback -> handled=1, no mutation, no sound/effects
removeIfHandled unchanged
then exact rollback of both world owners
```

Fail-closed target:

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

Integrity must preserve:

```text
arenaFNV          c3882516
mapStateFNV       cd99b98e
scriptFNV         f9e3d9df
lineStateFNV      e5e74861 after rollback
legacyNotebookFNV 4d7705c5
legacy Player.keys
Hud witness
DoomCanvas password witness
Game continuation witness
framebuffer
pack closed
legacy Render runtime clear
entities=0
monsters=0
ST_PLAYING not reached
```

The new 60-byte texture owner remains allocated at PARK but must be restored to its initial content fingerprint.

## Current execution path

```text
validated intro disposal
 -> native BSP inventory
 -> compact resident arena
 -> allocation-free access
 -> mutable tile state
 -> tile/event lookup
 -> descriptor/linkage
 -> compact script state
 -> event filter
 -> state-opcode execution/rollback
 -> strings/UI/effect owners
 -> CHECK_KEY gate
 -> PASSWORD pause/submit owner
 -> packed line OPEN/LOCKED state
 -> OPENLINE/CLOSELINE world mutation + rollback
 -> candidate: packed 9/10 line-texture state + EV_UNLOCK atomic world mutation + rollback
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

## Remaining MAP_INTRO families after current candidate

If UNLOCK passes:

```text
2  EV_CHANGEMAP
7  EV_SHOW
9  EV_GIVEMAP
18 EV_HIDE
27 EV_SAVEGAME
```

Do not pre-authorize the next family. After PASS + merge, recover the then-current repository and exact remaining legacy semantics.

## Validation target

Build/flash normal optimized:

```text
esp32-cyd
```

from `agent/esp32-map1-native-unlock-state` and capture `[MAPLINETEX]`, `[MAPUNLOCK]`, `[MAPUNLOCKPROBE]` plus a later stable `[ALIVE]` heartbeat.

Firmware candidate:

```text
e423093c8e17dda1345bebecf721dedf4bbb2002
```

No CI status is published for this SHA. Do not mark merge-ready until the real classic CYD supplies the PASS.
