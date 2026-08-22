# ESP32 MAP_INTRO native UNLOCK world-state milestone

Branch: `agent/esp32-map1-native-unlock-state`

Base merged `main`:

```text
PR   = #57 — native OPEN/CLOSE line world state
main = e4fb32f41b7074bbb433e64f4c824edb2167cf50
```

Hardware-tested firmware content:

```text
e423093c8e17dda1345bebecf721dedf4bbb2002
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

Extend the hardware-proven native line world with exact `13 / EV_UNLOCK` semantics without reviving legacy `Render_t`, entities or pointer-heavy line state:

```text
hardware-proven 120 B OPEN/LOCKED owner
 + 60 B packed texture-9/10 owner
 + real EV_UNLOCK bytecode
 -> clear native locked bit
 -> optional native texture 9 -> 10
 -> deferred sound / special-entity-def / view-refresh metadata
 -> exact rollback in probe
```

Only `EV_UNLOCK` is supported here. `EV_LOCK`, `EV_TOGGLELOCK`, actual `EntityDef` mutation, actual sound playback, view refresh, legacy line mutation, entity instantiation, gameplay rendering and `ST_PLAYING` remain forbidden.

## Recovered legacy behavior

Legacy `Game_executeEvent()` dispatches opcode 13 to:

```c
Game_setLineLocked(game, arg1, false, false);
```

and then reaches its normal `return true`. Therefore every valid UNLOCK is handled even if it is already fully unlocked.

`Game_setLineLocked(..., false, false)` performs:

```text
line.flags &= ~0x400

if current line.texture == 9:
    line.texture = 10
    play sound 5067
    if matching special line entity exists:
        entity def -> type 0 / subtype 2
        refresh view
```

The outer `Game_runEvent()` removes a successfully handled command when source `arg2 & 0x200`. Native UNLOCK exposes that as `removeCommandIfHandled`; it does not yet mutate `EspMapScriptState`.

## Permanent native ownership

The merged OPEN/CLOSE owner remains byte-for-byte unchanged and keeps its hardware fingerprint:

```text
openBits       = 60 B
lockedBits     = 60 B
storage        = 120 B
actual heap    = 136 B
initialOpen    = 0
initialLocked  = 7
lineStateFNV   = e5e74861
```

New permanent files:

```text
ESP32/include/esp_map_line_texture_state.h
ESP32/src/esp_map_line_texture_state.c
```

The texture owner stores one bit per line for only the mutable 9/10 door variant:

```text
source texture 9/10:
  bit 0 -> effective texture 9
  bit 1 -> effective texture 10

all other source textures:
  effective texture = immutable source texture
```

MAP_INTRO real classic-CYD state:

```text
lineCount       = 480
storageBytes    = 60 B payload
variantCount    = 6
initialTexture10= 0
textureStateFNV = f1fc1875
```

The owner has no `Game`, `Render`, `Entity`, `EntityDef`, Hud, Sound, Player or DoomCanvas dependency.

## Permanent UNLOCK result

Hardware-proven classic ESP32 ABI:

```text
EspMapLineUnlockResult = 20 B
```

Valid semantics:

```text
lockedAfter = 0
lockMutated = lockedBefore != 0

textureBefore == 9:
  textureAfter   = 10
  textureMutated = 1
  soundId        = 5067
  effects        = PLAY_SOUND | SPECIAL_ENTITY_DEF_SYNC | REFRESH_IF_ENTITY

textureBefore != 9:
  textureAfter   = textureBefore
  textureMutated = 0
  sound/effects  = none

legacyReturnValue      = 1 always
removeCommandIfHandled = source arg2 & 0x200
```

When both lock and texture change, the executor treats them as one logical operation. If the lock primitive unexpectedly fails after a texture change, the texture change is rolled back and the command returns INVALID with a zeroed result.

## Real-CYD corpus proof

Normal optimized environment: `esp32-cyd`.

The real classic no-PSRAM CYD established:

```text
refs              = 6
mutated           = 6
lockMutated       = 6
textureMutated    = 6
noMutation        = 0
removable         = 0
resultBytes       = 20
stateExecRefused  = 6
unlockFNV         = 261d756a
elapsed           = 9 ms
```

The complete real corpus is therefore exceptionally clean:

```text
6 UNLOCK refs
6 initially locked target lines
6 current texture-9 targets
6 lock 1 -> 0 mutations
6 texture 9 -> 10 mutations
0 already-unlocked real refs
0 removable refs
```

Canonical first real UNLOCK mutation:

```text
global command = 18
event          = 6
command offset = 7
line           = 400
locked         = 1 -> 0
texture        = 9 -> 10
lockMutated    = 1
textureMutated = 1
sound          = 5067
effects        = 07
handled        = 1
removeIfHandled= 0
```

This is real `/intro.bsp` bytecode, not a synthetic command.

## Two-owner mutation + rollback proof

Initial owners:

```text
lineStateFNV    = e5e74861
textureStateFNV = f1fc1875
```

Applying the canonical sample produces:

```text
mutatedLineFNV    = 8d5f89d8
mutatedTextureFNV = 997459ec
```

Every real mutating UNLOCK is immediately restored:

```text
rollback = 6 / 6
final lineStateFNV    = e5e74861
final textureStateFNV = f1fc1875
worldRestored = yes
```

Thus the real CYD proved one bytecode command atomically mutating two separate native world owners and restoring both exactly.

## Repeated handled semantics

The first real mutating UNLOCK was also applied twice without rollback between calls.

First call:

```text
status=OK
handled=1
lock and texture mutate
```

Second call against the already-unlocked native state:

```text
status=OK
handled=1
lockMutated=0
textureMutated=0
sound=0
effects=0
world FNVs unchanged from post-first-call state
removeCommandIfHandled unchanged
```

Hardware proof:

```text
idempotentHandled = 1
```

Both owners were then rolled back to their initial fingerprints.

## Fail-closed / atomicity proof

Real hardware proved:

```text
notReady       = 1
unsupported    = 1
badOffset      = 1
badDescriptor  = 1
nullDescriptor = 1
nullResult     = 1
badTextureIndex= 1
badTextureValue= 1
nonVariant     = 1
stateAtomic    = yes
worldRestored  = yes
```

## Persistent RAM proof

Entering this stage, the persistent native heap was hardware-proven at `15388 B`.

The real texture-owner allocation is:

```text
payload             = 60 B
persistentHeapCost  = 76 B
allocatorOverhead   = 16 B
```

New persistent native total:

```text
immutable arena       14112 B
mutable tile state     1040 B
mutable script state    100 B
mutable line state      136 B
mutable texture state    76 B
----------------------------
total                 15464 B
```

Current-firmware allocation witness:

```text
heap8       = 68516 -> 68440
largest8    = 34804 -> 34804
```

The owner remains resident at PARK; there is no post-build heap drift.

## Current-build integrity

The preceding line-door stage in this same firmware remained canonical:

```text
lineStateFNV  = e5e74861
refs          = 71
open/close    = 39/32
mutated       = 29
locked        = 18
alreadyTarget = 24
lineDoorFNV   = b1c9d297
rollback      = 29/29
```

UNLOCK preserved all inherited state across its full audit:

```text
frameFNV          = 64347226 -> 64347226
arenaFNV          = c3882516 -> c3882516
mapStateFNV       = cd99b98e -> cd99b98e
scriptFNV         = f9e3d9df -> f9e3d9df
lineStateFNV      = e5e74861 -> e5e74861
legacyNotebookFNV = 4d7705c5 -> 4d7705c5
legacy keys       = 00000000 -> 00000000
hudFNV            = 505b1255 -> 505b1255
passwordCanvasFNV = 214171cf -> 214171cf
continuationFNV   = e2ba14a5 -> e2ba14a5
packIO            = no
legacyRuntimeClear= yes
```

No legacy world mutation occurred.

## Final PARK boundary

Hardware proved:

```text
nativeLineState        = yes
nativeDoorExec         = yes
nativeLineTextureState = yes
nativeUnlockExec       = yes
textureStorageBytes    = 60
resultBytes            = 20
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
uptime=25201 ms heap=134204 heap8=68440 largest8=34804 all reported subsystems ready
uptime=30202 ms heap=134204 heap8=68440 largest8=34804 all reported subsystems ready
```

Absolute `heap8`, `largest8` and framebuffer FNV can differ across firmware builds. Acceptance is same-build stability plus canonical native-state fingerprints and bounded persistent ownership.

## Hardware acceptance status

The real CYD proved the complete UNLOCK corpus, exact lock + texture behavior, six two-owner world mutations, 6/6 exact rollback, repeated handled semantics, fail-closed paths, 76-byte persistent allocation, legacy integrity and stable PARK heartbeats.

This milestone is **REAL-CYD HARDWARE PASS / MERGE-READY**.

Hardware-tested firmware content:

```text
e423093c8e17dda1345bebecf721dedf4bbb2002
```

All later commits must remain documentation-only unless another firmware is flashed.

## Remaining MAP_INTRO families

After UNLOCK, still unowned:

```text
2  EV_CHANGEMAP
7  EV_SHOW
9  EV_GIVEMAP
18 EV_HIDE
27 EV_SAVEGAME
```

Do not preselect the next family before merge recovery. Read the new true `main`, recovery docs and exact remaining legacy behavior first.
