# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for stable build/architecture guidance, [`DOCUMENTATION.md`](DOCUMENTATION.md) for documentation ownership rules, and milestone documents for detailed hardware evidence.

The older full recovery catalog remains preserved in [`archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md`](archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md).

## Latest merged hardware baseline

```text
PR   = #43 — persistent compact native MAP_INTRO arena
main = 503fdd66fae625a45446fb4ea0853abc71d7dda3
```

PR #43 merged the first persistent gameplay-map structure owned by the ESP32-native runtime.

Current development candidate:

```text
branch = agent/esp32-map1-native-access
base   = 503fdd66fae625a45446fb4ea0853abc71d7dda3
hardware-affecting head = 22981ff9e9323034e56c2d0527a5c87f683622df
status = NATIVE COMPACT ACCESSORS IMPLEMENTED; AWAITING REAL-CYD HARDWARE PASS
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

## Current merged hardware-safe boundary

Normal optimized firmware at PR #43 reaches:

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
heap8                   = 70128 after resident load on PR #43 candidate
largest8                = 36852
arenaFNV                = c3882516
ST_PLAYING              = NOT entered
```

The resident arena is byte-addressed and immutable. Mutable future state belongs in separate explicit index-based overlays.

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

PR #43 real-CYD result:

```text
arenaBytes         = 14095
actual heap use    = 14112 B
allocator overhead = 17 B
populateReadCalls  = 33
populateElapsed    = 62 ms
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

## Current native access candidate

The new branch adds allocation-free, bounds-checked decoding over the resident compact arena.

Public native value contracts:

```text
EspMapNode
  source x1/y1/x2/y2 expanded by byte << 3
  args1/args2 decoded from the 10-byte BSP record

EspMapLine
  source x1/y1/x2/y2
  texture uint16
  flags uint32

EspMapSprite
  source x/y
  source info uint32

EspMapByteCode
  id uint8
  arg1/arg2 uint32
```

Additional accessors expose:

```text
events by index
string source offsets
1024 packed block-map cells
2 x 1024 plane texture IDs
required texture/sprite/plane-resource membership
```

### Important source/runtime separation

The accessors expose **immutable source semantics** only.

They deliberately do not hide later desktop runtime mutations such as:

```text
line coordinate +/-3 nudges
line derived Z/length values
sprite +/-1 nudges
sprite BSP relinking
runtime-only sprite flags
entity pointers / linked lists
```

Any behavior that the final game needs will be implemented explicitly by the appropriate native consumer or mutable overlay.

## Current access-validation path

After the merged PR #43 resident loader succeeds:

```text
resident EspMapRuntime
    -> MAPACCESS arm
    -> decode every node/line/sprite/event/bytecode through public accessors
    -> independently compare decoded values against compact source bytes
    -> validate all 94 string offsets
    -> validate all 1024 block cells
    -> validate both 1024-cell plane maps
    -> validate 256-ID resource sets = 33 / 45 / 12
    -> verify out-of-range access fails
    -> compute canonical decoded FNV
    -> require zero heap/largest8/framebuffer drift
    -> PARK
```

Expected inherited arena regression:

```text
arenaBytes = 14095
arenaFNV   = c3882516
```

The first real-CYD run will establish:

```text
decodedFNV
accessor elapsed time
first decoded node/line/sprite/event/bytecode sample
first/last string payload offsets
block-map 2-bit value distribution
exact new build heap baseline
```

Accessor execution itself must remain allocation-free and produce:

```text
heap8     X -> X
largest8  Y -> Y
frameFNV  Z -> Z
```

## Current temporary load timing

The inherited validation scaffolding still performs:

```text
pass1 inventory       ~= 166 ms on PR #43 build
second inventory      ~= 145 ms  # temporary duplicate
arena population       = 62 ms
```

The duplicate inventory remains a temporary validation artifact. A future native orchestrator should carry one validated inventory directly into allocation/population.

## Still forbidden

```text
shapeData/mediaTexels
complete raw BSP allocation
legacy Render_beginLoadMapData()
mutable entity/monster activation
player spawn
hidden line/sprite mutation inside accessors
map rendering
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

## Next action

Build/flash the current branch on normal `esp32-cyd` and capture the `MAPACCESS` block plus later stable `[ALIVE]` heartbeats.

If the accessor probe passes with zero drift, document the canonical decoded FNV and samples, then decide whether this branch is a coherent merge boundary or whether the first tiny native mutable/spatial consumer should remain on the same branch.
