# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for stable build guidance, [`DOCUMENTATION.md`](DOCUMENTATION.md) for the documentation index, and milestone archives for detailed hardware evidence.

## Latest merged hardware baseline

```text
PR   = #56 — native PASSWORD pause owner
main = 3c113cc047aeb613f2ba4ab7905e92487c796f80
hardware-tested firmware content = e2d12085712324444f26528b77ea5122c871d85b
```

Detailed merged evidence: [`MAP1_NATIVE_PASSWORD_OWNER.md`](MAP1_NATIVE_PASSWORD_OWNER.md).

## Current merge-ready milestone

```text
branch = agent/esp32-map1-native-line-door-state
base   = 3c113cc047aeb613f2ba4ab7905e92487c796f80
hardware-tested firmware content = 376f45bcdd12264d3cba1ee83e7197a52e248210
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Detailed evidence: [`MAP1_NATIVE_LINE_DOOR_STATE.md`](MAP1_NATIVE_LINE_DOOR_STATE.md).

This is the first explicit hardware-proven mutable-world owner. It keeps a packed OPEN/LOCKED state for the immutable 480-line runtime and executes only real `15 / EV_OPENLINE` and `16 / EV_CLOSELINE` open-bit transitions. Door animation, collision-entity synchronization, sound and command-removal stay deferred metadata; legacy Render/Game/Entity state remains untouched.

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
 -> bounded pack-backed string reader
 -> explicit native effect/player owners
 -> pure dynamic gates
 -> bounded native pause/input owners
 -> compact mutable native world overlays
 -> native event/script loop
 -> native gameplay/effect consumers
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
lineStateFNV         = e5e74861
lineDoorFNV          = b1c9d297
lineMutatedFNV       = 8f57d779
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

Hardware-proven persistent heap:

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

Current line-world owner:

```text
lineCount      = 480
openBits       = 60 B
lockedBits     = 60 B
storage payload= 120 B
actual heap    = 136 B
initialOpen    = 0
initialLocked  = 7
lineStateFNV   = e5e74861
```

## Hardware-proven execution/effect/control boundary

State-only native opcode executor still supports only:

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

OPENLINE/CLOSELINE:
  refs=71 open=39 close=32
  mutated=29 locked=18 alreadyTarget=24 removable=12
  stateExecRefused=71 resultBytes=16
  lineStateFNV=e5e74861 lineDoorFNV=b1c9d297 mutatedFNV=8f57d779
  rollback=29/29 idempotent=1 lockedGuard=1
```

Canonical first real OPEN/CLOSE success:

```text
cmd3 event1 off2 line459 opcode15 / EV_OPENLINE
open=0->1 locked=0 sound=5063 effects=07 removeIfHandled=0
```

## First hardware-proven native world mutation

The real CYD proved this complete native world transition:

```text
real MAP_INTRO bytecode
 -> canonical event/command linkage
 -> EspMapLineState line 459
 -> open bit 0 -> 1
 -> line state FNV e5e74861 -> 8f57d779
 -> exact rollback to e5e74861
```

Across the complete real corpus:

```text
29 successful mutations
29 exact rollbacks
18 blocked by native locked state
24 already in requested state
71 total refs accounted exactly
```

Door animation, entity relink and sound are returned only as deferred effect metadata. Legacy Render/Entity/Game world objects remain untouched.

`arg2 & 0x200` is exposed as `removeCommandIfHandled`; `EspMapScriptState` is not mutated by this owner. The future native event loop must consume that outer `Game_runEvent()` behavior.

## Current tested-build integrity

Pre-line-state stages in the hardware-tested firmware were stable at:

```text
heap8       = 68700
largest8    = 36852
frameFNV    = 5a979d01
```

Line-state construction then intentionally adds permanent world heap:

```text
heap8             = 68700 -> 68564
persistentHeapCost= 136 B
payload           = 120 B
allocatorOverhead = 16 B
largest8          = 36852 -> 36852
```

After all OPEN/CLOSE mutations and rollbacks:

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
```

PASSWORD transient pack access on this firmware was also bounded and fully recovered:

```text
heapOpen          = 64336
transientHeapCost = 4364 B
largestOpen       = 36852
```

Stable post-PARK heartbeats:

```text
uptime=30084 ms heap=134328 heap8=68564 largest8=36852 all reported subsystems ready
uptime=35085 ms heap=134328 heap8=68564 largest8=36852 all reported subsystems ready
```

## Fail-closed / atomicity proof for line world state

```text
notReady=1
unsupported=1
badOffset=1
badDescriptor=1
nullDescriptor=1
nullResult=1
badOpenIndex=1
badLockedIndex=1
stateAtomic=yes
worldRestored=yes
```

Final PARK:

```text
nativeLineState=yes
nativeDoorExec=yes
storageBytes=120
resultBytes=16
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
 -> real EV_PASSWORD -> pause owner + bounded submit evaluator
 -> packed native line world state
 -> real EV_OPENLINE/EV_CLOSELINE world mutation + rollback
 -> PARK + stable heartbeat
```

Still forbidden:

```text
legacy Render line mutation
legacy Entity link/unlink
actual door animation
actual sound playback
EV_UNLOCK texture/entity mutation
full native Game_runEvent execution loop
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
13 EV_UNLOCK
18 EV_HIDE
27 EV_SAVEGAME
```

## Merge recommendation

**MERGE `agent/esp32-map1-native-line-door-state`.**

Hardware-tested firmware content:

```text
376f45bcdd12264d3cba1ee83e7197a52e248210
```

All later commits must remain documentation-only unless another firmware is flashed.

## Next bounded milestone after merge

Reread the true new `main`, this recovery point, `DOCUMENTATION.md`, the merged line-door milestone and exact remaining legacy behavior before selecting the next family. `EV_UNLOCK` is an adjacent candidate because lock state is now natively owned, but its texture/entity-definition semantics must be recovered explicitly before implementation.
