# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for stable build/architecture guidance, [`DOCUMENTATION.md`](DOCUMENTATION.md) for documentation ownership rules, and milestone documents for detailed hardware evidence.

The older full recovery catalog remains preserved in [`archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md`](archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md).

## Latest merged hardware baseline

```text
PR   = #46 — allocation-free native MAP_INTRO tile event lookup
main = 438cffabaaaaa3dc3b45486f56eacec1a047edcf
```

PR #46 hardware-proved exhaustive tile -> event resolution directly over the compact resident event section with no extra persistent index.

Current candidate:

```text
branch = agent/esp32-map1-native-event-descriptor
base   = 438cffabaaaaa3dc3b45486f56eacec1a047edcf
hardware-affecting head = b3e453baea1ac861c608f0c11b8aaa592f1cc3e5
status = READ-ONLY NATIVE EVENT DESCRIPTOR/BYTECODE LINKAGE IMPLEMENTED; AWAITING REAL-CYD HARDWARE PASS
```

Detailed active milestone: [`MAP1_NATIVE_EVENT_DESCRIPTOR.md`](MAP1_NATIVE_EVENT_DESCRIPTOR.md).

Merged event-lookup evidence: [`MAP1_NATIVE_EVENTS.md`](MAP1_NATIVE_EVENTS.md).

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
    -> read-only native event descriptor / bytecode linkage
    -> later bounded execution filtering + mutable event state
    -> native gameplay + renderer
```

## Current merged hardware-safe boundary

Normal optimized firmware at PR #46 reaches:

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

native event lookup     = READY + ALLOCATION-FREE
lookup persistent bytes = 0
lookupFNV               = 63430151

combined native heap    = 15152 B
heap8                   = 69000 on PR #46 hardware run
largest8                = 36852
ST_PLAYING              = NOT entered
```

Absolute `heap8` may move slightly between builds as firmware code grows. The regression contract is exact ownership, measured allocation cost, largest-block integrity, fingerprints and zero drift inside allocation-free stages.

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

### Immutable compact arena

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

Spatial topology:

```text
block base             = 298 / 697 / 27 / 2
texture-7 entrances    = 4 refs / 4 unique cells
first entrance tile    = 68
event-bearing cells    = 93 refs / 93 unique cells
first event tile       = 68
visited cells          = 0
```

Combined measured persistent map/world heap:

```text
14112 B immutable arena
+1040 B mutable tile state
----------------------------
15152 B actual heap
```

Measured legacy structural allocation was `55341 B`, so current native structure + mutable spatial state saves `40189 B` (~72.6%).

## Native tile-event lookup — hardware validated

Recovered reference key:

```text
key = event & 1023
```

Permanent API:

```text
EspMapEvents_findByTile(tile, &eventRef)
```

The API performs lower-bound binary search directly over the already resident compact 372-byte event section. No `eventIndexByTile[1024]` or duplicate `Render.tileEvents` ownership exists.

Persistent cost:

```text
0 B
```

Real-CYD topology:

```text
events       = 93
sorted       = yes
unique tiles = yes
first        = tile 68 / index 0 / value 00080044
last         = tile 968 / index 92 / value 000c23c8
```

Exhaustive lookup result:

```text
queries      = 1024
hits         = 93
misses       = 931
stateEvents  = 93 / 93
elapsed      = 5 ms
lookupFNV    = 63430151
```

Integrity result:

```text
heap8      = 69000 -> 69000
largest8   = 36852 -> 36852
frameFNV   = b8b39f0f -> b8b39f0f
arenaFNV   = c3882516 -> c3882516
stateFNV   = cd99b98e -> cd99b98e
entities   = 0
monsters   = 0
ST_PLAYING = no
```

Later `[ALIVE]` heartbeats remained stable at `heap8=69000`, `largest8=36852`.

The legacy binary-search ordering assumption is therefore explicitly proven for MAP_INTRO rather than inherited blindly.

## Current candidate: native event descriptor / bytecode linkage

Recovered `Game_runEvent()` event layout:

```text
bits  0..9   tile index
bits 10..18  first command index
bits 19..24  command count
bits 25..28  event state
bits 29..31  event flags
```

Masks:

```text
tile          = 0x000003ff
command index = 0x0007fc00 >> 10
command count = 0x01f80000 >> 19
state         = 0x1e000000 >> 25
flags         = 0xe0000000 >> 29
```

The desktop code multiplies command index/count by `BYTE_CODE_MAX == 3` only because its `mapByteCode` is a flattened `int[]` containing ID/ARG1/ARG2 triples. Native compact storage already has one logical 9-byte `EspMapByteCode` per command, so native descriptor indexes remain logical record indexes.

Permanent additions:

```text
EspMapEventDescriptor
EspMapEvents_describe()
EspMapEvents_getCommand()
```

Descriptor fields:

```text
raw value
source event index
tile index
first compact command index
exclusive command end
command count
initial event state
event flags
```

The state field is explicitly **initialState** because recovered gameplay later rewrites bits 25..28 in `tileEvents[]`. Current mutable event state will need a separate overlay; the immutable arena must remain unchanged.

The candidate validates every event command range against:

```text
resident byteCodeCount = 265
```

and exposes each linked command only through the existing bounds-checked compact runtime accessor.

New persistent cost:

```text
0 B
```

## Current hardware gate

After the inherited `MAPEVENTPROBE` PASS, `MAPDESCPROBE` must:

```text
decode all 93 event descriptors
independently compare every bit field
validate every command range against 265 compact bytecodes
walk every linked command through EspMapEvents_getCommand()
compare every linked command to direct EspMapRuntime_getByteCode()
fail closed at commandOffset == commandCount
fail closed for NULL / forged refs
compute descriptorFNV
compute linkageFNV
measure total command refs
measure unique linked bytecodes
measure overlaps and gaps
measure command-count range and max command end
measure observed initial-state mask
measure observed event-flags mask
```

The probe uses only a ~34-byte temporary stack bitmap to measure bytecode coverage. It deliberately does not assume before hardware whether the 265 bytecodes are covered exactly once.

Integrity gates remain:

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

No command is executed and no event state is changed.

## Native regression fingerprints

Hardware-established fingerprints so far:

```text
source BSP FNV = d5cc751f
arenaFNV       = c3882516
decodedFNV     = a426dd18
stateFNV       = cd99b98e
lookupFNV      = 63430151
```

Candidate will add:

```text
descriptorFNV = pending hardware
linkageFNV    = pending hardware
```

These protect distinct layers: source, compact physical representation, decoded semantics, mutable spatial state, tile-event resolution, event bit decoding and event-command linkage.

## Current candidate execution path

```text
validated intro disposal
    -> native BSP inventory/plan
    -> compact resident arena
    -> allocation-free native accessor sweep
    -> 1024-byte native mutable tile-state build
    -> allocation-free 1024-tile event lookup sweep
    -> read-only event descriptor + command-link sweep
    -> PARK
```

Still absent:

```text
shapeData/mediaTexels
complete raw BSP allocation
legacy Render_beginLoadMapData()
legacy Render.tileEvents ownership
legacy Render.mapByteCode ownership
native Game_runEvent() execution semantics
bytecode side effects
mutable current event-state overlay
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

Detailed earlier evidence remains in milestone documents and archived recovery snapshots.

## Next action

Build/flash `agent/esp32-map1-native-event-descriptor` with normal `esp32-cyd`, progress through the intro, and capture the new `MAPDESC` / `MAPDESCPROBE` block plus later stable `[ALIVE]` heartbeats.

If it passes, record the descriptor/linkage fingerprints and actual command coverage topology, then decide merge.

Do **not** advance to actual event execution on an unproven descriptor/linkage contract. The likely next bounded milestone after a PASS is a side-effect-free recovery of `Game_runEvent()` filtering/eligibility semantics plus a separate mutable current-event-state model, before any command is allowed to mutate the world.
