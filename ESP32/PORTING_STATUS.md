# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for stable build/architecture guidance, [`DOCUMENTATION.md`](DOCUMENTATION.md) for documentation ownership rules, and milestone documents for detailed hardware evidence.

The older full recovery catalog remains preserved in [`archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md`](archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md).

## Latest merged hardware baseline

```text
PR   = #42 — MAP_INTRO structural feasibility + native BSP pass 1
main = c71ac1fb07c2e281bc3f8a70c102dd22c7b9300e
```

Current candidate:

```text
branch = agent/esp32-map1-native-runtime
base   = c71ac1fb07c2e281bc3f8a70c102dd22c7b9300e
hardware-tested code = ed4ddda37d941ce6acb01148f92b6f5aebe2a275
status = REAL-CYD HARDWARE PASS; NATIVE MAP ARENA RESIDENT; MERGE-READY
```

Documentation-only commits may follow the hardware-tested code without invalidating the run.

Detailed milestone: [`MAP1_NATIVE_RUNTIME.md`](MAP1_NATIVE_RUNTIME.md).

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

Desktop-derived `Render_t`, `DoomCanvas_t`, pointer-heavy map structures and linker wrappers are migration scaffolding, not permanent architecture requirements.

## Current hardware-safe boundary

Normal optimized firmware now reaches:

```text
menu                    = MENU_NONE
state                   = ST_INTRO (9)
storyPage               = 3
storyTextPage           = 0
intro clock/input       = inactive
intro images/texts      = NULL
render clip             = off
startupMap              = 1 (MAP_INTRO / /intro.bsp)
legacy nodes/lines      = NULL
legacy mapSprites       = NULL
legacy mappings         = NULL
shapeData               = NULL
mediaTexels             = NULL
wall/sprite LRU caches  = inactive
entities/monsters       = 0
legacy gameplay loader  = NOT called
native map arena        = RESIDENT
native arena payload    = 14095 B
actual heap cost        = 14112 B
heap8                   = 70128 after resident load
largest8                = 36852
ST_PLAYING              = NOT entered
```

The current resident arena is the first persistent gameplay-map structure owned by the native ESP32 runtime.

## MAP_INTRO source reference

```text
MAP_INTRO   = 1
file        = /intro.bsp
name        = Entrance
sourceBytes = 21823
CRC32       = 623f34e4
FNV-1a      = d5cc751f
read window = 256 B
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

`textureReq=33` and `spriteReq=45` remain map-derived dependencies, not a declaration of every future global gameplay resource.

## Rejected legacy map lifecycle

Measured legacy structural payload:

```text
legacy structural        = 55341 B
mappings resident cost   = 8440 B
raw /intro.bsp            = 21823 B
zero-headroom deficit     = 1237 B
4096-B-headroom deficit   = 5333 B
```

The fail-closed hardware probe proved that retaining the complete raw BSP while building pointer-heavy runtime structures is the wrong no-PSRAM architecture.

## Native compact arena

Exact resident payload:

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
-----------------------------
payload                14095 B
```

The arena is byte-addressed to prevent hidden C padding. Original compact BSP records remain compact. Mutable future state belongs in separate index-based overlays.

Strings remain on SD; only 188 B of little-endian source offsets are resident.

## Real-CYD resident-runtime proof

PR #42 pass regression on the new build:

```text
heap8       84240 -> 84240
largest8    36852 -> 36852
frameFNV    11b4cc0e -> 11b4cc0e
readCalls   = 86
elapsed     = 166 ms
FNV-1a      = d5cc751f
CRC32       = 623f34e4
```

Resident population:

```text
arenaBytes         = 14095
populateReadCalls  = 33
populateElapsed    = 62 ms
arenaFNV           = c3882516
sourceCRC          = 623f34e4
```

Exact heap measurement:

```text
heap8     84240 -> 70128
used              14112 B
payload            14095 B
allocator overhead    17 B
largest8  36852 -> 36852
frameFNV  11b4cc0e -> 11b4cc0e
```

Compared with legacy structural allocation:

```text
55341 -> 14112 B actual heap use
saved = 41229 B
reduction ~= 74.5%
```

The largest free 8-bit block staying at `36852` is a strong fragmentation result: the arena did not consume the largest region.

Stable later heartbeat:

```text
heap8     = 70128
largest8  = 36852
```

No OOM, reset, leak, framebuffer mutation, entity spawn or hidden gameplay transition occurred.

## Current temporary load timing

The validation scaffolding currently does:

```text
PR #42 pass1 inventory = 166 ms
second inventory       = 145 ms  # deliberate temporary duplicate
arena population       = 62 ms
--------------------------------
current validation     ~= 373 ms
```

The second inventory exists only to leave the already hardware-validated pass1 code untouched. A future native orchestrator should carry the validated inventory directly into allocation/population and remove that duplicate scan.

## Execution path now proven

```text
menu/start/intro path
    -> validated intro disposal
    -> ST_INTRO page 3
    -> native BSP inventory/plan
    -> one 14095-B native structural arena allocation
    -> direct .pak section population
    -> string-offset table; text stays on SD
    -> resident arena FNV proof
    -> PARK with arena resident
```

Still absent:

```text
shapeData/mediaTexels
complete raw BSP allocation
legacy Render_beginLoadMapData()
entity/monster activation
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

Detailed old menu/touch/LRU/FNV measurements remain in the archived recovery snapshot.

## Merge recommendation

**Merge this branch. No additional hardware test is required for this milestone.**

The hardware-affecting code was tested at:

```text
ed4ddda37d941ce6acb01148f92b6f5aebe2a275
```

Later changes should remain documentation-only before merge.

Two cosmetic Serial spacing defects are intentionally left in the tested binary and do not affect behavior.

## Next bounded milestone after merge

Branch fresh from the new `main`.

Objective: make the resident structural base useful through the smallest native indexed access/mutable layer needed by the first consumer.

Preferred direction:

```text
resident immutable EspMapRuntime
    -> decode/access compact records by index
    -> add only necessary mutable overlays using integer indices
    -> establish one bounded first consumer
    -> measure exact RAM/state cost
    -> PARK
```

Do not recreate desktop pointer-heavy per-record ownership. Keep full entity/world activation, player spawn, `ST_PLAYING` and the gameplay render loop as later measured milestones.
