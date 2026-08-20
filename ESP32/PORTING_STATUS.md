# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for the stable build/architecture guide, [`DOCUMENTATION.md`](DOCUMENTATION.md) for documentation ownership rules, and milestone documents for detailed hardware evidence.

The older full recovery catalog remains preserved in [`archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md`](archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md).

## Latest merged hardware baseline

```text
PR   = #42 — MAP_INTRO structural feasibility + native BSP pass 1
main = c71ac1fb07c2e281bc3f8a70c102dd22c7b9300e
```

PR #42 merged the first reusable native BSP reader and hardware-validated compact MAP_INTRO structural plan.

Current development candidate:

```text
branch = agent/esp32-map1-native-runtime
base   = c71ac1fb07c2e281bc3f8a70c102dd22c7b9300e
code   = ed4ddda37d941ce6acb01148f92b6f5aebe2a275
status = NATIVE MAP RUNTIME IMPLEMENTED; AWAITING REAL-CYD HARDWARE PASS
```

Detailed current milestone: [`MAP1_NATIVE_RUNTIME.md`](MAP1_NATIVE_RUNTIME.md).

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

Desktop-derived `Render_t`, `DoomCanvas_t`, pointer-heavy map structures and linker wrappers remain temporary migration scaffolding.

## Current hardware-safe boundary inherited from PR #42

Normal firmware reaches:

```text
menu                    = MENU_NONE
state                   = ST_INTRO (9)
storyPage               = 3
storyTextPage           = 0
intro clock/input       = inactive
intro images/texts      = NULL
render clip             = off
startupMap              = 1 (MAP_INTRO / /intro.bsp)
heap8                   = 84376 on PR #42 candidate build
largest8                = 36852
legacy nodes/lines      = NULL
legacy mapSprites       = NULL
legacy mappings         = NULL
shapeData               = NULL
mediaTexels             = NULL
wall/sprite LRU caches  = inactive
entities/monsters       = 0
legacy gameplay loader  = NOT called
```

At that boundary the native BSP reader consumed `/intro.bsp` with zero heap/largest-block/framebuffer drift.

## MAP_INTRO hardware reference

```text
MAP_INTRO   = 1
file        = /intro.bsp
name        = Entrance
sourceBytes = 21823
CRC32       = 623f34e4
FNV-1a      = d5cc751f
read window = 256 B
readCalls   = 86
scan time   = 161 ms for inventory+resource plan
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

Payload-relative section offsets:

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

## Rejected legacy map lifecycle

Measured legacy structural payload:

```text
legacy structural        = 55341 B
mappings resident cost   = 8440 B
raw /intro.bsp            = 21823 B
zero-headroom deficit     = 1237 B
4096-B-headroom deficit   = 5333 B
```

The fail-closed hardware probe proved that keeping the complete raw BSP resident while building pointer-heavy runtime structures is the wrong architecture for the no-PSRAM CYD.

## Hardware-validated compact plan

PR #42 established this exact persistent structural baseline:

```text
nodes          = 2230 B
lines          = 4800 B
mapSprites     = 1720 B
events         = 372 B
byteCodes      = 2385 B
stringOffsets  = 188 B
blockMap       = 256 B
planeMap       = 2048 B
resourceSets   = 96 B
-------------------------
persistent     = 14095 B
```

Compared with legacy:

```text
55341 -> 14095 B
saved = 41246 B
reduction ~= 74.5%
```

This is an immutable structural base only. Mutable entities, doors/scripts, runtime links, dynamic sprite pools, caches, rendering work buffers and string scratch remain separate future overlays/working sets.

## Current unvalidated implementation

`esp_map_runtime.c/.h` now allocates **one exact 14,095-byte byte-addressed arena** and populates it directly from `DoomRPG-ESP32.pak`.

Arena layout:

```text
+0      nodes          2230 B
+2230   lines          4800 B
+7030   mapSprites     1720 B
+8750   events          372 B
+9122   byteCodes      2385 B
+11507  stringOffsets   188 B
+11695  blockMap        256 B
+11951  planeMap       2048 B
+13999  texture bitset   32 B
+14031  sprite bitset    32 B
+14063  plane bitset     32 B
end                    14095 B
```

Byte-addressed storage is deliberate: no hidden C struct padding/alignment may inflate the measured plan. String offsets are stored packed little-endian and accessed through a helper instead of unaligned casts.

The string payload remains on SD. The runtime builds the 94 payload offsets using a bounded 256-byte cached scan of the string section.

The new runtime records:

```text
arenaFNV
populateReadCalls
populateElapsedMs
heap8 before/after
allocator overhead over 14095 B
largest8 before/after
framebuffer FNV before/after
```

## Current execution path candidate

```text
merged menu/start/intro path
    -> validated intro disposal
    -> ST_INTRO page 3
    -> PR #42 native BSP pass 1
    -> PARK
    -> native runtime loader ARMED
    -> refresh BSP inventory (temporary validation scaffold)
    -> allocate 14095 B native arena
    -> direct readRange section population
    -> build string-offset table; text stays on SD
    -> copy resource bitsets
    -> PARK with native arena resident
```

Still forbidden:

```text
shapeData/mediaTexels
complete raw BSP resident allocation
legacy Render_beginLoadMapData()
entity/monster spawn
player spawn
map rendering
ST_PLAYING
continuous gameplay loop
```

## Hardware PASS target for current branch

Normal `esp32-cyd` must prove:

```text
PR #42 pass1 still PASS
arenaBytes = 14095
native arena remains resident
legacy runtime remains NULL
entities/monsters = 0
framebuffer unchanged
heap consumption >= 14095 B
allocator overhead measured
largest8 measured after allocation
stable later ALIVE heartbeats
```

The exact heap/largest-block result is intentionally not predicted. This milestone exists to measure allocator overhead and fragmentation on the real CYD.

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

Detailed old menu/touch/LRU/FNV measurements remain in the archived recovery snapshot.

## Next step

First build/flash the current branch and validate the resident 14,095-byte arena.

If hardware passes, document exact arena FNV, population timing/read count, allocator overhead and new persistent heap boundary, then merge. The following branch should add only the smallest required mutable index-based overlays/first consumer while keeping rendering/game activation bounded.
