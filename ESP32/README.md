# Doom RPG ESP32 port

This directory contains the ESP32-specific port and bring-up work for the classic
ESP32-2432S028R (Cheap Yellow Display, no PSRAM).

The ESP32 target is treated as its own constrained architecture, not as a PC
backend with smaller buffers. DoomRPG-RE is used as a behavioural/data-format
reference while resource management and rendering are progressively rebuilt for
the actual target.

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

For the authoritative hardware figures, branch state and recovery boundary, see
[`PORTING_STATUS.md`](PORTING_STATUS.md).

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
validated sprite frame            = 2,112 B logical
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

GFXRM itself remains the storage/load boundary. Cache policy is deliberately a
separate layer rather than hidden inside every load.

## Native sprite rendering contract

Validated worst-case menu sprite:

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
framebuffer FNV1a   = 001910a9
```

`shapeData` and `mediaTexels` remain `NULL` throughout the native sprite path.

## Native wall rendering contract

Validated menu wall texture:

```text
texture index       = 112
size                = 64x64
packed texels       = 2048 B
allocator cost      = 2064 B
texel offset        = 65536
wtexels byte offset = 32768
palette offset      = 480
pixels drawn        = 4096
texel FNV1a         = 92d40704
framebuffer FNV1a   = e39af2c4
```

Wall texels use the original column-major layout:

```text
logical texel index = x * 64 + y
```

## Projected wall: direct native span source

The projected wall path is native at the resource boundary:

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

Strong invariant:

```text
shapeData   = NULL
mediaTexels = NULL
```

Global `mediaTexelOffsets[]` are not rewritten to fit local frames.

The deterministic synthetic projected-wall regression remains:

```text
projected wall FNV = ad191f54
```

## First real `menu.bsp` scene

The project renders an actual walls-only frame from the real menu map. This is no
longer synthetic geometry.

### Camera source

The original menu loader derives X/Y and direction from the BSP header:

```text
spawnIndex = 460
spawn tile = 12,14
world      = 800,928
direction  = 0
camera Z   = 36
```

The first four values come from `menu.bsp`; Z=36 is the documented normal engine
eye-height convention used for the deterministic probe.

### Real scene pipeline

```text
menu.bsp structures
      |
      v
real menu camera
      |
      v
Render_render camera setup
      |
      v
Render_walkNode
(BSP traversal/culling/leaf ordering/occlusion)
      |
      v
real visible map lines
      |
      v
real texture mappings
      |
      v
wall cache / GFXRM
      |
      v
native projected wall spans
      |
      v
shared framebuffer
```

For the current real scene:

```text
projected sprites       disabled
textured floor/ceiling  disabled
menu UI overlay         not yet composed
```

So the displayed image is intentionally a grayscale walls-only scene. The
absence of the earlier Cacodemon diagnostic sprite is expected; projected
real-scene sprites are a separate integration step.

### Stable real-scene regression

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

The animation phase is frozen at `animFrameTime=0` so this reference frame and
resource-request order are deterministic.

## Measured wall request pattern

Exact 25-request sequence:

```text
116, 32, 40, 112, 108, 108, 116, 116, 116, 112,
116, 112, 108, 108, 108, 116, 44, 0, 40, 152,
152, 116, 116, 116, 152
```

Unique textures:

```text
0, 32, 40, 44, 108, 112, 116, 152
```

Totals:

```text
requests = 25
unique   = 8
repeats  = 17
```

Keep this sequence in documentation. It is recovery/test data for future cache
changes and makes it possible to evaluate cache policies without rediscovering
the first-frame access pattern on hardware.

## Hardware-validated 3-slot wall LRU

The three-slot wall cache is now a real validated engine component, not only a
simulation.

### Separation of responsibilities

```text
DoomRPG-ESP32.pak
        |
        v
      GFXRM
        |
        v
NativeWallLruCache
        |
        v
borrowed wall frame
        |
        v
ProjectedWallRenderer
```

- GFXRM owns storage-format knowledge and physical loading.
- `NativeWallLruCache` owns cached wall-frame lifetime and LRU replacement.
- the projected wall renderer borrows cache frames and never frees cache slots.

The previous owning projected-wall API still exists for diagnostics. Cache use is
explicit/opt-in.

### Borrowed ownership

When the cache supplies a frame, the renderer uses a borrowed view:

```c
EspNativeProjectedWall_beginBorrowed(render, cachedFrame);
...
EspNativeProjectedWall_end();
```

`end()` clears the borrowed rendering state but does not free the cache slot.
The cache remains the sole owner and releases all slots during teardown.

### Hardware result

The exact PR #22 request sequence produced:

```text
[MENUWALL] LRU slots=3 requests=25 hits=14 misses=11 evictions=8 resident=3 peak=3 residentBytes=6144 peakBytes=6144
[MENUWALL] LRU resident heap8=24200 largest8=16372 aggregateCost=6192B logicalPayload=6144B
[MENUWALL] GFXRM wallLoads=11 packOpenCycles=11 logicalBytes=22528 expected=22528 peakFrame=2048
[MENUWALL] Native spans begin=25 end=25 spanCalls=224 pixels=5589 rangeErrors=0 legacyPtrViolations=0 mappingViolations=0
[MENUWALL] framebufferFNV=a6d87c4a expected=a6d87c4a renderMs=1357 floor=4208 ceiling=8c51 mediaTexels=0x0
[WALLCACHE] END requests=25 hits=14 misses=11 evictions=8 resident=3 peak=3 residentBytes=6144 peakBytes=6144
[MENUWALL] End heap8=30392 largest8=22516 deltaFromStart=0 cacheReleased=yes
```

Validated cache contract:

```text
slots                    = 3
logical requests         = 25
hits                     = 14
misses                   = 11
evictions                = 8
peak resident slots      = 3
logical cache payload    = 6,144 B
allocator cache cost     = 6,192 B
physical wall loads      = 11
pack open cycles         = 11
wall payload bytes read  = 22,528 B
single-frame peak        = 2,048 B
```

The output is bit-identical to the uncached scene:

```text
uncached framebuffer = a6d87c4a
cached framebuffer   = a6d87c4a
```

### Memory result

Before cache activation:

```text
heap8=30392
largest8=22516
```

With three slots resident:

```text
heap8=24200
largest8=16372
```

After cache teardown:

```text
heap8=30392
largest8=22516
deltaFromStart=0
```

The cache therefore costs 6,192 allocator bytes for 6,144 logical bytes and
releases exactly.

The lower baseline relative to the previous branch is static cache/borrowed-path
bookkeeping, not a leak; the hardware contract is exact restoration to the
current branch baseline.

### I/O/timing comparison

```text
                          uncached      3-slot LRU
logical wall requests       25              25
physical GFXRM loads         25              11
pack open cycles             25              11
wall bytes read          51,200          22,528
framebuffer FNV         a6d87c4a        a6d87c4a
measured renderMs          1676            1357
```

The single measured cached run is about 319 ms / 19% faster. Treat timing as an
observation, not a deterministic assertion: SD and serial logging timing can
vary. Hashes, request accounting and memory restoration are the durable tests.

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
       |       borrowed wall frame
       |            |
       |            v
       |       projected wall spans
       |            |
       v            v
native sprite   real BSP walls
       \            /
        \          /
       shared 160x120 framebuffer
                  |
                  v
            exact 2x output
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
```

Useful deterministic checks:

```text
sprite 172 texel FNV         = 0c0a7acd
sprite diagnostic FNV        = 001910a9
wall 112 texel FNV           = 92d40704
wall diagnostic FNV          = e39af2c4
synthetic projected wall FNV = ad191f54
real menu walls frame FNV    = a6d87c4a
real menu request FNV        = 4db9da28
wall LRU reference           = 14 hits / 11 misses / 8 evictions
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
- preserve deterministic framebuffer hashes when replacing memory/resource policy
- measure real access patterns before selecting cache sizes
- do not assume a wall-cache policy is appropriate for sprites; measure sprites
  independently
- keep documentation sufficient for an engineer to resume without chat history
- keep audio out of the memory-critical bring-up until gameplay/render is stable

## Next renderer direction

Next, add **real projected sprites to the same deterministic `menu.bsp` frame**.
Keep the validated three-slot wall LRU enabled, but do not add a sprite cache in
the same increment.

First measure the real sprite path:

- reuse the `viewSprites` order already produced by the original BSP walk
- acquire bounded sprite frames through GFXRM
- render the visible real-scene sprites without `shapeData` or `mediaTexels`
- keep textured floor/ceiling disabled
- record the sprite request order, unique/repeated resources, physical bytes and
  peak frame size
- establish a deterministic framebuffer hash for walls + sprites
- release every sprite frame exactly

Only after those hardware measurements should a sprite-cache size or policy be
chosen.
