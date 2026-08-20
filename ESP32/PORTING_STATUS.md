# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for stable build/architecture guidance, [`DOCUMENTATION.md`](DOCUMENTATION.md) for documentation ownership rules, and milestone documents for detailed hardware evidence.

The older full recovery catalog remains preserved in [`archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md`](archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md).

## Latest merged hardware baseline

```text
PR   = #43 — persistent compact native MAP_INTRO arena
main = 503fdd66fae625a45446fb4ea0853abc71d7dda3
```

Current candidate:

```text
branch = agent/esp32-map1-native-access
base   = 503fdd66fae625a45446fb4ea0853abc71d7dda3
hardware-tested code = dfe25218b74db9d2765850fbc29057e703c57154
status = REAL-CYD HARDWARE PASS; NATIVE COMPACT ACCESS CONTRACT VALIDATED; MERGE-READY
```

Detailed active milestone: [`MAP1_NATIVE_ACCESS.md`](MAP1_NATIVE_ACCESS.md).

Merged resident-runtime evidence: [`MAP1_NATIVE_RUNTIME.md`](MAP1_NATIVE_RUNTIME.md).

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
native map arena        = RESIDENT + IMMUTABLE
native arena payload    = 14095 B
actual heap cost        = 14112 B
heap8                   = 70112 after resident load on current tested build
largest8                = 36852
arenaFNV                = c3882516
decodedFNV              = a426dd18
ST_PLAYING              = NOT entered
```

The resident arena is byte-addressed and immutable. Native consumers access it allocation-free by index; mutable future state belongs in separate explicit index-based overlays.

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

Payload-relative source offsets:

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

`textureReq=33` and `spriteReq=45` are map-derived dependencies, not a declaration of every future global gameplay resource.

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

## Native compact arena — hardware validated

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

Current real-CYD regression:

```text
arenaBytes         = 14095
actual heap use    = 14112 B
allocator overhead = 17 B
populateReadCalls  = 33
populateElapsed    = 63 ms
arenaFNV           = c3882516
largest8           = 36852 -> 36852
frameFNV           = unchanged
```

Compared with legacy structural allocation:

```text
55341 -> 14112 B actual heap use
saved = 41229 B
reduction ~= 74.5%
```

Strings remain on SD; only 188 B of little-endian payload offsets are resident.

## Native compact access contract — hardware validated

Allocation-free, bounds-checked native accessors expose:

```text
EspMapRuntime_getNode()
EspMapRuntime_getLine()
EspMapRuntime_getMapSprite()
EspMapRuntime_getEvent()
EspMapRuntime_getByteCode()
EspMapRuntime_getStringSourceOffset()
EspMapRuntime_getBlockCell()
EspMapRuntime_getPlaneTexture()
EspMapRuntime_textureRequired()
EspMapRuntime_spriteRequired()
EspMapRuntime_planeTextureUsed()
```

They expose **immutable source semantics**, not later desktop runtime mutations such as line +/-3 nudges, sprite +/-1 nudges, BSP relinking, derived line Z/length or entity pointers.

The real CYD fully swept:

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
all family bounds checks
```

Hardware result:

```text
decodedFNV = a426dd18
elapsed    = 3 ms
heap8      = 70112 -> 70112
largest8   = 36852 -> 36852
frameFNV   = 1f4d9cd6 -> 1f4d9cd6
arenaFNV   = c3882516 unchanged
```

Decoded reference sample:

```text
node0   = 64,128-1984,1984 args=00010400/00640001
line0   = 1728,1440-1696,1472 tex=87 flags=00000000
sprite0 = 160,1440 info=00010091
event0  = 00080044
code0   = 16/000001cb/000000d0
string0 = 11554
```

Block-map distribution:

```text
0 = 298
1 = 697
2 = 27
3 = 2
sum = 1024
```

String payload offsets span `11554..19512`. Resource-set sweep reproduced exactly `33 / 45 / 12`. Out-of-range accesses fail closed.

A later heartbeat remained stable at `heap8=70112`, `largest8=36852`.

## Current temporary load timing

The validation scaffolding currently does:

```text
pass1 inventory       = 166 ms
second inventory      = 145 ms  # temporary duplicate
arena population      = 63 ms
accessor full sweep   =   3 ms
```

The duplicate inventory remains a temporary validation artifact. A future native orchestrator should carry one validated inventory directly into allocation/population.

## Execution path now proven

```text
menu/start/intro path
    -> validated intro disposal
    -> native BSP inventory/plan
    -> one compact native arena allocation
    -> direct .pak population
    -> resident arena FNV proof
    -> allocation-free indexed accessors
    -> full semantic access sweep + decoded FNV proof
    -> PARK
```

Still absent:

```text
shapeData/mediaTexels
complete raw BSP allocation
legacy Render_beginLoadMapData()
mutable world overlays
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

**MERGE this branch. No additional hardware test is required if later commits remain documentation-only.**

Hardware-tested code:

```text
dfe25218b74db9d2765850fbc29057e703c57154
```

Two inherited cosmetic Serial spacing defects remain harmless and are intentionally not changed after the hardware pass.

## Next bounded milestone after merge

Branch fresh from the new `main`.

Create the first genuinely useful small mutable consumer: a native spatial tile-state overlay.

Preferred design:

```text
EspMapState
    uint8_t tileFlags[1024]
```

Initialization should use only the hardware-proven access API:

```text
1. copy/decode the 2-bit block-map base into tileFlags
2. walk lines and derive BIT_AM_ENTRANCE cells from texture 7 semantics
3. walk events and OR BIT_AM_EVENTS into referenced cells
4. preserve EspMapRuntime byte-for-byte (`arenaFNV=c3882516`)
5. measure the exact ~1 KiB persistent mutable cost
6. PARK before entities, rendering, player spawn or ST_PLAYING
```

This gives the native engine its first real mutable world/spatial state without recreating desktop pointer graphs.
