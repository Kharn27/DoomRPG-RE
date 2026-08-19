# Doom RPG ESP32 port

This directory contains the ESP32-specific port and bring-up work for the classic
ESP32-2432S028R Cheap Yellow Display with **no PSRAM**.

The target is treated as its own constrained engine architecture. DoomRPG-RE is
used as a behavioural/data-format reference while resource management and
rendering are progressively rebuilt for the real hardware.

For the authoritative branch state, hardware measurements and exact recovery
boundary, see [`PORTING_STATUS.md`](PORTING_STATUS.md).

## Current target

- ESP32-2432S028R / classic CYD, no PSRAM
- ESP32-D0WD-V3, 240 MHz, 4 MB flash
- ILI9341 320x240 landscape
- XPT2046 touch
- internal render target: 160x120 RGB565
- gameplay viewport: 160x80 at framebuffer y=20
- physical output: exact nearest-neighbour 2x to 320x240
- microSD-backed resources
- audio disabled during bring-up

## Build and flash

From the repository root:

```bash
cd ESP32
pio run -t upload
pio device monitor
```

A clean build is normally unnecessary. Use it when generated-source or linker
configuration changes make it useful:

```bash
cd ESP32
pio run -t clean
pio run -t upload
```

## SD card contents

During the migration the card contains:

```text
/DoomRPG.zip
/DoomRPG-ESP32.pak
```

`DoomRPG.zip` remains only for legacy paths not migrated yet.
`DoomRPG-ESP32.pak` is the native direct-access resource backend.

## Building the native asset pack

```bash
python3 ESP32/tools/build_asset_pack.py \
    /path/to/DoomRPG.zip \
    /path/to/DoomRPG-ESP32.pak
```

Example directly to a mounted SD:

```bash
python3 ESP32/tools/build_asset_pack.py \
    /media/$USER/DOOMRPG/DoomRPG.zip \
    /media/$USER/DOOMRPG/DoomRPG-ESP32.pak
```

List records while building:

```bash
python3 ESP32/tools/build_asset_pack.py --list \
    /path/to/DoomRPG.zip \
    /path/to/DoomRPG-ESP32.pak
```

The builder must finish with:

```text
[PACK] self-check: OK
```

Validated pack v2 for 241 resources:

```text
header                   24 B
241 index records x 20  4820 B
------------------------------
index/data boundary     4844 B
```

Payloads are uncompressed and directly seekable. The full index is not kept
resident in RAM.

## Why the native path exists

The original graphics architecture does not fit a classic no-PSRAM ESP32.
Measured menu-map legacy sizes include:

```text
wall-only legacy mediaTexels = 172,032 B
selected sprite texels       = 143,990 B
expanded shapeData           = 55,676 B
```

The native engine instead works with bounded frames:

```text
largest validated sprite frame = 2,112 B
one packed wall texture         = 2,048 B
3-slot wall cache payload       = 6,144 B
3-slot sprite cache peak        = 6,038 B on the reference frame
```

## Strong invariant

Do not reintroduce the legacy monolithic graphics pools:

```text
shapeData   = NULL
mediaTexels = NULL
```

Native consumers use small GFXRM frames and measured caches instead.

## Native palette

Native consumers use canonical RGB565 in the shared framebuffer. The ESP32 path
normalizes the reconstructed legacy palette once before native rendering.

Validated diagnostic:

```text
[PALETTE] Normalized 3280 entries legacy R/B order -> framebuffer RGB565
[PALETTE] sprite172 offset=1616 first4 before=0000,ffff,c000,07ff after=0000,ffff,0018,ffe0
```

Do not modify source texel nibble order to compensate for palette ordering.

## Graphics resource manager (GFXRM)

GFXRM owns pack/file-format knowledge and physical bounded loads:

```text
                         render/cache consumers
                                  |
                                  v
               NativeGraphicsResourceManager
                                  |
                         DoomRPG-ESP32.pak
                         /       |       \
                 bitshapes   stexels   wtexels
```

Public frame API:

```c
EspNativeGraphics_loadSpriteFrame(render, spriteIndex, &frame);
EspNativeGraphics_releaseSpriteFrame(&frame);

EspNativeGraphics_loadWallFrame(render, textureIndex, &frame);
EspNativeGraphics_releaseWallFrame(&frame);
```

Cache policy is deliberately outside GFXRM.

## Native sprite frame contract

A sprite frame contains only:

```text
shape bounds/header
bitshape mask
packed active stexels (4 bpp)
palette offset
```

Worst validated menu frame, sprite 172:

```text
size                = 64x64
active pixels       = 3199
mask                = 512 B
packed texels       = 1600 B
logical frame       = 2112 B
allocator cost      = 2128 B
palette offset      = 1616
texel FNV1a         = 0c0a7acd
```

## Native wall contract

Validated texture 112:

```text
size                = 64x64
packed texels       = 2048 B
allocator cost      = 2064 B
source texel offset = 65536
palette offset      = 480
texel FNV1a         = 92d40704
```

Wall logical texel layout is column-major:

```text
logical texel index = x * 64 + y
```

## Projected wall path

The engine preserves useful original transform/clip/projection behaviour, but
samples bounded wall frames directly:

```text
real Line_t / BSP geometry
        |
        v
original transform / clip / projection
        |
        v
ESP32-native wall-span geometry
        |
        v
bounded packed wall frame
        |
        v
shared RGB565 framebuffer
```

Synthetic projected-wall regression:

```text
framebuffer FNV = ad191f54
```

Global `mediaTexelOffsets[]` are never rewritten to fake a monolithic local pool.

## Real menu scene

Deterministic camera:

```text
spawnIndex = 460
spawn tile = 12,14
world      = 800,928
direction  = 0
camera Z   = 36
animFrameTime = 0
```

The original renderer still provides BSP traversal, visibility, leaf ordering,
occlusion and `viewSprites` ordering. ESP32-native code supplies bounded resource
loading and rasterization.

### Walls regression

```text
BSP nodeCount        = 28
BSP nodeRasterCount  = 10
visible leaves       = 10
source lines         = 46
wall requests        = 25
backface culled      = 15
clip culled          = 6
request FNV          = 4db9da28
native span calls    = 224
native pixels        = 5589
framebuffer FNV      = a6d87c4a
floor RGB565         = 4208
ceiling RGB565       = 8c51
```

Exact wall request stream:

```text
116, 32, 40, 112, 108, 108, 116, 116, 116, 112,
116, 112, 108, 108, 108, 116, 44, 0, 40, 152,
152, 116, 116, 116, 152
```

## Hardware-validated 3-slot wall LRU

```text
requests               = 25
hits                   = 14
misses                 = 11
evictions               = 8
logical cache payload  = 6,144 B
allocator cache cost   = 6,192 B
physical wall loads    = 11
pack opens             = 11
wall bytes read        = 22,528 B
framebuffer FNV        = a6d87c4a
```

The cache changes lifetime/I/O only; cached and uncached wall frames are
bit-identical.

## Real BSP-sorted sprites

`Render_walkNode()` creates the original `sortZ`-ordered `viewSprites` list. The
native projected-sprite renderer consumes that list directly.

```text
real menu BSP
     |
     v
Render_walkNode
     |
     v
viewSprites
     |
     v
mediaSpriteIds / animation resolution
     |
     +-------------------------------+
     |                               |
bitshape-backed                 wall-backed
     |                               |
     v                               v
bounded sprite frame          bounded wall frame
(mask + stexels)                    |
     |                              |
     v                              v
native sprite spans         native wall spans
     \                              /
      \                            /
        shared 160x120 framebuffer
```

The native sprite renderer reconstructs opaque runs directly from the bounded
bitshape mask. It does not recreate expanded map-wide `shapeData`.

Span modes 0..9 are implemented. The current reference scene uses normal mode 0
and mode 7.

## Deterministic sprite regression

```text
viewSprites count           = 17
viewSprites list FNV        = 962cd657
hidden objects              = 0
lights skipped              = 5
resolved draws              = 14
sprite requests             = 11
unique sprite frames        = 8
repeated requests           = 3
sprite request FNV          = 4457ac94
wall-backed objects         = 2
near culled                 = 1
clip culled                 = 1
sprite span calls           = 389
sprite pixels               = 4590
wall-backed spans           = 20
wall-backed pixels          = 340
final walls+sprites FNV     = ffe0995e
```

Exact sprite request stream:

```text
request  object  mediaId  mode
1        17      562      0
2        41      406      0
3        41      410      7
4        34      598      0
5        27      172      0
6        28      578      0
7        21      578      0
8        23      426      0
9        23      410      7
10       18      578      0
11       16      102      0
```

Sequence only:

```text
562, 406, 410, 598, 172, 578, 578, 426, 410, 578, 102
```

Wall-backed objects in the same pass:

```text
object 35 -> texture 152
object 30 -> texture 108
```

## Hardware-validated 3-slot sprite LRU

Current branch: `agent/esp32-menu-sprite-lru-cache`.

The cache was deliberately integrated **without modifying the validated projected
sprite renderer or GFXRM implementation**. Two GNU ld wrappers intercept the
resource ownership boundary:

```text
-Wl,--wrap=EspNativeGraphics_loadSpriteFrame
-Wl,--wrap=EspNativeGraphics_releaseSpriteFrame
```

Behaviour:

```text
cache inactive:
renderer -> wrapper -> __real_GFXRM call

cache active:
renderer -> wrapper -> NativeSpriteLruCache
                         |-- HIT  -> borrowed frame
                         `-- MISS -> __real_GFXRM load
```

On renderer release, a borrowed view is cleared but the cache slot remains
resident. Only eviction or `EspNativeSpriteCache_end()` calls the real GFXRM
release.

This makes cache use explicit and keeps all previous probes transparent when the
cache is inactive.

### Why 3 slots

Hardware-derived LRU comparison:

```text
slots  hits  misses  evictions  peak logical bytes
1       1      10       9        2,112
2       1      10       8        4,089
3       2       9        6        6,038
4       2       9        5        6,834
5       3       8        3        7,437
```

Three slots capture two repeat hits with a 6,038 B peak. Four slots give no extra
hit; five slots save only one more load while consuming another 1,399 B at the
reference peak.

### Hardware result

```text
[MENUSPRITE] Baseline heap8=29852 largest8=21492
[SPRITECACHE] BEGIN slots=3 variablePayload=yes cold=yes
...
[MENUSPRITE] Sprite LRU slots=3 requests=11 hits=2 misses=9 evictions=6 resident=3 peak=3 residentBytes=3709 peakBytes=6038 maxFrame=2112B
[MENUSPRITE] Sprite LRU resident heap8=26092 largest8=17396 currentCost=3760B logicalCurrent=3709B logicalPeak=6038B
[MENUSPRITE] GFXRM spriteLoads=9 wallLoads=2 packOpenCycles=11 logicalBytes=14830 expected=14830 peakFrame=2112
[MENUSPRITE] framebufferFNV=ffe0995e expected=ffe0995e wallsFNV=a6d87c4a renderMs=1038 shapeData=0x0 mediaTexels=0x0
[SPRITECACHE] END requests=11 hits=2 misses=9 evictions=6 resident=3 peak=3 residentBytes=3709 peakBytes=6038 maxFrame=2112
[MENUSPRITE] End heap8=29852 largest8=21492 deltaFromStart=0 cacheReleased=yes
```

Validated cache contract:

```text
slots                   = 3
requests                = 11
hits                    = 2
misses                  = 9
evictions               = 6
peak logical payload    = 6,038 B
final resident payload  = 3,709 B
final allocator cost    = 3,760 B
max single frame        = 2,112 B
physical sprite loads   = 9
sprite bytes read       = 10,734 B
wall-backed loads       = 2
pack opens              = 11
GFXRM logical total     = 14,830 B
framebuffer FNV         = ffe0995e
```

There is no hardware allocator snapshot exactly at the 6,038-byte logical peak;
do not label 6,038 B as measured allocator cost.

### I/O comparison

```text
                         uncached PR #24    sprite LRU3
sprite loads                    11               9
sprite bytes                14,688          10,734
wall-backed loads                2               2
pack opens                      13              11
GFXRM logical total         18,784          14,830
framebuffer FNV            ffe0995e        ffe0995e
measured renderMs              1064            1038
```

Sprite payload reads fall by 3,954 B, about 27%. The measured time improves only
~26 ms / 2.4% on this verbose diagnostic run, so timing is not a contract.

### Memory recovery

```text
before cache: heap8=29852 largest8=21492
final resident cache: heap8=26092 largest8=17396
cache teardown: heap8=29852 largest8=21492
```

Exact allocator restoration is mandatory. The absolute baseline can change when
static cache/wrapper code changes; compare before/after within the same firmware.

## Current native graphics architecture

```text
                     SD / DoomRPG-ESP32.pak
                              |
                              v
                            GFXRM
                          /       \
                         /         \
               sprite frames      wall frames
                    |                  |
        NativeSpriteLruCache(3)   NativeWallLruCache(3)
                    |                  |
              borrowed frame      borrowed frame
                    |                  |
                    v                  v
          native projected       native projected
             sprite spans           wall spans
                    \                  /
                     \                /
                      shared 160x120 RGB565
                              |
                              v
                         exact 2x CYD
```

Useful deterministic signatures:

```text
sprite 172 texel FNV         = 0c0a7acd
wall 112 texel FNV           = 92d40704
synthetic projected wall FNV = ad191f54
real wall request FNV        = 4db9da28
real walls framebuffer FNV   = a6d87c4a
viewSprites list FNV         = 962cd657
sprite request FNV           = 4457ac94
walls+sprites framebuffer    = ffe0995e
wall LRU                     = 14 hits / 11 misses / 8 evictions
sprite LRU                   = 2 hits / 9 misses / 6 evictions
```

## Native serial markers

Important markers include:

```text
[ASSETPAK]
[BITSHAPE]
[SPRITETEX]
[PALETTE]
[GFXRM]
[SPRITERENDER]
[WALLRENDER]
[PROJWALL]
[WALLCACHE]
[MENUWALL]
[MENUSPRITE]
[SPRITECACHE]
```

For hardware validation, preserve the complete newest marker block and the next
`[ALIVE]` heartbeat. A photo is useful for visual milestones.

## Linker-wrap caution

Current ESP32 wrappers include renderer lifetime, loading-bar interception and
sprite-cache resource ownership. They are declared in `platformio.ini`.

Do not remove or rename a wrapped symbol without checking the corresponding
`__wrap_*` / `__real_*` implementation and all earlier hardware probes.

The sprite cache specifically depends on:

```text
-Wl,--wrap=EspNativeGraphics_loadSpriteFrame
-Wl,--wrap=EspNativeGraphics_releaseSpriteFrame
```

The wrappers must stay transparent while `EspNativeSpriteCache_isActive()==0`.

## Porting workflow

1. Create a branch from exact latest validated `main`.
2. Implement one small objective.
3. Test on real CYD.
4. Fix failures on that same branch.
5. After PASS, update `PORTING_STATUS.md`, this README and any other relevant
   documentation on the same branch.
6. Only then merge.
7. Start the next branch only after merge acknowledgement.

## Design rules

- preserve game behaviour/data compatibility, not unnecessary desktop memory
  layout
- keep allocations bounded and measured
- treat SD as backing store and RAM as working/cache space
- isolate storage from cache policy and rasterization
- keep packed indexed graphics packed
- never recreate monolithic `shapeData` or map-wide `mediaTexels`
- never rewrite global mappings just to make a bounded local frame look legacy
- require exact allocator restoration after teardown
- preserve deterministic framebuffer hashes when changing resource lifetime
- measure access patterns before choosing cache sizes
- choose wall and sprite cache sizes independently
- keep documentation sufficient to resume without chat history
- keep audio out until the gameplay/render path is stable

## Next direction

The wall and sprite resource lifetimes for the current menu scene are now
hardware-validated. The next increment should become visual again rather than
adding another cache.

Recommended next step: trace the original `DoomCanvas` / `MenuSystem` menu
composition path and integrate **one small real main-menu visual element** over
the validated scene. Keep the pre-overlay regression boundaries:

```text
walls FNV             = a6d87c4a
viewSprites list FNV  = 962cd657
sprite request FNV    = 4457ac94
walls+sprites FNV     = ffe0995e
shapeData              = NULL
mediaTexels            = NULL
```

Do not combine menu UI composition with textured floor/ceiling work in the same
increment. If analysis shows the original menu requires plane texturing first,
make that its own separately hardware-validated branch.
