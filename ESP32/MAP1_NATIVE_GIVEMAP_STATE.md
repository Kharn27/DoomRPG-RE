# ESP32 MAP_INTRO native GIVEMAP automap-state milestone

Branch: `agent/esp32-map1-native-givemap-state`

Base merged `main`:

```text
PR   = #58 — native EV_UNLOCK world state
main = 7503b379185db3f05713eb34f1762173edb977d0
```

Firmware candidate content:

```text
2e0f8f5de93f806380ee254a8dab59a817c73f5d
```

Status: **IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING**.

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

The new reveal owner payload is therefore:

```text
60 + 43 = 103 B
```

No map geometry, sprite records or tile flags are duplicated.

## Why GIVEMAP before SHOW/HIDE

Recovered remaining world semantics show a useful boundary split.

`EV_SHOW` is entity-coupled: it changes map-sprite frame/hidden bits, can kill up to two qualifying entities occupying the sprite tile, then links `sprite->ent` into the world.

`EV_HIDE` is also entity-coupled: it walks the complete entity chain at a tile, hides each qualifying entity-backed sprite and unlinks those entities.

Those opcodes should wait for an explicit compact native entity/topology owner rather than smuggling legacy `Entity_t` behavior into a sprite bitset.

`EV_GIVEMAP`, by contrast, is pure automap state and can be owned permanently now.

## Recovered legacy behavior

Legacy `Game_executeEvent()` dispatches opcode 9 to `Game_givemap(game)` and then reaches its normal `return true`.

`Game_givemap()` performs exactly:

```text
for every line:
    if !(line.flags & 0x20):
        line.flags |= 0x80

for every map sprite:
    sprite.info |= 0x10000000

for each of 32x32 mapFlags cells:
    if cell & BIT_AM_ENTRANCE:
        cell |= BIT_AM_VISITED
```

A valid GIVEMAP remains handled even when all targets are already revealed/visited.

The outer legacy `Game_runEvent()` may remove a handled command when source `arg2 & 0x200`; native GIVEMAP returns `removeCommandIfHandled` but does not mutate `EspMapScriptState` yet.

## Existing tile-state ownership reused

`EspMapState` already owns the canonical 1024 mutable tile flags and already defines:

```text
BIT_AM_ENTRANCE = 0x04
BIT_AM_VISITED  = 0x10
```

Its initial hardware fingerprint remains:

```text
mapStateFNV = cd99b98e
```

This branch adds only:

```text
EspMapState_setVisited(tileIndex, visited)
```

The setter preserves every other structural tile bit, changes no storage layout and allocates no memory.

## Permanent automap reveal owner

New files:

```text
ESP32/include/esp_map_automap_state.h
ESP32/src/esp_map_automap_state.c
```

State view:

```c
typedef struct EspMapAutomapStateView_s {
    const uint8_t* lineRevealedBits;
    const uint8_t* spriteRevealedBits;
    uint32_t lineCount;
    uint32_t spriteCount;
    uint32_t lineBitsetBytes;
    uint32_t spriteBitsetBytes;
    uint32_t storageBytes;
    uint32_t stateFNV1a;
    uint32_t lineRevealedCount;
    uint32_t spriteRevealedCount;
} EspMapAutomapStateView;
```

For MAP_INTRO the structural payload is fixed from already-proven counts:

```text
lineCount         = 480
lineBitsetBytes   = 60
mapSpriteCount    = 344
spriteBitsetBytes = 43
storageBytes      = 103 B
```

Initial bits are derived directly from immutable source records:

```text
line reveal   <- source line.flags & 0x80
sprite reveal <- source sprite.info & 0x10000000
```

The real CYD must establish the initial reveal counts, initial automap-state FNV and actual allocator cost.

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

## GIVEMAP result

Expected classic ESP32 ABI:

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

Repeated valid GIVEMAP semantics are explicit:

```text
first apply:
  handled=1
  one or more native automap/tile mutations expected

second apply without rollback:
  handled=1
  linesMutated=0
  spritesMutated=0
  tilesMutated=0
  mutated=0
  state fingerprints unchanged from post-first-apply
```

## Temporary real-CYD probe

New files:

```text
ESP32/include/native_map1_givemap_probe.h
ESP32/src/native_map1_givemap_probe.c
```

The probe runs only after the hardware-proven UNLOCK stage. It requires these inherited native owners to remain exact:

```text
arenaFNV             = c3882516
mapStateFNV          = cd99b98e
scriptFNV            = f9e3d9df
lineStateFNV         = e5e74861
lineTextureStateFNV  = f1fc1875
```

### Initial automap proof

Every line/sprite reveal bit is independently checked against its immutable source record before GIVEMAP is executed.

Hardware-pending canons:

```text
initialLineRevealed
initialSpriteRevealed
automapStateFNV
lineTargets
spriteTargets
entranceTargets
```

### Complete real GIVEMAP corpus

The probe scans all 93 events / 265 bytecodes and discovers every real opcode-9 command.

For each real ref it requires:

```text
state-only executor -> UNSUPPORTED
canonical descriptor/command provenance
status -> OK
legacyReturnValue -> 1
line targets exactly match !(source flags & 0x20)
sprite targets exactly match all 344 map sprites
entrance targets exactly match native ESP_MAP_TILE_ENTRANCE cells
mutation counts exactly match current native state
removeCommandIfHandled exactly matches arg2 & 0x200
```

After each real mutating command the probe restores:

```text
line reveal bits -> immutable source 0x80 values
sprite reveal bits -> immutable source 0x10000000 values
all VISITED bits -> initial clear state
```

and requires exact initial fingerprints again.

Acceptance requires at least one real GIVEMAP mutation.

New hardware canons intentionally left pending:

```text
GIVEMAP ref count
mutated/no-mutation refs
remove-if-handled count
line/sprite/tile mutation totals
first real sample
giveMapFNV
first mutated automap-state FNV
first mutated map-state FNV
```

### Fail-closed / atomicity proof

Expected:

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

## RAM / integrity boundary

Hardware-proven persistent native heap entering this milestone:

```text
immutable arena        14112 B
mutable tile state      1040 B
mutable script state     100 B
mutable line state       136 B
mutable texture state     76 B
-----------------------------
proven total           15464 B
```

Candidate payload:

```text
automap reveal state = 103 B
```

Probe acceptance for actual allocator cost:

```text
103 B <= incremental persistent heap <= 167 B
largest8 >= 32768 B
no heap drift after state construction
```

Exact allocation and new persistent total are hardware-pending.

All inherited owners and legacy witnesses must remain exact after rollback:

```text
framebuffer unchanged
arenaFNV unchanged
mapStateFNV = cd99b98e
scriptFNV unchanged
lineStateFNV = e5e74861
lineTextureStateFNV = f1fc1875
legacy notebook/keys/Hud/password/continuation unchanged
pack closed
legacy Render runtime clear
entities=0
monsters=0
ST_PLAYING not reached
```

The 103-byte automap owner remains resident at PARK but restored to its initial content FNV.

## Expected Serial family

```text
[MAPGIVEMAPPROBE] ARMED ...

=== Doom RPG ESP32-native MAP_INTRO GIVEMAP automap state ===
[MAPGIVEMAPPROBE] CONTRACT ...
[MAPAUTOMAP] READY lines=480 lineBytes=60 lineRevealed=... sprites=344 spriteBytes=43 spriteRevealed=... storageBytes=103 stateFNV=...
[MAPGIVEMAP] READY refs=... mutated=... noMutation=... removable=... resultBytes=20 stateExecRefused=... lineTargets=... spriteTargets=... entranceTargets=... lineMutTotal=... spriteMutTotal=... tileMutTotal=... giveMapFNV=... elapsed=...ms
[MAPGIVEMAP] SAMPLE cmd=... event=... off=... lineMut=... spriteMut=... tileMut=... handled=1 removeIfHandled=...
[MAPGIVEMAP] WORLD ... automapStateFNV=... mutatedAutomapFNV=... mapStateFNV=cd99b98e mutatedMapStateFNV=... rollback=.../... idempotentHandled=1
[MAPGIVEMAP] FAILCLOSED ... stateAtomic=yes worldRestored=yes
[MAPGIVEMAPPROBE] RAM heap8=...->... persistentHeapCost=... payload=103 allocatorOverhead=... largest8=...->...
[MAPGIVEMAPPROBE] LEGACY ... packIO=no legacyRuntimeClear=yes
[MAPGIVEMAPPROBE] PARK ... nativeAutomapState=yes nativeGiveMapExec=yes storageBytes=103 resultBytes=20 worldMutationProven=yes worldRestored=yes ...
[ALIVE] ...
```

Use normal optimized PlatformIO environment:

```text
esp32-cyd
```

No CI status is published for firmware candidate `2e0f8f5de93f806380ee254a8dab59a817c73f5d`. A local build could not be attempted because the execution container could not resolve GitHub; this is not a compile result. Real classic-CYD Serial remains the validation authority.

## Remaining MAP_INTRO families after PASS

If GIVEMAP passes, still unowned:

```text
2  EV_CHANGEMAP
7  EV_SHOW
18 EV_HIDE
27 EV_SAVEGAME
```

Do not pre-authorize the next family before PASS + merge recovery.
