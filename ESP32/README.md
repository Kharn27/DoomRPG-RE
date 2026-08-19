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
- exact 2x nearest-neighbour output to 320x240
- XPT2046 touch on a separate software-SPI path
- microSD for game/resource data
- audio disabled during bring-up

For the latest hardware-validated memory figures and current safe stop boundary,
see [`PORTING_STATUS.md`](PORTING_STATUS.md).

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

From the repository root:

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

Payloads are stored uncompressed and directly seekable. Each 20-byte index
record contains:

```text
normalized-name FNV-1a hash
payload offset
payload size
CRC32
flags
```

The index remains on SD; the ESP32 binary-searches it instead of keeping 241
records resident in RAM.

Runtime name lookup normalizes ASCII case, leading `/`, and `\\` to `/`, so:

```text
menu.bsp
/MENU.BSP
```

resolve the same resource.

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
```

Immutable graphics therefore stay on SD and native consumers acquire only
bounded working sets.

## Native palette convention

Native consumers use canonical RGB565 in the shared framebuffer.
`Render_loadPalettes()` leaves the reconstructed legacy palette in the opposite
red/blue convention, so the ESP32 path normalizes the palette once before native
rendering.

Validated diagnostic:

```text
[PALETTE] Normalized 3280 entries legacy R/B order -> framebuffer RGB565
[PALETTE] sprite172 offset=1616 first4 before=0000,ffff,c000,07ff after=0000,ffff,0018,ffe0
```

Do not compensate palette ordering by modifying `stexels.bin`, `wtexels.bin` or
nibble order. Those source bytes are already hardware-proven.

## Native graphics resource manager (GFXRM)

Sprite and wall rasterizers share one storage/file-format boundary:

```text
                  native rasterizers
                 /                  \
              sprite               wall
                 \                  /
                  \                /
             NativeGraphicsResourceManager
                         |
                  DoomRPG-ESP32.pak
                  /        |        \
          bitshapes     stexels    wtexels
```

The manager owns pack open/close, entry lookup, source offset/range validation,
bounded frame allocation/read and release. Rasterizers no longer know the
on-disk layout.

Public API:

```c
EspNativeGraphics_loadSpriteFrame(render, spriteIndex, &frame);
EspNativeGraphics_releaseSpriteFrame(&frame);

EspNativeGraphics_loadWallFrame(render, textureIndex, &frame);
EspNativeGraphics_releaseWallFrame(&frame);
```

The manager is deliberately **not a cache yet**. Each successful load opens the
pack, performs the bounded read and closes the pack before returning the frame.

Validated diagnostic session:

```text
[GFXRM] SPRITE id=172 storage=2112B mask=512B texels=1600B hash=0c0a7acd pack=closed
[GFXRM] WALL id=112 storage=2048B hash=92d40704 pack=closed
[GFXRM] Session stats spriteLoads=1 wallLoads=1 packOpenCycles=2 logicalBytes=4160 peakFrame=2112
```

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

The same hashes remain exact after routing the load through GFXRM.

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

## Projected wall compatibility bridge

The first real projected wall path has now been hardware-validated without
recreating the legacy wall pool.

The actual Doom RPG projection/raster path remains unchanged:

```text
Render_drawLines
  -> Render_transform2DVerts
  -> Render_clipLine
  -> Render_projectVertex
  -> Render_drawWallSpans
  -> Render_getSpanMode
  -> Render_SpanMode0
```

A deterministic test wall is placed in front of a fixed camera:

```text
v1=(128,-32,z0)
v2=(128, 32,z64)
camera=(0,0,z32)
spanMode=0
```

GFXRM loads texture 112 as one 2,048-byte frame. To let the untouched legacy
`Render_SpanMode0()` consume that bounded frame, the compatibility bridge does
this **only for the duration of one draw**:

```text
mediaTexelOffsets[112 * 2] : 65536 -> 0
mediaTexels                : NULL  -> pointer to 2,048-byte GFXRM frame
```

Then it restores both values immediately:

```text
mediaTexelOffsets[112 * 2] : 0 -> 65536
mediaTexels                : bounded frame -> NULL
```

This temporary alias must not be confused with the forbidden legacy
`mediaTexels` architecture. The old map-wide 172,032-byte pool is never created.

### Hardware result

```text
[PROJWALL] PROJECTED columns=60..100 count=40 scale=81920/81920 z=0/5242880 lineRasterCount=1 changedPixels=1600
[PROJWALL] Bridge stats begin=1 end=1 bound=2048B texture=112 palette=480 originalOffset=65536 texelHash=92d40704
[PROJWALL] GFXRM stats spriteLoads=0 wallLoads=1 packOpenCycles=1 logicalBytes=2048 peakFrame=2048
[PROJWALL] framebufferFNV=ad191f54 mediaTexelsRestored=0x0 mappingOffsetRestored=65536
[PROJWALL] READY unchanged projection + Render_drawWallSpans + Render_SpanMode0 consumed one bounded GFXRM frame
[PROJWALL] READY legacy mediaTexels alias existed only during draw and is restored to NULL
```

Deterministic projected-wall regression markers:

```text
texture source FNV     = 92d40704
projected columns      = 40
changed pixels         = 1600
projected framebuffer  = ad191f54
```

This is the first graphics proof that is no longer just an asset viewer: the
wall geometry, clipping, projection, per-column scale and vertical span sampling
are performed by the real renderer.

### Memory result and allocator rule

Current branch result:

```text
before    heap8=30584 largest8=22516
resident  heap8=28520 largest8=20468
released  heap8=30584 largest8=22516
```

The frame allocation costs 2,064 bytes. The temporary change in the largest free
block is allocator-placement dependent. A probe must **not** require
`largest8` to remain unchanged while a frame is resident.

The contract is:

- bounded allocation size is known
- no leak after release
- free heap returns exactly to the starting value
- largest free block returns exactly to the starting value

The first hardware run exposed this distinction and the probe was corrected on
the same branch.

## Current native graphics architecture

```text
SD / native pack
       |
       v
NativeGraphicsResourceManager
       |
       +--> EspNativeSpriteFrame (~2.1 KB max validated)
       |
       +--> EspNativeWallFrame   (2 KB)
       |
       +--> projected-wall compatibility bridge
       |        |
       |        v
       |   original wall projection + SpanMode0
       |
       v
shared 160x120 RGB565 framebuffer
       |
       v
exact 2x physical presentation
```

The long-term rule remains:

```text
shapeData = NULL
no map-wide mediaTexels allocation
```

For ordinary native sprite/wall consumers, `mediaTexels` remains `NULL`
throughout. For the current projected-wall compatibility proof only, it briefly
aliases one bounded 2 KB wall frame and is restored to `NULL` immediately.

The next renderer increment should remove even that temporary alias and feed the
projected wall span sampler from the GFXRM frame explicitly.

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
```

Useful deterministic checks:

```text
sprite 172 texel FNV       = 0c0a7acd
sprite diagnostic FNV      = 001910a9
wall 112 texel FNV         = 92d40704
wall diagnostic FNV        = e39af2c4
projected wall FNV         = ad191f54
```

When reporting a hardware test, keep the complete block around the newest
marker plus the following `[ALIVE]` heartbeat.

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

- preserve game behaviour and data compatibility, not unnecessary PC/mobile
  memory architecture
- prefer bounded allocations with known maximum sizes
- treat SD as secondary storage and RAM as a small working/cache area
- isolate storage/file-format concerns from rasterizers
- avoid whole-file decompression for large immutable resources
- avoid resident indexes when a compact on-disk index is sufficient
- avoid duplicate framebuffer/resource representations
- keep indexed/paletted graphics packed whenever possible
- use one explicit canonical RGB565 convention at the native rendering boundary
- never recreate monolithic `shapeData` or map-wide `mediaTexels`
- compatibility aliases must be bounded, explicit and temporary
- require exact allocator-state restoration after release rather than making
  assumptions about which free block services an allocation
- do not preserve reverse-engineered implementation details merely for fidelity
  when a simpler ESP32-native design provides the same behaviour
- refactor subsystems incrementally rather than performing a speculative rewrite
- do not choose cache slot counts blindly; measure actual access patterns first
- keep audio out of the memory-critical bring-up until gameplay/render is stable
