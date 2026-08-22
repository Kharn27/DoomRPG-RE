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

## Current candidate

```text
branch = agent/esp32-map1-native-line-door-state
base   = 3c113cc047aeb613f2ba4ab7905e92487c796f80
firmware candidate content = 376f45bcdd12264d3cba1ee83e7197a52e248210
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

Detailed active milestone: [`MAP1_NATIVE_LINE_DOOR_STATE.md`](MAP1_NATIVE_LINE_DOOR_STATE.md).

The candidate is the first explicit native mutable-world owner. It adds a 2-bit-per-line packed OPEN/LOCKED overlay over the immutable 480-line runtime and executes only real `15 / EV_OPENLINE` and `16 / EV_CLOSELINE` open-bit transitions. Door animation, collision-entity synchronization and sound remain deferred result metadata; legacy Render/Game/Entity state remains untouched.

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
 -> compact native line-world state
 -> native event/script loop
 -> explicit native gameplay/render effects
 -> ESP32-native gameplay + renderer
```

## Hardware-proven fingerprints through PR #56

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

Current line-world candidate will establish new hardware canons:

```text
lineStateFNV = pending
lineDoorFNV  = pending
mutatedFNV   = pending
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

Hardware-proven persistent heap through PR #56:

```text
immutable arena       = 14112 B actual heap
mutable tile state    =  1040 B actual heap
mutable script state  =   100 B actual heap
opcode/string/effect owners = 0 B persistent heap
-----------------------------------------
proven total          = 15252 B
largest8              = 36852 preserved
```

Hardware-proven caller-owned/value types:

```text
EspMapStatusMessageState   =   8 B
EspMapDialogOwnerState     =  12 B
EspMapNotebookState        = 514 B
EspMapKeyGateResult        =  12 B
EspMapPasswordOwnerState   =  20 B
EspMapPasswordSubmitResult =  12 B
```

Current candidate adds the first persistent world overlay target:

```text
lineCount                   = 480
open bitset payload          =  60 B
locked bitset payload        =  60 B
EspMapLineState payload      = 120 B
EspMapLineDoorResult         =  16 B expected caller-local
actual line-state heap cost  = hardware pending
```

The probe accepts allocator cost only in the bounded range `120..184 B`; the exact real-CYD cost becomes canonical after PASS.

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

PASSWORD canonical sample:

```text
cmd17 event6 off6 resume7
arg1=00001d1c arg2=00040100
code=string28@13630+4 codeFNV=92444853
prompt=string29@13636+41 promptFNV=ddbe080a
codeLen=4
```

## Latest merged tested-build integrity

PASSWORD firmware:

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
passwordCanvasFNV = 214171cf -> 214171cf
gamePassCodeStable= yes
```

PASSWORD bounded pack access:

```text
heapOpen           = 64384
transientHeapCost  = 4364 B
largestOpen        = 36852
persistentHeapBytes= 0
```

Complete post-PARK heartbeat:

```text
uptime=170149 ms
heap=134512
heap8=68748
largest8=36852
all reported subsystems = ready
```

Absolute heap/frame values may change in the current firmware. Acceptance is exact before/after integrity within that build plus the expected persistent line-state allocation.

## Current line-door recovered contract

Legacy `Game_performDoorEvent()` for IDs 15/16:

```text
locked (flags & 0x400) -> false / no mutation
OPENLINE + already open (flags & 0x40) -> false / no mutation
CLOSELINE + already closed -> false / no mutation
otherwise toggle 0x40 -> true
successful open sound  = 5063
successful close sound = 5064
successful transition also requests door animation + special entity link sync
```

The permanent native owner keeps only two packed mutable predicates:

```text
openBits
lockedBits
```

All immutable geometry/texture/other flags remain in `EspMapRuntime`.

`lockedBits` is mutable state infrastructure but opcode `13 / EV_UNLOCK` remains unsupported. Its texture/entity-definition behavior is explicitly outside this milestone.

Successful native OPEN/CLOSE returns deferred effect flags rather than calling legacy animation/entity/sound APIs.

If a successfully handled source command has `arg2 & 0x200`, the result sets `removeCommandIfHandled=1`. It does not mutate the existing 81-byte `EspMapScriptState`; future native `Game_runEvent` ownership will do that outer-loop step.

## Current real-CYD probe target

The full 265-bytecode corpus is scanned and real OPEN/CLOSE counts are discovered rather than guessed.

Acceptance:

```text
refs > 0
openRefs + closeRefs = refs
mutatedRefs > 0
mutated + locked + alreadyTarget = refs
stateExecRefused = refs
resultBytes = 16
rollbackProofs = mutatedRefs
idempotent = 1
lockedGuard = 1
```

The first successful real command must produce an actual native open-bit mutation, a different state FNV, then exact rollback to the initial `lineStateFNV`.

The CYD will establish:

```text
initialOpen / initialLocked counts
lineStateFNV
OPEN/CLOSE corpus distribution
mutated / locked / already-target counts
removable handled count
first successful sample
lineDoorFNV
first mutatedFNV
persistent line-state heap cost
new-build heap/frame absolute values
```

Fail-closed target:

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

Integrity must preserve:

```text
arenaFNV          c3882516
mapStateFNV       cd99b98e
scriptFNV         f9e3d9df
legacyNotebookFNV 4d7705c5
legacy Player.keys
Hud witness
DoomCanvas password witness
Game continuation witness
framebuffer
legacy Render runtime remains clear
pack remains closed
entities=0
monsters=0
ST_PLAYING not reached
```

The new line-state owner remains allocated at PARK but must be restored to initial contents after every probe mutation.

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
 -> candidate: packed line world state + real EV_OPENLINE/EV_CLOSELINE mutation/rollback
```

Still forbidden:

```text
actual DoomCanvas dialog/password presentation
legacy Game continuation mutation by native code
legacy Hud/Player/password mutation
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

## Remaining MAP_INTRO families after current candidate

If OPEN/CLOSE passes, still unowned:

```text
2  EV_CHANGEMAP
7  EV_SHOW
9  EV_GIVEMAP
13 EV_UNLOCK
18 EV_HIDE
27 EV_SAVEGAME
```

Do not pre-authorize the next family. After PASS + merge, reread the then-current repository and choose the next bounded owner from the actual remaining semantics.

## Current validation target

Build/flash normal optimized:

```text
esp32-cyd
```

from `agent/esp32-map1-native-line-door-state` and capture `[MAPLINESTATE]`, `[MAPDOOR]`, `[MAPDOORPROBE]` plus a later stable `[ALIVE]` heartbeat.

Firmware candidate to identify the tested content:

```text
376f45bcdd12264d3cba1ee83e7197a52e248210
```

No CI status is published for this SHA. Do not mark merge-ready until the real classic CYD supplies the PASS.
