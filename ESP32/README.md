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

The obsolete v1 prototype contained only `bitshapes.bin`, `wtexels.bin` and
`stexels.bin` and was 305,743 bytes. It is intentionally incompatible with the
v2 reader.

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

The rule is therefore simple: immutable graphics stay on SD and native render
consumers acquire only bounded working sets.

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

Sprite and wall rasterizers now share one storage/file-format boundary:

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

The manager owns:

- opening/closing `DoomRPG-ESP32.pak`
- resolving required entries
- source offset/range validation
- bounded allocation of sprite/wall frames
- direct reads from `bitshapes.bin`, `stexels.bin`, and `wtexels.bin`
- release of native frames
- lightweight diagnostic statistics

The rasterizers own:

- palette sampling
- sprite mask traversal / active-pixel consumption
- wall texel sampling
- writing RGB565 pixels to the shared framebuffer

The public contract is defined in `include/native_graphics_resource_manager.h`:

```c
EspNativeGraphics_loadSpriteFrame(render, spriteIndex, &frame);
EspNativeGraphics_releaseSpriteFrame(&frame);

EspNativeGraphics_loadWallFrame(render, textureIndex, &frame);
EspNativeGraphics_releaseWallFrame(&frame);
```

The manager is deliberately **not a cache yet**. Each successful load currently
opens the pack, performs the bounded seek/read, closes the pack, and returns the
resident frame. The pack-open cost was previously measured at about 4,376 bytes,
so keeping it permanently open would consume meaningful RAM before real access
patterns justify that tradeoff.

### Hardware-validated GFXRM session

One sprite load followed by one wall load produces:

```text
[GFXRM] SPRITE id=172 storage=2112B mask=512B texels=1600B hash=0c0a7acd pack=closed
[GFXRM] WALL id=112 storage=2048B hash=92d40704 pack=closed
[GFXRM] Session stats spriteLoads=1 wallLoads=1 packOpenCycles=2 logicalBytes=4160 peakFrame=2112
[GFXRM] READY one shared backend served sprite + wall with zero persistent cache allocation
```

Expected session counters for this diagnostic are therefore:

```text
spriteLoads      = 1
wallLoads        = 1
packOpenCycles   = 2
logicalBytes     = 4160 B
peakFrame        = 2112 B
```

The counters themselves are static diagnostic state, not a graphics cache.

### Memory result

The shared manager introduces a stable 24-byte reduction in free 8-bit heap
versus the previous build:

```text
previous build baseline   heap8=30688 largest8=21492
GFXRM build baseline      heap8=30664 largest8=21492
```

The manager stores five `uint32_t` counters (20 bytes); the observed 24-byte
change is consistent with that persistent state plus alignment/accounting.
Every bounded load still returns exactly to the new baseline:

```text
sprite resident  heap8=28536 -> release 30664
wall resident    heap8=28600 -> release 30664
largest8 remains 21492
```

There is no per-load leak and no persistent frame/cache allocation.

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

The same source and framebuffer hashes remain exact after routing the load
through GFXRM, proving the refactor did not alter the sprite output.

`shapeData` and `mediaTexels` remain `NULL` throughout this path.

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

Wall texels use the layout expected by the original rasterizer:

```text
logical texel index = x * 64 + y
```

The same source and framebuffer hashes remain exact after routing the load
through GFXRM.

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
       v
native rasterizers
       |
       v
shared 160x120 RGB565 framebuffer
       |
       v
exact 2x physical presentation
```

The old map-wide graphics pools remain forbidden:

```text
shapeData   = NULL
mediaTexels = NULL
```

The next useful step is not to add arbitrary cache slots. It is to route one
**real projected renderer path** through GFXRM and measure actual repeated
resource access before choosing cache size, pack lifetime, hits/misses or an
eviction strategy.

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
```

Useful deterministic checks:

```text
sprite 172 texel FNV       = 0c0a7acd
sprite diagnostic FNV      = 001910a9
wall 112 texel FNV         = 92d40704
wall diagnostic FNV        = e39af2c4
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
- never recreate monolithic `shapeData` or `mediaTexels`
- do not preserve reverse-engineered implementation details merely for fidelity
  when a simpler ESP32-native design provides the same behaviour
- refactor subsystems incrementally rather than performing a speculative rewrite
- do not choose cache slot counts blindly; measure actual access patterns first
- keep audio out of the memory-critical bring-up until gameplay/render is stable
