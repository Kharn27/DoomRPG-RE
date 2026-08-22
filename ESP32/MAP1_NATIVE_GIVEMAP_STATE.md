# ESP32 MAP_INTRO native GIVEMAP automap-state milestone

Branch: `agent/esp32-map1-native-givemap-state`

Base merged `main`:

```text
PR   = #58 — native EV_UNLOCK world state
main = 7503b379185db3f05713eb34f1762173edb977d0
```

Hardware-tested firmware content:

```text
2e0f8f5de93f806380ee254a8dab59a817c73f5d
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

Own exact `9 / EV_GIVEMAP` semantics without reviving legacy `Render_t` or entity state:

```text
immutable 480-line runtime
 -> 60 B packed line-revealed bits
immutable 344-map-sprite runtime
 -> 43 B packed sprite-revealed bits
existing 1024-byte EspMapState
 -> mutate only BIT_AM_VISITED on BIT_AM_ENTRANCE cells
real EV_GIVEMAP bytecode
 -> native automap mutation
 -> exact rollback in probe
```

The new reveal owner payload is exactly `103 B`. No map geometry, sprite records or duplicate tile-state array are allocated.

## Why GIVEMAP before SHOW/HIDE

Recovered legacy behavior shows that `EV_SHOW` and `EV_HIDE` are entity-topology operations, not visibility-only commands.

`EV_SHOW` changes the target map-sprite frame/hidden bits, can kill up to two qualifying entities occupying the sprite tile, then links `sprite->ent` into the world.

`EV_HIDE` walks the complete entity chain at a tile, hides each qualifying entity-backed sprite and unlinks those entities.

Those families therefore remain deferred until an explicit compact native entity/topology owner exists.

`EV_GIVEMAP`, by contrast, is pure automap/tile state and is permanently ownable now.

## Recovered legacy behavior

Legacy `Game_executeEvent()` dispatches opcode 9 to `Game_givemap(game)` and then returns handled=true.

`Game_givemap()` performs:

```text
for every line:
    if !(line.flags & 0x20):
        line.flags |= 0x80

for every map sprite:
    sprite.info |= 0x10000000

for each 32x32 mapFlags cell:
    if cell & BIT_AM_ENTRANCE:
        cell |= BIT_AM_VISITED
```

A valid GIVEMAP remains handled even when all targets are already revealed/visited.

The outer `Game_runEvent()` may remove a handled command when source `arg2 & 0x200`; native GIVEMAP exposes this as `removeCommandIfHandled` but does not mutate `EspMapScriptState` yet.

## Existing tile-state ownership reused

`EspMapState` already owns the canonical 1024 mutable tile flags and defines:

```text
BIT_AM_ENTRANCE = 0x04
BIT_AM_VISITED  = 0x10
```

Initial hardware fingerprint:

```text
mapStateFNV = cd99b98e
```

This branch adds only:

```text
EspMapState_setVisited(tileIndex, visited)
```

The setter preserves every other structural tile bit, changes no storage layout and allocates no memory.

## Permanent automap owner

New files:

```text
ESP32/include/esp_map_automap_state.h
ESP32/src/esp_map_automap_state.c
```

MAP_INTRO storage proven on hardware:

```text
lineCount             = 480
lineBitsetBytes       = 60
initialLineRevealed   = 0
mapSpriteCount        = 344
spriteBitsetBytes     = 43
initialSpriteRevealed = 0
storageBytes          = 103
automapStateFNV       = 669b1aa7
```

Initial bits are derived directly from immutable source records:

```text
line reveal   <- source line.flags & 0x80
sprite reveal <- source sprite.info & 0x10000000
```

Permanent primitives:

```text
EspMapAutomapState_reset()
EspMapAutomapState_buildFromRuntime()
EspMapAutomapState_isReady()
EspMapAutomapState_view()
EspMapAutomapState_getLineRevealed()
EspMapAutomapState_setLineRevealed()
EspMapAutomapState_getSpriteRevealed()
EspMapAutomapState_setSpriteRevealed()
EspMapAutomapState_applyGiveMapCommand()
```

The permanent implementation has no `Game`, `Render`, `Entity`, Hud, Sound, Player or DoomCanvas dependency.

## GIVEMAP result ABI

Hardware-proven classic ESP32 ABI:

```text
EspMapGiveMapResult = 20 B
```

The result owns:

```text
source event / global command / source offset
line target count
sprite target count
entrance-tile target count
lines actually changed
sprites actually changed
tiles actually changed
mutated flag
legacyReturnValue = 1
removeCommandIfHandled
```

## Real-CYD corpus proof

Normal optimized environment: `esp32-cyd`.

The real classic no-PSRAM CYD established:

```text
refs             = 1
mutated          = 1
noMutation       = 0
removable        = 0
resultBytes      = 20
stateExecRefused = 1
lineTargets      = 430
spriteTargets    = 344
entranceTargets  = 4
lineMutTotal     = 430
spriteMutTotal   = 344
tileMutTotal     = 4
giveMapFNV       = 98c7ac59
elapsed          = 34 ms
```

Canonical real command:

```text
global command = 43
event          = 14
command offset = 1
line mutations = 430
sprite mutations = 344
tile mutations = 4
handled        = 1
removeIfHandled= 0
```

The line target count also proves that 50 of 480 source lines carry the no-automap `0x20` flag and are intentionally not revealed by GIVEMAP.

All 344 map sprites are GIVEMAP targets. The four entrance targets are the four native `BIT_AM_ENTRANCE` cells already owned by `EspMapState`.

## World mutation + rollback proof

Initial fingerprints:

```text
automapStateFNV = 669b1aa7
mapStateFNV     = cd99b98e
```

Applying the real GIVEMAP produces:

```text
mutatedAutomapFNV = 9d03ca2d
mutatedMapStateFNV = e21edbce
```

The probe then restores both owners exactly:

```text
rollback = 1 / 1
final automapStateFNV = 669b1aa7
final mapStateFNV     = cd99b98e
worldRestored = yes
```

Thus one real Doom RPG bytecode command simultaneously mutates the dedicated automap reveal owner and the pre-existing native tile owner, with exact two-owner rollback.

## Repeated handled semantics

The real mutating GIVEMAP was applied twice without rollback between calls.

First call:

```text
status=OK
handled=1
linesMutated=430
spritesMutated=344
tilesMutated=4
mutated=1
```

Second call against the already-revealed native state:

```text
status=OK
handled=1
linesMutated=0
spritesMutated=0
tilesMutated=0
mutated=0
world fingerprints unchanged from post-first-call state
removeCommandIfHandled unchanged
```

Hardware proof:

```text
idempotentHandled = 1
```

Both owners were then restored to their initial fingerprints.

## Fail-closed / atomicity proof

Real hardware proved:

```text
notReady       = 1
unsupported    = 1
badOffset      = 1
badDescriptor  = 1
nullDescriptor = 1
nullResult     = 1
badLineIndex   = 1
badSpriteIndex = 1
badRevealValue = 1
badVisitedIndex= 1
badVisitedValue= 1
stateAtomic    = yes
worldRestored  = yes
```

## Persistent RAM proof

Hardware-proven persistent native heap entering this stage was `15464 B`.

The real automap-owner allocation is:

```text
payload             = 103 B
persistentHeapCost  = 120 B
allocatorOverhead   = 17 B
```

New persistent native total:

```text
immutable arena        14112 B
mutable tile state      1040 B
mutable script state     100 B
mutable line state       136 B
mutable texture state     76 B
mutable automap state    120 B
-----------------------------
total                  15584 B
```

Same-build allocation witness:

```text
heap8       = 68384 -> 68264
largest8    = 34804 -> 34804
frameFNV    = 453f0d5c -> 453f0d5c
```

The 17-byte allocator overhead is the measured allocator delta for the 103-byte payload on this build; it is within the predeclared 103..167 B acceptance bound.

## Inherited world / legacy integrity

The preceding UNLOCK stage remained semantically canonical in this same firmware:

```text
textureStateFNV = f1fc1875
unlockFNV       = 261d756a
rollback        = 6/6
```

GIVEMAP preserved all inherited state after rollback:

```text
frameFNV            = 453f0d5c -> 453f0d5c
arenaFNV            = c3882516 -> c3882516
mapStateFNV         = cd99b98e -> cd99b98e
scriptFNV           = f9e3d9df -> f9e3d9df
lineStateFNV        = e5e74861
lineTextureStateFNV = f1fc1875
legacyNotebookFNV   = 4d7705c5 -> 4d7705c5
legacy keys         = 00000000 -> 00000000
hudFNV              = 505b1255 -> 505b1255
passwordCanvasFNV   = 214171cf -> 214171cf
continuationFNV     = e2ba14a5 -> e2ba14a5
packIO              = no
legacyRuntimeClear  = yes
```

No legacy world mutation occurred.

## Final PARK boundary

Hardware proved:

```text
nativeAutomapState = yes
nativeGiveMapExec  = yes
storageBytes       = 103
resultBytes        = 20
worldMutationProven = yes
worldRestored       = yes
legacyWorldMutation = no
framebufferMutation = no
entities             = 0
monsters             = 0
noGameplay           = yes
```

Stable post-PARK heartbeats:

```text
uptime=25135 ms heap=134028 heap8=68264 largest8=34804 all reported subsystems ready
uptime=30136 ms heap=134028 heap8=68264 largest8=34804 all reported subsystems ready
uptime=35137 ms heap=134028 heap8=68264 largest8=34804 all reported subsystems ready
```

Absolute `heap8`, `largest8` and framebuffer FNV can differ across firmware builds. Acceptance uses same-build stability plus canonical fingerprints and bounded persistent ownership.

## Hardware acceptance status

The real CYD proved the complete GIVEMAP corpus, exact line/sprite/entrance targeting, two-owner mutation, exact rollback, repeated handled semantics, fail-closed paths, 120-byte persistent allocation, legacy integrity and stable PARK heartbeats.

This milestone is **REAL-CYD HARDWARE PASS / MERGE-READY**.

Hardware-tested firmware content:

```text
2e0f8f5de93f806380ee254a8dab59a817c73f5d
```

All later commits must remain documentation-only unless another firmware is flashed.

## Remaining MAP_INTRO families

After GIVEMAP, still unowned:

```text
2  EV_CHANGEMAP
7  EV_SHOW
18 EV_HIDE
27 EV_SAVEGAME
```

Do not preselect the next family before merge recovery. SHOW/HIDE remain entity-topology coupled; CHANGEMAP and SAVEGAME remain larger ownership boundaries.