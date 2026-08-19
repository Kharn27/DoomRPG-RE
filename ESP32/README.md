# Doom RPG ESP32 port

This directory contains the ESP32-specific port and bring-up work for the classic
ESP32-2432S028R (Cheap Yellow Display, no PSRAM).

The ESP32 target is treated as its own constrained architecture, not as a PC
backend with smaller buffers. DoomRPG-RE is used as a behavioural/data-format
reference while resource management and rendering are progressively rebuilt for
the actual target.

For the authoritative recovery point, exact hardware figures and current branch
state, see [`PORTING_STATUS.md`](PORTING_STATUS.md).

## Current target

- ESP32-2432S028R / classic CYD, no PSRAM
- ESP32-D0WD-V3, 240 MHz, 4 MB flash
- ILI9341 320x240 landscape
- internal render target: 160x120 RGB565
- gameplay viewport: 160x80 at framebuffer y=20
- exact 2x nearest-neighbour output to 320x240
- XPT2046 touch on a separate software-SPI path
- microSD for game/resource data
- audio disabled during bring-up

## Build and flash

From the repository root:

```bash
cd ESP32
pio run -t upload
pio device monitor
```

A clean build is normally unnecessary between ordinary increments. Use it only
when generated-source/build-system changes make it useful:

```bash
cd ESP32
pio run -t clean
pio run -t upload
```

## Game data on the SD card

During migration the SD card contains:

```text
/DoomRPG.zip
/DoomRPG-ESP32.pak
```

`DoomRPG.zip` remains only for legacy engine paths not migrated yet.
`DoomRPG-ESP32.pak` is the native direct-access resource backend.

The long-term target is to remove the ZIP dependency completely.

## Building the ESP32 native asset pack

```bash
python3 ESP32/tools/build_asset_pack.py \
    /path/to/DoomRPG.zip \
    /path/to/DoomRPG-ESP32.pak
```

Example writing directly to a mounted SD card:

```bash
python3 ESP32/tools/build_asset_pack.py \
    /media/$USER/DOOMRPG/DoomRPG.zip \
    /media/$USER/DOOMRPG/DoomRPG-ESP32.pak
```

To print every generated asset record:

```bash
python3 ESP32/tools/build_asset_pack.py --list \
    /path/to/DoomRPG.zip \
    /path/to/DoomRPG-ESP32.pak
```

The builder self-check must finish with:

```text
[PACK] self-check: OK
```

### Pack format v2

The validated bring-up archive contains 241 entries. For 241 entries:

```text
header                   24 B
241 index records x 20  4820 B
------------------------------
index/data boundary     4844 B
```

Payloads are uncompressed and directly seekable. The on-disk index is binary
searched; it is not kept resident as a 241-entry RAM table.

## Why the native pack exists

The original graphics architecture cannot fit a classic ESP32 without PSRAM.
For the real menu map:

```text
mapTextureTexelsCount = 84
mapSpriteTexelsCount  = 284
planeTexturesCnt      = 11
```

Problematic legacy resident sizes:

```text
wall-only legacy mediaTexels     = 172,032 B
selected sprite texels total     = 143,990 B
legacy expanded shapeData        = 55,676 B
```

Measured native working sets are tiny by comparison:

```text
bitshape mask-column scratch     <= 32 B
largest selected sprite payload   = 1,600 B
largest validated sprite frame    = 2,112 B logical
one packed wall texture           = 2,048 B
validated 3-slot wall cache       = 6,144 B logical
```

Immutable graphics therefore remain on SD and are acquired/reused as bounded
working sets.

## Native palette convention

Native consumers use canonical RGB565 in the shared framebuffer. The ESP32 path
normalizes the reconstructed legacy palette once before native rendering.

Validated diagnostic:

```text
[PALETTE] Normalized 3280 entries legacy R/B order -> framebuffer RGB565
[PALETTE] sprite172 offset=1616 first4 before=0000,ffff,c000,07ff after=0000,ffff,0018,ffe0
```

Do not compensate palette ordering by modifying `stexels.bin`, `wtexels.bin` or
nibble order. Those source bytes are hardware-proven.

## Native graphics resource manager (GFXRM)

Sprite and wall paths share one storage/file-format boundary:

```text
                  render consumers
                 /                \
              sprite             wall
                 \                /
                  \              /
             NativeGraphicsResourceManager
                         |
                  DoomRPG-ESP32.pak
                  /        |        \
          bitshapes     stexels    wtexels
```

Public frame API:

```c
EspNativeGraphics_loadSpriteFrame(render, spriteIndex, &frame);
EspNativeGraphics_releaseSpriteFrame(&frame);

EspNativeGraphics_loadWallFrame(render, textureIndex, &frame);
EspNativeGraphics_releaseWallFrame(&frame);
```

GFXRM owns storage/file-format knowledge. Cache policy remains a separate layer.

## Native sprite frame contract

A native sprite frame contains only the data required to render one sprite:

```text
source bitshape header/bounds
source bitshape mask
packed active stexels (4 bpp)
palette offset
```

The hardware-validated worst-case menu frame remains sprite 172:

```text
sprite index        = 172
size                = 64x64
active pixels       = 3199
bitshape mask       = 512 B
packed texels       = 1600 B
logical frame data  = 2112 B
allocator cost      = 2128 B
palette offset      = 1616
texel FNV1a         = 0c0a7acd
```

No native sprite path requires a resident expanded `shapeData` array.

## Native wall contract

Validated menu wall texture:

```text
texture index       = 112
size                = 64x64
packed texels       = 2048 B
allocator cost      = 2064 B
texel offset        = 65536
wtexels byte offset = 32768
palette offset      = 480
texel FNV1a         = 92d40704
```

Wall texels use the original column-major layout:

```text
logical texel index = x * 64 + y
```

## Strong legacy-pool invariant

The current native real-scene renderer keeps both original monolithic graphics
pools absent:

```text
shapeData   = NULL
mediaTexels = NULL
```

Do not reintroduce either pool as a shortcut. Bounded GFXRM frames and small
measured caches are the intended target architecture.

## Projected walls

The wall path keeps the original transform/clip/projection semantics but samples
a bounded native wall frame directly:

```text
wall frame
    |
    v
original transform / clip / projection semantics
    |
    v
ESP32-native wall-span geometry
    |
    v
direct packed 4-bit frame sampling
    |
    v
shared RGB565 framebuffer
```

Global `mediaTexelOffsets[]` are not rewritten to fit local frames.

Synthetic regression:

```text
projected wall framebuffer FNV = ad191f54
```

## Real `menu.bsp` camera

The deterministic menu scene uses the real BSP header camera position/direction:

```text
spawnIndex = 460
spawn tile = 12,14
world      = 800,928
direction  = 0
camera Z   = 36
```

X/Y/direction come directly from `menu.bsp`. Z=36 is the documented normal
engine eye-height convention used by the deterministic bring-up frame.

`animFrameTime` is frozen to zero for deterministic resource selection and
framebuffer hashes.

## Real menu wall frame

The original engine still supplies:

- camera transform
- BSP traversal
- bounding-box culling
- leaf ordering
- wall occlusion
- real `Line_t` geometry
- texture-ID mapping semantics

The ESP32-native path supplies bounded resources and native texel sampling.

Stable wall-only regression:

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

### Exact wall request sequence

```text
116, 32, 40, 112, 108, 108, 116, 116, 116, 112,
116, 112, 108, 108, 108, 116, 44, 0, 40, 152,
152, 116, 116, 116, 152
```

This sequence is recovery/test data and must remain documented.

## Hardware-validated 3-slot wall LRU

The wall cache is intentionally separate from GFXRM and the projected renderer:

```text
DoomRPG-ESP32.pak
        |
        v
      GFXRM
        |
        v
NativeWallLruCache (3 slots)
        |
        v
borrowed wall frame
        |
        v
ProjectedWallRenderer
```

Hardware result on the exact wall request sequence:

```text
requests                 = 25
hits                     = 14
misses                   = 11
evictions                 = 8
peak resident slots      = 3
logical cache payload    = 6,144 B
allocator cache cost     = 6,192 B
physical wall loads      = 11
pack open cycles         = 11
wall payload bytes read  = 22,528 B
framebuffer FNV          = a6d87c4a
```

The cached output is bit-identical to the uncached real wall scene.

The cache is torn down completely before the current sprite-measurement pass so
sprite memory/I/O measurements are isolated.

## Real BSP-sorted menu sprites

The project now renders the actual sprite objects visible in the same real menu
camera.

The original `Render_walkNode()` is still responsible for visibility and the
`sortZ`-ordered `viewSprites` list. The native renderer consumes that ordered
list instead of inventing a separate scene order.

```text
real menu BSP
     |
     v
Render_walkNode
     |
     v
viewSprites (original sortZ order)
     |
     v
mediaSpriteIds / anim resolution
     |
     +----------------------------+
     |                            |
 bitshape-backed             wall-backed object
     |                            |
     v                            v
GFXRM sprite frame           bounded GFXRM wall frame
(mask + stexels)                  |
     |                            v
     v                    native projected wall spans
native projected sprite spans     |
     |                            |
     +-------------+--------------+
                   |
                   v
        framebuffer already containing
        hardware-validated cached walls
```

### How `shapeData` was removed

The legacy sprite renderer uses expanded `shapeData` to precompute opaque runs
per source column. The native renderer reconstructs those runs directly from the
bounded source bitshape mask and uses the active-pixel prefix to address the
packed stexel stream.

This keeps the behavioural semantics without recreating the 55,676-byte expanded
map-wide structure.

### Supported render modes

The native projected-sprite span sampler implements legacy span modes 0..9. The
current deterministic menu frame exercises normal mode 0 and additive mode 7.

The hardware validation reported:

```text
unsupportedFlags = 0
unsupportedModes = 0
```

## Hardware result: walls + real sprites

Authoritative current regression:

```text
[MENUSPRITE] Begin wallsFNV=a6d87c4a expected=a6d87c4a shapeData=0x0 mediaTexels=0x0
[MENUSPRITE] Baseline heap8=30176 largest8=22516 numMapSprites=44 runtimeSlots=68
[MENUSPRITE] View list objects=17 outOfRange=0 listFNV=962cd657 ordering=original-BSP-sortZ
[MENUSPRITE] Objects total=17 hidden=0 lightsSkipped=5 entityUnsupported=0 resolvedDraws=14
[MENUSPRITE] Requests spriteFrames=11 unique=8 repeats=3 requestFNV=4457ac94 wallBacked=2 maxFrame=2112B
[MENUSPRITE] Cull near=1 backface=0 clipped=1 spans=389 pixels=4590
[MENUSPRITE] Invariants rangeErrors=0 legacyPtrViolations=0 shapeDataViolations=0 mappingViolations=0 unsupportedFlags=0 unsupportedModes=0
[MENUSPRITE] GFXRM spriteLoads=11 wallLoads=2 packOpenCycles=13 logicalBytes=18784 peakFrame=2112
[MENUSPRITE] Wall-backed projected begin=2 end=2 spans=20 pixels=340 errors=0/0/0
[MENUSPRITE] framebufferFNV=ffe0995e wallsFNV=a6d87c4a changed=yes renderMs=1064 shapeData=0x0 mediaTexels=0x0
[MENUSPRITE] End heap8=30176 largest8=22516 deltaFromStart=0
[MENUSPRITE] READY real menu sprites rendered from bounded uncached GFXRM frames
```

Stable deterministic signatures:

```text
real menu walls frame FNV     = a6d87c4a
real menu wall request FNV    = 4db9da28
viewSprites list FNV          = 962cd657
real sprite request FNV       = 4457ac94
real menu walls+sprites FNV   = ffe0995e
```

Treat `renderMs=1064` as an observation only. Timing depends on SD and verbose
serial logging.

## Exact real sprite request sequence

The 11 bitshape-backed frame requests are:

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

Unique frames:

```text
102, 172, 406, 410, 426, 562, 578, 598
```

Repeat pattern:

```text
578 requested 3 times
410 requested 2 times
```

Two visible objects use wall textures instead of bitshape-backed sprites:

```text
object 35 -> texture 152
object 30 -> texture 108
```

The sprite request FNV `4457ac94` includes object index, media ID and render mode,
so it protects the ordered semantic request stream rather than only the media-ID
list.

## Real sprite frame sizes

Observed logical frame storage:

```text
mediaId  bytes
562       962
406       603
410       796
598      1949
172      2112
578      1977
426       603
102       936
```

Current uncached sprite pass:

```text
11 sprite loads     = 14,688 B logical
2 wall-backed loads =  4,096 B logical
--------------------------------------
GFXRM total         = 18,784 B logical
pack open cycles    = 13
peak one frame      = 2,112 B
```

Every frame is released before the next load; hardware returns exactly to:

```text
heap8=30176
largest8=22516
```

## Derived sprite LRU guidance

No sprite cache exists yet. The following table is **analysis derived from the
hardware-validated request stream**, not an implemented cache:

```text
slots  hits  misses  evictions  peak logical bytes on this sequence
1       1      10       9        2,112
2       1      10       8        4,089
3       2       9        6        6,038
4       2       9        5        6,834
5       3       8        3        7,437
6       3       8        2        8,399
7       3       8        1        9,002
8       3       8        0        9,938
```

Interpretation:

- 3 slots catch 2 of the 3 possible repeat hits at about 6 KB peak logical
  payload;
- 4 slots add no hit compared with 3;
- 5 slots are the smallest cache that catches all 3 repeat hits, at 7,437 B peak
  logical payload on the reference sequence;
- >5 slots add no hit on this deterministic frame.

Do not select a sprite cache size from the number of unique frames alone. The
next cache increment should choose an explicit RAM/I/O tradeoff and verify exact
allocator cost on hardware.

## Current native graphics architecture

```text
SD / DoomRPG-ESP32.pak
           |
           v
          GFXRM
        /       \
       /         \
 sprite frame   wall frame
       |            |
       |       NativeWallLruCache (3)
       |            |
       v            v
native projected  native projected
sprite spans      wall spans
       \            /
        \          /
       shared 160x120 framebuffer
                  |
                  v
            exact 2x output
```

Current real-scene frame signature:

```text
walls only      = a6d87c4a
walls + sprites = ffe0995e
```

## Native serial diagnostics

Important successful markers include:

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
```

When reporting a hardware test, keep the complete newest marker block plus the
following `[ALIVE]` heartbeat. A photo is especially useful for real-scene
milestones.

## Porting workflow

The project is intentionally developed in small hardware-validated increments:

1. create one branch from the latest validated `main`
2. implement one small measurable objective
3. build/flash/test it on the real CYD
4. fix failures on the **same branch**
5. after hardware success, update every relevant `.md` file on that **same branch**
6. only when code + documentation agree is the branch considered merge-ready
7. merge the branch
8. only then start the next increment from the new exact `main` SHA

Documentation is part of the increment, not a separate follow-up increment.

## ESP32 design rules

- preserve game behaviour/data compatibility, not unnecessary PC/mobile memory
  architecture
- prefer bounded allocations with known maximum sizes
- treat SD as secondary storage and RAM as a small working/cache area
- isolate storage/file-format concerns from rasterizers
- isolate cache/reuse policy from storage loading
- avoid whole-file decompression for large immutable resources
- avoid duplicate framebuffer/resource representations
- keep indexed/paletted graphics packed whenever possible
- use one explicit canonical RGB565 convention at the native rendering boundary
- never recreate monolithic `shapeData` or map-wide `mediaTexels`
- do not rewrite global resource mappings merely to make bounded frames look like
  a legacy monolithic pool
- require exact allocator-state restoration after release rather than assuming
  which free block services an allocation
- preserve deterministic framebuffer hashes when replacing resource/memory policy
- measure real access patterns before selecting cache sizes
- measure wall and sprite caches independently; their payload sizes/access patterns
  are different
- keep documentation sufficient for an engineer to resume without chat history
- keep audio out of the memory-critical bring-up until gameplay/render is stable

## Next renderer direction

The next recommended increment is **sprite resource reuse only**, using this exact
walls+sprites frame as a regression. Do not add textured planes or menu UI in the
same increment.

Evidence-based candidates:

```text
3-slot sprite LRU -> expected 2 hits / 9 misses, ~6,038 B peak logical
5-slot sprite LRU -> expected 3 hits / 8 misses, ~7,437 B peak logical
```

Whichever cache size is selected, require:

```text
walls FNV             = a6d87c4a
viewSprites list FNV  = 962cd657
sprite request FNV    = 4457ac94
final framebuffer FNV = ffe0995e
shapeData              = NULL
mediaTexels            = NULL
```

Also measure exact allocator cost, hit/miss/eviction counts, physical bytes and
full cache teardown on the real CYD before moving on to textured floor/ceiling or
final menu UI composition.
