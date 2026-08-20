# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for stable build/architecture guidance, [`DOCUMENTATION.md`](DOCUMENTATION.md) for documentation ownership rules, and milestone documents for detailed hardware evidence.

The older full recovery catalog remains preserved in [`archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md`](archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md).

## Latest merged hardware baseline

```text
PR   = #44 — native compact MAP_INTRO access contract
main = ddcf19e6166f210a6f63fec1c608234ee3e253ea
```

PR #44 hardware-proved that the complete resident MAP_INTRO structure can be consumed through allocation-free native indexed accessors with canonical `decodedFNV=a426dd18` and zero heap/largest-block/framebuffer drift.

Current candidate:

```text
branch = agent/esp32-map1-native-state
base   = ddcf19e6166f210a6f63fec1c608234ee3e253ea
hardware-affecting head = 9a17654b56a190932615bba4894e90debd0e3773
status = FIRST NATIVE MUTABLE TILE STATE IMPLEMENTED; AWAITING REAL-CYD HARDWARE PASS
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

## Current merged hardware-safe boundary

Normal optimized firmware at PR #44 reaches:

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
heap8                   = 70112 after resident load on PR #44 test build
largest8                = 36852
arenaFNV                = c3882516
decodedFNV              = a426dd18
ST_PLAYING              = NOT entered
```

The PR #44 access sweep took 3 ms and left heap8, largest8, framebuffer FNV and arena contents unchanged.

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
populateElapsed        ~= 63 ms
arenaFNV               = c3882516
largest8               = 36852 preserved
```

Legacy structural runtime measured `55341 B`, so the native resident map saves about 41.2 KiB / 74.5% versus the pointer-heavy structural allocation.

Strings remain on SD; only 188 B of payload offsets are resident.

## Native access contract — hardware validated

PR #44 swept every public native accessor over:

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

Block-map distribution:

```text
0 = 298
1 = 697
2 = 27
3 = 2
sum = 1024
```

Hardware result:

```text
elapsed    = 3 ms
heap8      = 70112 -> 70112
largest8   = 36852 -> 36852
frameFNV   = unchanged
arenaFNV   = c3882516 unchanged
```

Accessors expose immutable source semantics. Recovered runtime transformations belong in explicit consumers/overlays rather than hidden decode behavior.

## Current candidate: `EspMapState`

The branch adds the first mutable native world/spatial state:

```text
EspMapState
    uint8 tileFlags[1024]
```

Planned payload:

```text
32 x 32 x 1 B = 1024 B
```

It is a separate allocation from the immutable 14,095-byte arena because source structure and mutable world state have different ownership/lifecycles.

Recovered tile bits:

```text
WALL     = 1
SECRET   = 2
ENTRANCE = 4
EVENTS   = 8
VISITED  = 16
```

Initial-state construction uses only the PR #44 access API:

```text
block-map cells -> low two state bits
texture-7 lines -> ENTRANCE
qualifying events -> EVENTS
```

### Entrance semantic is intentionally in the consumer

The reference loader applies line coordinate nudges before locating texture-7 entrance cells:

```text
horizontal(512) + east/south(8)  -> x += 3
horizontal(512) + west/north(16) -> x -= 3
vertical(256)   + east/south(8)  -> y += 3
vertical(256)   + west/north(16) -> y -= 3
```

Then it computes the midpoint and shifts by 6 to obtain the 32x32 tile coordinate.

The immutable line accessor remains raw; `EspMapState` now owns this recovered runtime semantic explicitly.

### Event semantic

Recovered loader rule:

```text
if event & 0x01f80000:
    tileFlags[event & 1023] |= EVENTS
```

### Visited remains deferred

Initial state must contain zero `VISITED` bits. The reference applies visited semantics later during world/entity activation and save/load, so that behavior is intentionally not mixed into this milestone.

## Candidate hardware gate

After the existing `MAPACCESS` PASS, the new stage runs:

```text
MAPSTATEPROBE
```

It must prove:

```text
payload                  = 1024 B
base counts              = 298 / 697 / 27 / 2
visited cells            = 0
all entrance tiles       = exact from texture-7 line semantics
all event tiles          = exact from event mask/index semantics
state tile bounds        = fail closed
allocator overhead       <= 64 B
largest8 after state     >= 32768 B
framebuffer              = unchanged
arenaFNV                 = c3882516 unchanged
entities/monsters        = 0
ST_PLAYING               = no
```

The real-CYD run must establish:

```text
stateFNV
build elapsed
entrance refs / unique cells
event refs / unique cells
first entrance/event tiles
actual heap cost + allocator overhead
largest8 result
```

## Current execution path candidate

```text
validated intro disposal
    -> native BSP inventory/plan
    -> compact resident arena
    -> native accessor sweep
    -> 1024-byte native mutable tile-state build
    -> semantic state verification
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

Detailed old menu/touch/LRU/FNV measurements remain in the archived recovery snapshot and milestone documents.

## Next action

Build/flash `agent/esp32-map1-native-state` with normal `esp32-cyd`, progress through the intro, and capture the new `MAPSTATE` / `MAPSTATEPROBE` block plus later stable `[ALIVE]` heartbeats.

If it passes, record the canonical `stateFNV`, exact 1 KiB allocator cost and entrance/event topology, then decide merge. Do not advance to entities or `ST_PLAYING` on an unproven tile-state base.
