# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for stable build/architecture guidance, [`DOCUMENTATION.md`](DOCUMENTATION.md) for documentation ownership rules, and milestone documents for detailed hardware evidence.

The older full recovery catalog remains preserved in [`archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md`](archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md).

## Latest merged hardware baseline

```text
PR   = #45 — first native mutable MAP_INTRO tile state
main = feec8a7fcb839dbd9f6de708f56f26b69a1e79e9
```

PR #45 hardware-proved the first separately owned mutable native world state while preserving the compact source arena byte-for-byte.

Current candidate:

```text
branch = agent/esp32-map1-native-events
base   = feec8a7fcb839dbd9f6de708f56f26b69a1e79e9
hardware-affecting head = a522c56403ff3269e02e93213b8f7d643bfba0af
status = ALLOCATION-FREE NATIVE TILE EVENT LOOKUP IMPLEMENTED; AWAITING REAL-CYD HARDWARE PASS
```

Detailed active milestone: [`MAP1_NATIVE_EVENTS.md`](MAP1_NATIVE_EVENTS.md).

Merged state evidence: [`MAP1_NATIVE_STATE.md`](MAP1_NATIVE_STATE.md).

## Permanent target / ownership

```text
board        = ESP32-2432S028R classic CYD
MCU          = ESP32-D0WD-V3 dual-core 240 MHz
flash        = 4 MB
PSRAM        = none
display      = ILI9341 320x240 landscape
touch        = XPT2046
storage      = microSD
framebuffer  = 160x120 RGB565 = 38400 B
presentation = exact nearest-neighbor 2x
audio        = deferred
```

Permanent resource invariant:

```text
shapeData   == NULL
mediaTexels == NULL
```

Permanent engine ownership:

```text
DoomRPG-RE = executable specification / format + behavior reference
final CYD engine = our ESP32-native engine
```

Desktop-derived `Render_t`, `DoomCanvas_t`, pointer-heavy map structures and linker wrappers remain migration scaffolding, not permanent architecture requirements.

Current native direction:

```text
Doom RPG source data
    -> EspBspReader
    -> immutable compact EspMapRuntime
    -> allocation-free native accessors
    -> explicit mutable EspMapState
    -> allocation-free EspMapEvents tile lookup
    -> later bounded native event/gameplay consumers
```

## Current merged hardware-safe boundary

Normal optimized firmware at PR #45 reaches:

```text
menu                    = MENU_NONE
state                   = ST_INTRO (9)
storyPage               = 3
storyTextPage           = 0
startupMap              = 1 (MAP_INTRO / /intro.bsp)
intro clock/input       = inactive
intro images/texts      = NULL
legacy nodes/lines      = NULL
legacy mapSprites       = NULL
legacy mappings         = NULL
shapeData               = NULL
mediaTexels             = NULL
wall/sprite LRU caches  = inactive
entities/monsters       = 0
legacy gameplay loader  = NOT called

native map arena        = RESIDENT + IMMUTABLE
arena payload           = 14095 B
actual arena heap       = 14112 B
arenaFNV                = c3882516

native tile state       = RESIDENT + MUTABLE
tile payload            = 1024 B
actual tile heap        = 1040 B
stateFNV                = cd99b98e

combined native heap    = 15152 B
heap8                   = 69016 after state build on PR #45 test build
largest8                = 36852
ST_PLAYING              = NOT entered
```

Build-to-build heap8 may move as code grows. The permanent regression is ownership/integrity, not one absolute free-heap address.

## MAP_INTRO source reference

```text
file        = /intro.bsp
name        = Entrance
sourceBytes = 21823
CRC32       = 623f34e4
FNV-1a      = d5cc751f
```

Structure:

```text
nodes          = 223
lines          = 480
mapSprites     = 344
events         = 93
byteCodes      = 265
strings        = 94
stringData     = 7779 B
maxString      = 313 B
```

Payload-relative offsets:

```text
nodes         = 35
lines         = 2267
mapSprites    = 7069
events        = 8791
byteCodes     = 9165
strings       = 11552
blockMap      = 19519
planeTextures = 19775
end           = 21823
```

Map-derived resource inventory:

```text
line texture IDs        = 20
map sprite IDs          = 48
required texture IDs    = 33
required sprite IDs     = 45
plane texture IDs       = 12
EV_CHANGESPRITE         = 0
sprite-as-texture refs  = 0
ID overflow             = 0 / 0 / 0
```

## Native map foundation — hardware validated

### Immutable arena

```text
payload                = 14095 B
actual heap use        = 14112 B
allocator overhead     = 17 B
populateReadCalls      = 33
populateElapsed        = 61–63 ms across tested builds
arenaFNV               = c3882516
largest8               = 36852 preserved
```

### Allocation-free access contract

Canonical semantic fingerprint:

```text
decodedFNV = a426dd18
```

The real CYD swept every node, line, map sprite, event, bytecode, string offset, block cell, plane cell, resource ID and bounds family with zero heap/largest-block/framebuffer drift.

### Mutable tile state

```text
payload            = 1024 B
actual heap use    = 1040 B
allocator overhead = 16 B
stateFNV           = cd99b98e
build/verify       = 9 ms
largest8           = 36852 preserved
```

Block-map base:

```text
0 = 298
1 = 697
2 = 27
3 = 2
sum = 1024
```

Recovered spatial topology:

```text
texture-7 entrance refs  = 4
unique entrance cells    = 4
first entrance tile      = 68

event refs               = 93
unique event cells       = 93
first event tile         = 68
visited cells            = 0
```

Tile 68 carries both entrance and event bits, confirming the composable bit-field state model.

Combined measured native map/world heap:

```text
14112 B immutable arena
+1040 B mutable tile state
----------------------------
15152 B actual heap
```

Measured legacy structural allocation was `55341 B`, so the current native structure + mutable spatial state still saves `40189 B` (~72.6%).

## Current candidate: allocation-free native event lookup

Recovered reference behavior:

```text
key = event & 1023
Render_findEventIndex() = binary search over tileEvents[] by that key
```

Rather than allocate another 1024-entry tile index, the new permanent API binary-searches the already resident compact 372-byte event section directly:

```text
EspMapEvents_findByTile(tile, &eventRef)
```

Returned compact value:

```text
EspMapEventRef
    index      uint16
    tileIndex  uint16
    value      uint32
```

The implementation uses deterministic lower-bound semantics and adds:

```text
persistent heap = 0 B
```

For MAP_INTRO, the existing state result strongly predicts a strict one-event-per-tile topology:

```text
93 event records
93 unique event-bearing tiles
first event = tile 68 / index 0 / value 00080044
```

The legacy binary search implies ordering by tile key; the new hardware probe validates that assumption explicitly instead of merely trusting it.

## Current hardware gate

After the inherited `MAPSTATEPROBE` PASS, `MAPEVENTPROBE` must:

```text
scan all 93 events
require strictly increasing (event & 1023) tile keys
require all 93 retained trigger-mask semantics
query all 1024 tiles through EspMapEvents_findByTile()
compare against an independent sequential oracle
require EspMapState.EVENTS == lookup existence for every tile
require 93 hits / 931 misses
require tile 1024 and NULL output to fail closed
establish canonical lookupFNV
establish last event tile/value
measure full 1024-query elapsed time
```

Integrity gates:

```text
heap8      X -> X
largest8   Y -> Y
frameFNV   F -> F
arenaFNV   c3882516 -> c3882516
stateFNV   cd99b98e -> cd99b98e
entities   = 0
monsters   = 0
ST_PLAYING = no
```

No event bytecode is executed in this milestone.

## Current candidate execution path

```text
validated intro disposal
    -> native BSP inventory/plan
    -> compact resident arena
    -> native accessor sweep
    -> native mutable tile-state build
    -> allocation-free tile/event lookup sweep
    -> PARK
```

Still absent:

```text
shapeData/mediaTexels
complete raw BSP allocation
legacy Render_beginLoadMapData()
legacy Render.tileEvents ownership
native bytecode/event execution
entity/monster activation
entityDb ownership
visited/save-state mutation
player spawn
native gameplay rendering
ST_PLAYING
continuous gameplay loop
```

## Stable earlier references

```text
logical framebuffer           = 160x120 RGB565 = 38400 B
normal full-screen Present    ~= 42.7 ms
wall LRU3 peak payload        = 6144 B
sprite LRU3 peak payload      = 6038 B
menu persistent used          = 14092 B
fresh Start cleanup recovered = 55416 B
intro teardown recovered      = 33768 B on PR #41
```

Detailed earlier evidence remains in the milestone documents and archived recovery snapshots.

## Next action

Build/flash `agent/esp32-map1-native-events` with normal `esp32-cyd`, progress through the intro, and capture the new `MAPEVENTS` / `MAPEVENTPROBE` block plus later stable `[ALIVE]` heartbeats.

If it passes, record `lookupFNV`, last event tile/value and elapsed time, then decide merge. Do not advance to bytecode execution on an unproven lookup contract.
