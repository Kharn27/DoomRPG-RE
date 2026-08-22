# ESP32 MAP_INTRO native line-door world state milestone

Branch: `agent/esp32-map1-native-line-door-state`

Base merged `main`:

```text
PR   = #56 — native PASSWORD pause owner
main = 3c113cc047aeb613f2ba4ab7905e92487c796f80
```

Hardware-tested firmware content:

```text
376f45bcdd12264d3cba1ee83e7197a52e248210
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

Cross the first explicit mutable-world boundary without reintroducing legacy `Render_t`, entities or map-wide mutable line objects:

```text
480 immutable EspMapRuntime lines
 -> 60 B packed open bits
 -> 60 B packed locked bits
 -> real EV_OPENLINE / EV_CLOSELINE
 -> native open-bit transition
 -> deferred animation / entity-link / sound metadata
```

This milestone supports only:

```text
15 EV_OPENLINE
16 EV_CLOSELINE
```

It does **not** support `EV_UNLOCK`, mutate line texture, instantiate collision entities, animate doors, play sound, mutate legacy Render/Game/Entity state, render gameplay or enter `ST_PLAYING`.

## Why this family first

The hardware-proven opcode inventory already established that global bytecode command `0` is a real `16 / EV_CLOSELINE` command. OPEN/CLOSE is therefore the earliest real unsupported command in the MAP_INTRO corpus.

Unlike `EV_UNLOCK`, OPEN/CLOSE has one compact canonical world mutation: legacy `Game_performDoorEvent()` changes the line `0x40` open bit. `EV_UNLOCK` additionally changes the `0x400` lock bit, can switch texture `9 <-> 10`, changes the special collision entity definition and can play another sound, so it remains a later dedicated milestone.

## Recovered legacy behavior

For OPEN/CLOSE, legacy `Game_executeEvent()` delegates to `Game_performDoorEvent(game, codeId, arg1, flags)`:

```text
line = lines[arg1]

if line.flags & 0x400:
    return false                 // locked

if OPENLINE and line.flags & 0x40:
    return false                 // already open

if CLOSELINE and !(line.flags & 0x40):
    return false                 // already closed

line.flags ^= 0x40
DoomCanvas_updatePlayerDoors(line)
sync the matching special collision entity link
sound = 5063 when now open
sound = 5064 when now closed
return true
```

The outer legacy `Game_runEvent()` removes a successfully handled command when source `arg2` carries `0x200 / MCODE_FLAG_REMOVE`. Native line execution therefore returns `removeCommandIfHandled` but deliberately does **not** mutate `EspMapScriptState`; that remains future native event-loop behavior.

## Permanent native world state

Files:

```text
ESP32/include/esp_map_line_state.h
ESP32/src/esp_map_line_state.c
```

The immutable runtime already owns all 480 compact line records. Only the dynamic predicates required by this family are materialized:

```text
openBits   = ceil(lineCount / 8)
lockedBits = ceil(lineCount / 8)
```

Real MAP_INTRO classic-CYD layout:

```text
lineCount    = 480
bitsetBytes  = 60
storageBytes = 120 B payload
initialOpen  = 0
initialLocked= 7
lineStateFNV = e5e74861
```

No geometry, texture, flags word, line pointer or entity pointer is duplicated.

View:

```c
typedef struct EspMapLineStateView_s {
    const uint8_t* openBits;
    const uint8_t* lockedBits;
    uint32_t lineCount;
    uint32_t bitsetBytes;
    uint32_t storageBytes;
    uint32_t stateFNV1a;
    uint32_t openCount;
    uint32_t lockedCount;
} EspMapLineStateView;
```

Permanent primitives:

```text
EspMapLineState_reset()
EspMapLineState_buildFromRuntime()
EspMapLineState_isReady()
EspMapLineState_view()
EspMapLineState_getOpen()
EspMapLineState_getLocked()
EspMapLineState_setOpen()
EspMapLineState_setLocked()
```

`setLocked()` remains only a generic state primitive. There is no opcode-13 dispatcher and no `EV_UNLOCK` semantics are authorized yet.

## Permanent OPEN/CLOSE executor

`EspMapLineState_applyDoorCommand()` revalidates the descriptor against the immutable runtime, reads its canonical linked command and supports only IDs 15/16.

Hardware-proven result ABI:

```text
EspMapLineDoorResult = 16 B
```

Semantic statuses:

```text
NOT_READY         -> no state/result mutation
UNSUPPORTED       -> non-15/16 command, fail closed
INVALID           -> bad descriptor/offset/etc.
LINE_OUT_OF_RANGE -> malformed source index
LOCKED            -> valid semantic no-op, legacy return false
ALREADY_TARGET    -> valid semantic no-op, legacy return false
OK                -> open bit mutated, legacy return true
```

A successful result carries deferred effect metadata only:

```text
DOOR_ANIMATION
ENTITY_RELINK
PLAY_SOUND
```

No legacy effect is applied.

## Real-CYD corpus proof

Normal optimized environment: `esp32-cyd`.

The real classic no-PSRAM CYD established the complete OPEN/CLOSE corpus:

```text
refs              = 71
openRefs          = 39
closeRefs         = 32
mutated           = 29
locked            = 18
alreadyTarget     = 24
removable         = 12
stateExecRefused  = 71
resultBytes       = 16
lineDoorFNV       = b1c9d297
elapsed           = 11 ms
```

The semantic partition is exact:

```text
29 mutated + 18 locked + 24 alreadyTarget = 71 refs
```

Canonical first successful real world command:

```text
global command = 3
event          = 1
command offset = 2
line           = 459
opcode         = 15 / EV_OPENLINE
open           = 0 -> 1
locked         = 0
sound          = 5063
effects        = 07
removeIfHandled= 0
```

This is a real bytecode from `/intro.bsp`, not a synthetic command.

## First native world mutation + rollback proof

The initial 120-byte world overlay is:

```text
lineStateFNV = e5e74861
```

Applying the canonical real OPENLINE sample changes exactly one packed open bit:

```text
mutatedFNV = 8f57d779
```

The probe restores every successful real command immediately:

```text
rollback = 29 / 29
final lineStateFNV = e5e74861
worldRestored = yes
```

This is the first hardware-proven Doom RPG world mutation owned by the ESP32-native engine:

```text
real compact bytecode
 -> canonical descriptor
 -> EspMapLineState openBits
 -> real line 459: closed -> open
 -> rollback to exact initial native world state
```

The immutable arena, legacy Render lines and entities are never modified.

## Idempotence + lock gate proof

The first successful command was replayed against its mutated state:

```text
second apply -> ALREADY_TARGET
idempotent   = 1
state stable = yes
```

The same command was tested after temporarily setting its native lock bit:

```text
locked apply -> LOCKED
lockedGuard  = 1
open bit unchanged
no deferred effects applied
```

The temporary lock change was also rolled back exactly.

## Fail-closed proof

Hardware proved all expected refusal/atomicity paths:

```text
notReady       = 1
unsupported    = 1
badOffset      = 1
badDescriptor  = 1
nullDescriptor = 1
nullResult     = 1
badOpenIndex   = 1
badLockedIndex = 1
stateAtomic    = yes
worldRestored  = yes
```

## Persistent RAM proof

This is the first milestone that intentionally adds persistent native world heap.

Real classic-CYD allocation:

```text
payload             = 120 B
persistentHeapCost  = 136 B
allocatorOverhead   = 16 B
largest8            = 36852 -> 36852
```

The previously proven native structural/script heap was `15252 B`. The new hardware-proven persistent native total is therefore:

```text
15252 + 136 = 15388 B
```

The allocation remains resident at PARK because this is permanent mutable world state, not transient probe scratch.

## Current-build integrity

Before constructing `EspMapLineState` and after all command mutation/rollback work:

```text
heap8             = 68700 -> 68564
persistent delta  = 136 B exactly
largest8          = 36852 -> 36852
frameFNV          = 5a979d01 -> 5a979d01
arenaFNV          = c3882516 -> c3882516
mapStateFNV       = cd99b98e -> cd99b98e
scriptFNV         = f9e3d9df -> f9e3d9df
```

Legacy witnesses remained exact:

```text
legacyNotebookFNV = 4d7705c5 -> 4d7705c5
legacy keys       = 00000000 -> 00000000
hudFNV            = 505b1255 -> 505b1255
passwordCanvasFNV = 214171cf -> 214171cf
continuationFNV   = e2ba14a5 -> e2ba14a5
packIO            = no
legacyRuntimeClear= yes
```

The preceding CHECK_KEY/PASSWORD stages in this firmware were also stable at `heap8=68700`, `largest8=36852`, `frameFNV=5a979d01`. PASSWORD transient PAK open measured `heapOpen=64336`, `transientHeapCost=4364 B`, and fully recovered before the line-world stage.

## Final PARK boundary

Hardware proved:

```text
nativeArena            = yes
nativeTileState        = yes
nativeEventLookup      = yes
nativeEventDescriptor  = yes
nativeScriptState      = yes
nativeFilter           = yes
nativeOpcodeExec       = yes
nativeUiIntent         = yes
nativeStringReader     = yes
nativeStatusMessageOwner = yes
nativeDialogOwner      = yes
nativeNotebookOwner    = yes
nativeKeyGate          = yes
nativePasswordOwner    = yes
nativeLineState        = yes
nativeDoorExec         = yes
storageBytes           = 120
resultBytes            = 16
worldMutationProven    = yes
worldRestored          = yes
legacyWorldMutation    = no
framebufferMutation    = no
entities               = 0
monsters               = 0
noGameplay             = yes
```

Stable post-PARK heartbeats:

```text
uptime=30084 ms heap=134328 heap8=68564 largest8=36852 all reported subsystems ready
uptime=35085 ms heap=134328 heap8=68564 largest8=36852 all reported subsystems ready
```

Permanent prohibitions remain:

```text
shapeData == NULL
mediaTexels == NULL
legacy Render line mutation = no
legacy Entity link/unlink = no
actual door animation = no
actual sound playback = no
EV_UNLOCK texture/entity mutation = no
full native Game_runEvent loop = no
sprite SHOW/HIDE mutation = no
GIVEMAP automap mutation = no
map transitions = no
savegame mutation = no
entity/monster activation = no
native gameplay rendering = no
ST_PLAYING = no
```

## Hardware acceptance status

The complete real OPEN/CLOSE corpus, packed initial line state, first real world mutation, exact 29/29 rollback, idempotence, lock gating, fail-closed paths, persistent heap ownership, legacy integrity and stable post-PARK heartbeats are a **REAL-CYD HARDWARE PASS**.

This branch is **MERGE-READY**. The firmware-bearing content actually tested is:

```text
376f45bcdd12264d3cba1ee83e7197a52e248210
```

Every later commit must remain documentation-only unless another firmware is flashed.

## Remaining MAP_INTRO families

After OPEN/CLOSE, still unowned:

```text
2  EV_CHANGEMAP
7  EV_SHOW
9  EV_GIVEMAP
13 EV_UNLOCK
18 EV_HIDE
27 EV_SAVEGAME
```

Do not preselect the next family before merge recovery. `EV_UNLOCK` is an adjacent candidate, but its lock + texture + special-entity semantics must be owned explicitly rather than silently folded into this milestone.
