# ESP32 MAP_INTRO native UNLOCK world-state milestone

Branch: `agent/esp32-map1-native-unlock-state`

Base merged `main`:

```text
PR   = #57 — native OPEN/CLOSE line world state
main = e4fb32f41b7074bbb433e64f4c824edb2167cf50
```

Firmware candidate content:

```text
e423093c8e17dda1345bebecf721dedf4bbb2002
```

Status: **IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING**.

## Objective

Extend the first native world owner with exact `13 / EV_UNLOCK` semantics without reintroducing legacy `Render_t` or entities:

```text
hardware-proven 120 B line OPEN/LOCKED owner
 + 60 B packed 9/10 door-texture overlay
 + real EV_UNLOCK bytecode
 -> clear native locked bit
 -> if current texture == 9, native texture 9 -> 10
 -> deferred sound / special-entity-def / view-refresh metadata
 -> exact rollback in probe
```

This milestone supports only `EV_UNLOCK`. It does not authorize `EV_LOCK`, `EV_TOGGLELOCK`, actual `EntityDef` mutation, actual sound, renderer refresh, legacy line mutation, entity instantiation, gameplay rendering or `ST_PLAYING`.

## Recovered legacy behavior

`Game_executeEvent()` handles opcode 13 as:

```c
case EV_UNLOCK:
    Game_setLineLocked(game, arg1, false, false);
    break;
```

`Game_executeEvent()` then reaches its normal `return true`, so a valid UNLOCK is **handled even if it changes nothing**.

`Game_setLineLocked(game, index, false, false)` performs:

```text
line.flags &= ~0x400

if line.texture == 9:
    line.texture = 10
    play sound 5067
    if matching special line entity exists:
        entity def -> type 0 / subtype 2
        refresh view
```

Because `z2=false`, any line whose current texture is not 9 only receives the lock clear.

The outer `Game_runEvent()` still applies the existing handled-command rule:

```text
if command.arg2 & 0x200 and Game_executeEvent returned true:
    remove/disable that command
```

The native UNLOCK result exposes `removeCommandIfHandled` but does not mutate `EspMapScriptState`; the future native event loop owns that outer step.

## Why a separate texture overlay

The merged OPEN/CLOSE owner is already hardware-proven and canonical:

```text
openBits       = 60 B
lockedBits     = 60 B
storage        = 120 B
lineStateFNV   = e5e74861
actual heap    = 136 B
initialOpen    = 0
initialLocked  = 7
```

Changing that storage layout would invalidate a useful hardware fingerprint. Therefore this milestone leaves `EspMapLineState` untouched and adds a separate compact owner only for the mutable 9/10 lock-door texture variant.

For each of 480 lines:

```text
source texture 9/10:
  one bit stores current variant
  0 -> texture 9
  1 -> texture 10

all other source textures:
  effective texture remains immutable source texture
```

MAP_INTRO payload target:

```text
480 bits = 60 B
```

This representation is also reusable by a later native LOCK/TOGGLELOCK milestone without copying complete line records.

## Permanent native API

New files:

```text
ESP32/include/esp_map_line_texture_state.h
ESP32/src/esp_map_line_texture_state.c
```

State view:

```c
typedef struct EspMapLineTextureStateView_s {
    const uint8_t* texture10Bits;
    uint32_t lineCount;
    uint32_t bitsetBytes;
    uint32_t storageBytes;
    uint32_t stateFNV1a;
    uint32_t variantCount;
    uint32_t texture10Count;
} EspMapLineTextureStateView;
```

Permanent functions:

```text
EspMapLineTextureState_reset()
EspMapLineTextureState_buildFromRuntime()
EspMapLineTextureState_isReady()
EspMapLineTextureState_view()
EspMapLineTextureState_getEffectiveTexture()
EspMapLineTextureState_setDoorTexture()
EspMapLineTextureState_applyUnlockCommand()
```

The source depends only on native runtime/event/line-state APIs and heap allocation. It has no `Game`, `Render`, `Entity`, `EntityDef`, Hud, Sound, Player or DoomCanvas dependency.

## UNLOCK result

```c
typedef struct EspMapLineUnlockResult_s {
    uint16_t sourceEventIndex;
    uint16_t globalCommandIndex;
    uint16_t lineIndex;
    uint16_t soundId;
    uint16_t textureBefore;
    uint16_t textureAfter;
    uint8_t sourceCommandOffset;
    uint8_t lockedBefore;
    uint8_t lockedAfter;
    uint8_t lockMutated;
    uint8_t textureMutated;
    uint8_t effectFlags;
    uint8_t legacyReturnValue;
    uint8_t removeCommandIfHandled;
} EspMapLineUnlockResult;
```

Expected classic ESP32 ABI:

```text
resultBytes = 20
```

Valid command semantics:

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

legacyReturnValue = 1 always for valid EV_UNLOCK
```

`SPECIAL_ENTITY_DEF_SYNC` means future entity ownership should reproduce subtype 2 for a matching special line entity. `REFRESH_IF_ENTITY` preserves the legacy conditional refresh: the old code refreshes only after finding that entity.

## Atomicity

The executor validates descriptor, linked command, line index and both owners before mutation.

When both lock and texture change, it commits the texture and lock changes as one logical operation. If the second primitive unexpectedly fails, the first is rolled back before returning INVALID and the result is zeroed.

## Temporary real-CYD probe

New files:

```text
ESP32/include/native_map1_unlock_probe.h
ESP32/src/native_map1_unlock_probe.c
```

The stage runs only after the hardware-proven line-door probe and requires the inherited line owner to remain canonical:

```text
storageBytes = 120
lineStateFNV = e5e74861
openCount    = 0
lockedCount  = 7
```

### Texture-state build proof

The new owner is built directly from all 480 immutable line textures. Hardware must establish rather than predeclare:

```text
variantCount      // source texture 9 or 10
initialTexture10
textureStateFNV
actual persistent heap cost for the 60 B payload
```

Every effective texture must equal its immutable source texture before any UNLOCK command is executed.

### Real UNLOCK corpus

The probe scans all 93 events / 265 bytecodes and discovers every real opcode-13 command.

For each real ref it requires:

```text
state-only opcode executor -> UNSUPPORTED
canonical descriptor and command provenance
line index < 480
status -> OK
legacyReturnValue -> 1
lockedAfter -> 0
lockMutated exactly matches prior lock bit
textureMutated exactly matches current texture == 9
texture 9 -> 10 only
sound 5067 only on texture mutation
effect flags 0x07 only on texture mutation
removeCommandIfHandled exactly matches arg2 & 0x200
```

After each real mutation both native owners are restored to their exact initial FNVs.

Hardware acceptance requires at least one real UNLOCK mutation so this branch proves actual world execution rather than only owner construction.

The CYD will establish:

```text
UNLOCK ref count
mutated refs
lock-mutated refs
texture-mutated refs
no-mutation handled refs
removable handled refs
first real mutated command sample
unlockFNV
sample mutated line-state FNV
sample mutated texture-state FNV
```

### Repeated handled semantics

The first real mutating UNLOCK is applied twice without rollback between calls:

```text
first apply:
  OK / handled=1
  one or both native world owners mutate

second apply:
  OK / handled=1
  lockMutated=0
  textureMutated=0
  sound=0
  effects=0
  world FNVs unchanged from post-first-apply state
  removeCommandIfHandled unchanged
```

Then both owners are rolled back to their initial fingerprints.

### Fail closed

Expected hardware proof:

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

## RAM / integrity boundary

The proven persistent native heap entering this milestone is:

```text
immutable arena       14112 B
mutable tile state     1040 B
mutable script state    100 B
mutable line state      136 B
----------------------------
current total         15388 B
```

The candidate adds a 60-byte persistent texture payload. The probe accepts only a bounded allocation:

```text
60 B <= incremental heap cost <= 124 B
```

Exact allocator overhead and new total are hardware-pending.

The following must stay bit-exact through the probe:

```text
arenaFNV          = c3882516
mapStateFNV       = cd99b98e
scriptFNV         = f9e3d9df
lineStateFNV      = e5e74861 after rollback
legacyNotebookFNV = 4d7705c5
legacy Player.keys
Hud witness
DoomCanvas password witness
Game continuation witness
framebuffer
legacy Render runtime clear
pack closed
entities=0
monsters=0
ST_PLAYING not reached
```

The texture-state owner remains allocated at PARK but must be restored to its initial content FNV after all probe mutations.

## Expected Serial family

```text
[MAPUNLOCKPROBE] ARMED ...

=== Doom RPG ESP32-native MAP_INTRO UNLOCK world state ===
[MAPUNLOCKPROBE] CONTRACT ...
[MAPLINETEX] READY lines=480 storageBytes=60 variants=... texture10=... stateFNV=...
[MAPUNLOCK] READY refs=... mutated=... lockMutated=... textureMutated=... noMutation=... removable=... resultBytes=20 stateExecRefused=... unlockFNV=... elapsed=...ms
[MAPUNLOCK] SAMPLE cmd=... event=... off=... line=... locked=...->0 texture=...->... lockMut=... texMut=... sound=... effects=... handled=1 removeIfHandled=...
[MAPUNLOCK] WORLD lineStateBytes=120 lineStateFNV=e5e74861 textureBytes=60 variants=... initialTexture10=... textureStateFNV=... mutatedLineFNV=... mutatedTextureFNV=... rollback=.../... idempotentHandled=1
[MAPUNLOCK] FAILCLOSED notReady=1 unsupported=1 badOffset=1 badDescriptor=1 nullDescriptor=1 nullResult=1 badTextureIndex=1 badTextureValue=1 nonVariant=1 stateAtomic=yes worldRestored=yes
[MAPUNLOCKPROBE] RAM heap8=...->... persistentHeapCost=... payload=60 allocatorOverhead=... largest8=...->... frameFNV=...->... arenaFNV=... mapStateFNV=... scriptFNV=... lineStateFNV=e5e74861->e5e74861
[MAPUNLOCKPROBE] LEGACY ... packIO=no legacyRuntimeClear=yes
[MAPUNLOCKPROBE] PARK ... nativeLineTextureState=yes nativeUnlockExec=yes textureStorageBytes=60 resultBytes=20 worldMutationProven=yes worldRestored=yes legacyWorldMutation=no ...
[ALIVE] ...
```

Use normal optimized PlatformIO environment:

```text
esp32-cyd
```

No CI status is currently published for firmware candidate `e423093c8e17dda1345bebecf721dedf4bbb2002`. No local build or hardware PASS is claimed.

## Remaining MAP_INTRO families after PASS

If UNLOCK passes, still unowned:

```text
2  EV_CHANGEMAP
7  EV_SHOW
9  EV_GIVEMAP
18 EV_HIDE
27 EV_SAVEGAME
```

Do not pre-authorize the next family before PASS + merge recovery.
