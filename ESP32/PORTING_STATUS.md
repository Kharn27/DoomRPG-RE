# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for stable build/architecture guidance, [`DOCUMENTATION.md`](DOCUMENTATION.md) for documentation ownership rules, and milestone documents for detailed hardware evidence.

The older full recovery catalog remains preserved in [`archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md`](archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md).

## Latest merged hardware baseline

```text
PR   = #44 — native compact MAP_INTRO access contract
main = ddcf19e6166f210a6f63fec1c608234ee3e253ea
```

PR #44 hardware-proved complete allocation-free native indexed access over the resident MAP_INTRO arena with canonical `decodedFNV=a426dd18` and zero heap/largest-block/framebuffer drift.

Current candidate:

```text
branch = agent/esp32-map1-native-state
base   = ddcf19e6166f210a6f63fec1c608234ee3e253ea
hardware-tested code = 9a17654b56a190932615bba4894e90debd0e3773
status = REAL-CYD HARDWARE PASS; FIRST NATIVE MUTABLE TILE STATE VALIDATED; MERGE-READY
```

Detailed active milestone: [`MAP1_NATIVE_STATE.md`](MAP1_NATIVE_STATE.md).

Merged access evidence: [`MAP1_NATIVE_ACCESS.md`](MAP1_NATIVE_ACCESS.md).

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

Current native ownership direction:

```text
Doom RPG source data
    -> EspBspReader
    -> immutable compact EspMapRuntime
    -> allocation-free native accessors
    -> small explicit mutable EspMapState / later overlays
    -> native gameplay + renderer
```

## Current hardware-safe boundary

Normal optimized firmware now reaches:

```text
menu                    = MENU_NONE
state                   = ST_INTRO (9)
storyPage               = 3
storyTextPage           = 0
intro clock/input       = inactive
intro images/texts      = NULL
startupMap              = 1 (MAP_INTRO / /intro.bsp)
legacy nodes/lines      = NULL
legacy mapSprites       = NULL
legacy mappings         = NULL
shapeData               = NULL
mediaTexels             = NULL
wall/sprite LRU caches  = inactive
entities/monsters       = 0
legacy gameplay loader  = NOT called
native map arena        = RESIDENT + IMMUTABLE
native arena payload    = 14095 B
actual arena heap cost  = 14112 B
native tile state       = RESIDENT + MUTABLE
native tile payload     = 1024 B
actual tile heap cost   = 1040 B
combined native heap    = 15152 B
heap8                   = 69016 after tile-state build on current test build
largest8                = 36852
arenaFNV                = c3882516
decodedFNV              = a426dd18
stateFNV                = cd99b98e
ST_PLAYING              = NOT entered
```

The mutable state is separately owned from the immutable arena. Source records remain byte-for-byte stable while gameplay/spatial bits can evolve through explicit native state APIs.

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

Resource inventory remains:

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

## Native compact arena — hardware validated

```text
payload                = 14095 B
actual heap use        = 14112 B
allocator overhead     = 17 B
populateReadCalls      = 33
populateElapsed        = 61 ms on current test build
arenaFNV               = c3882516
largest8               = 36852 preserved
```

Legacy structural runtime measured `55341 B`; the native resident arena alone saves about 41.2 KiB / 74.5% versus that pointer-heavy structural allocation.

Strings remain on SD; only 188 B of payload offsets are resident.

## Native access contract — hardware validated

The access layer sweeps:

```text
223 nodes
480 lines
344 map sprites
93 events
265 bytecodes
94 string offsets
1024 block-map cells
2048 plane cells
256 resource IDs
all out-of-range families
```

Canonical semantic fingerprint:

```text
decodedFNV = a426dd18
```

Current test build result:

```text
elapsed    = 4 ms
heap8      = 70056 -> 70056
largest8   = 36852 -> 36852
frameFNV   = unchanged
arenaFNV   = c3882516 unchanged
```

The earlier 3 ms result and this 4 ms result share the same semantic fingerprint and zero-drift boundary; the 1 ms difference is normal timing variation.

Block-map distribution remains:

```text
0 = 298
1 = 697
2 = 27
3 = 2
sum = 1024
```

Accessors expose immutable source semantics. Recovered runtime transformations belong in explicit consumers/overlays rather than hidden decode behavior.

## `EspMapState` — hardware validated

The branch adds the first mutable native world/spatial state:

```text
EspMapState
    uint8 tileFlags[1024]
```

Persistent payload and actual classic-CYD cost:

```text
payload            = 1024 B
actual heap use    = 1040 B
allocator overhead = 16 B
largest8           = 36852 -> 36852
```

Recovered tile bits:

```text
WALL     = 1
SECRET   = 2
ENTRANCE = 4
EVENTS   = 8
VISITED  = 16
```

Initial-state construction uses only the hardware-proven native access API:

```text
block-map cells -> low two state bits
texture-7 lines -> ENTRANCE
qualifying events -> EVENTS
VISITED -> deliberately absent at initial build
```

### Entrance semantic

The reference loader applies line coordinate nudges before locating texture-7 entrance cells:

```text
horizontal(512) + east/south(8)  -> x += 3
horizontal(512) + west/north(16) -> x -= 3
vertical(256)   + east/south(8)  -> y += 3
vertical(256)   + west/north(16) -> y -= 3
```

Then it computes the midpoint and shifts by 6 to obtain the 32x32 tile coordinate.

Hardware topology:

```text
texture-7 line refs = 4
unique entrance cells = 4
first entrance tile = 68
```

### Event semantic

Recovered loader rule:

```text
if event & 0x01f80000:
    tileFlags[event & 1023] |= EVENTS
```

Hardware topology:

```text
source events = 93
qualifying event refs = 93
unique event cells = 93
first event tile = 68
```

Every event in MAP_INTRO qualifies, and each maps to a unique event cell.

Tile `68` is both entrance-bearing and event-bearing, confirming that the state representation must support composable bits.

### Visited remains deferred

Initial state contains:

```text
visited cells = 0
```

Visited/save/automap mutation remains a later gameplay concern.

### Canonical state proof

```text
stateFNV = cd99b98e
build+verification elapsed = 9 ms
base counts = 298 / 697 / 27 / 2
```

RAM/integrity proof:

```text
heap8       = 70056 -> 69016
used        = 1040 B
payload     = 1024 B
overhead    = 16 B
largest8    = 36852 -> 36852
frameFNV    = unchanged
arenaFNV    = c3882516 -> c3882516
entities    = 0
monsters    = 0
ST_PLAYING  = no
```

Later `[ALIVE]` heartbeats remained stable at `heap8=69016`, `largest8=36852`.

## Combined native memory result

Measured current persistent map/world foundation:

```text
immutable arena actual heap = 14112 B
mutable tile state actual   =  1040 B
-------------------------------------
combined actual heap        = 15152 B
```

Compared with the measured legacy structural allocation:

```text
legacy structural = 55341 B
native arena+state = 15152 B
saved              = 40189 B
reduction          ~= 72.6%
```

The native side now includes both structural source state and the first mutable spatial/gameplay substrate.

## Execution path now proven

```text
validated intro disposal
    -> native BSP inventory/plan
    -> compact resident arena
    -> native accessor sweep
    -> 1024-byte native mutable tile-state build
    -> independent semantic state verification
    -> PARK
```

Still absent:

```text
shapeData/mediaTexels
complete raw BSP allocation
legacy Render_beginLoadMapData()
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

Detailed old menu/touch/LRU/FNV measurements remain in archived recovery snapshots and milestone documents.

## Merge recommendation

**MERGE `agent/esp32-map1-native-state`.**

Hardware-tested code:

```text
9a17654b56a190932615bba4894e90debd0e3773
```

If all commits after that SHA remain documentation-only, no additional hardware flash is required.

## Next bounded milestone after merge

The event topology is now a strong consumer signal:

```text
93 event refs
93 unique event-bearing tiles
```

Preferred next step: implement a compact native **tile -> event lookup/index contract** so gameplay can resolve an event from `EspMapState` without desktop `tileEvents` ownership.

Candidate shape should be derived from actual event-query behavior rather than copied from `Render_t`. Preserve the immutable arena and current 1 KiB state; do not advance to the desktop entity graph simply because richer gameplay eventually needs entities.
