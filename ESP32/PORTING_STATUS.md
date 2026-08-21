# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for stable build/architecture guidance, [`DOCUMENTATION.md`](DOCUMENTATION.md) for documentation ownership rules, and milestone documents for detailed hardware evidence.

The older full recovery catalog remains preserved in [`archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md`](archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md).

## Latest merged hardware baseline

```text
PR   = #46 — allocation-free native MAP_INTRO tile event lookup
main = 438cffabaaaaa3dc3b45486f56eacec1a047edcf
```

Current candidate:

```text
branch = agent/esp32-map1-native-event-descriptor
base   = 438cffabaaaaa3dc3b45486f56eacec1a047edcf
hardware-tested code = b3e453baea1ac861c608f0c11b8aaa592f1cc3e5
status = REAL-CYD HARDWARE PASS; EVENT DESCRIPTOR/BYTECODE LINKAGE VALIDATED; MERGE-READY
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
    -> side-effect-free event eligibility/filtering + mutable event state
    -> native gameplay + renderer
```

## Current hardware-safe boundary

Normal optimized firmware now reaches:

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

native descriptors      = READY + ALLOCATION-FREE
descriptorFNV           = 27115328
linkageFNV              = 5727902c

combined native heap    = 15152 B
heap8                   = 68992 on current hardware run
largest8                = 36852
ST_PLAYING              = NOT entered
```

Absolute `heap8` may move slightly between builds as firmware code grows. Regression is based on ownership, allocation cost, largest-block integrity and fingerprints.

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

The real CYD swept every node, line, map sprite, event, bytecode, string offset, block cell, plane cell, resource ID and bounds family with zero drift.

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

Measured legacy structural allocation was `55341 B`; current native structure + mutable spatial state saves `40189 B` (~72.6%).

## Native tile-event lookup — hardware validated

```text
EspMapEvents_findByTile(tile, &eventRef)
```

The API lower-bound binary-searches the resident 372-byte event section. No duplicate tile index is allocated.

```text
persistent cost = 0 B
events           = 93
sorted           = yes
unique tiles     = yes
first            = 68 / 0 / 00080044
last             = 968 / 92 / 000c23c8
queries          = 1024
hits/misses      = 93 / 931
elapsed          = 5 ms
lookupFNV        = 63430151
```

Hardware integrity:

```text
heap drift     = 0
largest drift  = 0
frame drift    = 0
arenaFNV       = c3882516 unchanged
stateFNV       = cd99b98e unchanged
```

## Native event descriptor / bytecode linkage — hardware validated

Recovered 32-bit event layout:

```text
bits  0..9   tile
bits 10..18  first compact command index
bits 19..24  command count
bits 25..28  initial event state
bits 29..31  event flags
```

Permanent API:

```text
EspMapEvents_describe()
EspMapEvents_getCommand()
```

The desktop `* 3` index/count scaling is only a flattened `ID/ARG1/ARG2 int[]` storage artifact; native values are logical `EspMapByteCode` record indexes.

Real-CYD fingerprints:

```text
descriptorFNV = 27115328
linkageFNV    = 5727902c
elapsed       = 2 ms
persistent    = 0 B
```

### Exact event -> bytecode topology

```text
commandRefs = 265
unique      = 265
overlaps    = 0
gaps        = 0
countRange  = 1..14
maxEnd      = 265
```

The 93 event command ranges therefore partition the complete 265-command stream exactly once: every command belongs to one event, there are no holes and no overlaps.

Boundary descriptors:

```text
first event = tile 68 / index 0 / raw 00080044 / commands 0+1 / state 0 / flags 0
last event  = tile 968 / index 92 / raw 000c23c8 / commands 264+1 / state 0 / flags 0
```

Observed masks:

```text
stateMask = 0001 -> initial states present = {0}
flagsMask = 03   -> event flag values present = {0,1}
```

All 93 BSP events start at initial state 0. Later current-state changes belong in a separate mutable overlay; the immutable arena must remain unchanged.

Descriptor-stage integrity:

```text
heap8      = 68992 -> 68992
largest8   = 36852 -> 36852
frameFNV   = bd237825 -> bd237825
arenaFNV   = c3882516 -> c3882516
stateFNV   = cd99b98e -> cd99b98e
scriptExec = no
entities   = 0
monsters   = 0
ST_PLAYING = no
```

Later `[ALIVE]` remained stable at `heap8=68992`, `largest8=36852`.

## Native regression fingerprints

```text
source BSP FNV = d5cc751f
arenaFNV       = c3882516
decodedFNV     = a426dd18
stateFNV       = cd99b98e
lookupFNV      = 63430151
descriptorFNV  = 27115328
linkageFNV     = 5727902c
```

## Execution path now proven

```text
validated intro disposal
    -> native BSP inventory/plan
    -> compact resident arena
    -> allocation-free native accessor sweep
    -> 1024-byte native mutable tile-state build
    -> allocation-free 1024-tile event lookup sweep
    -> allocation-free event descriptor + command-link sweep
    -> PARK
```

Still absent:

```text
shapeData/mediaTexels
complete raw BSP allocation
legacy Render_beginLoadMapData()
legacy Render.tileEvents/mapByteCode ownership
native Game_runEvent() execution
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

## Merge recommendation

**MERGE `agent/esp32-map1-native-event-descriptor`.**

Hardware-tested code:

```text
b3e453baea1ac861c608f0c11b8aaa592f1cc3e5
```

If all later commits remain documentation-only, no additional hardware flash is required.

## Next bounded milestone after merge

Recover `Game_runEvent()` **eligibility/filtering semantics without side effects**. The next layer should decide which commands would be eligible from:

```text
current event state
EVENT_FLAG_BLOCKINPUT
event trigger/direction flags
command arg2 state/trigger filters
resume/start command offset
```

A small separately owned current-event-state overlay is likely required. It must be initialized from descriptor `initialState` and must not mutate `EspMapRuntime`.

Do not execute `Game_executeEvent()` effects until that decision layer is independently hardware-proven.
