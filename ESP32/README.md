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

The historical DOOM-RPG-BREW-PATCH fixes texture/sprite read corruption on
specific BREW binaries; it is not the palette conversion mechanism used here.

## Native sprite rendering contract

A complete real Doom RPG sprite is hardware-validated through our native path
with both legacy graphics pools absent.

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

Resource flow:

```text
DoomRPG-ESP32.pak
        |
        +--> bitshapes.bin : source header + bounded mask
        +--> stexels.bin   : bounded 4-bpp payload
        |
        v
EspNativeSpriteFrame
        |
        +--> mapped 16-color palette
        +--> canonical RGB565
        |
        v
native sprite rasterizer
        |
        v
shared 160x120 framebuffer
```

`shapeData` and `mediaTexels` remain `NULL` throughout this path.

## Native wall rendering contract

A complete real 64x64 wall texture is also hardware-validated through a native
consumer, again with `mediaTexels == NULL`.

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

The source byte regression markers are:

```text
first = aa b5 44 b4
last  = e5 ee ee ce
```

Wall texels use the layout expected by the original rasterizer:

```text
logical texel index = x * 64 + y
```

Two texels are packed into each byte. The second mapping value supplies the
16-color palette offset.

Resource flow:

```text
DoomRPG-ESP32.pak
        |
        +--> wtexels.bin : direct 2048 B seek/read
        |
        v
EspNativeWallFrame
        |
        +--> mapped 16-color palette
        +--> canonical RGB565
        |
        v
native wall rasterizer
        |
        v
shared 160x120 framebuffer
```

Hardware memory result:

```text
before       heap8=30688 largest8=21492
resident     heap8=28624 largest8=21492
released     heap8=30688 largest8=21492 deltaFromStart=0
```

This proves that a real wall can be rendered from original Doom RPG data without
creating the old 172 KB wall pool.

## Current native graphics architecture

Two complete native graphics primitives are now proven:

```text
                 DoomRPG-ESP32.pak
                         |
             +-----------+-----------+
             |                       |
          sprite                    wall
    bitshape + stexels            wtexels
             |                       |
       bounded frame            bounded frame
             |                       |
             +-----------+-----------+
                         |
                 canonical RGB565
                         |
                 native rasterizers
                         |
                 shared framebuffer
```

The next step should consolidate duplicated resource-loading logic behind a
small reusable native graphics resource manager. Do not interpret that as a
license to build a large cache immediately: cache size and eviction policy must
follow measured runtime access patterns.

## Native serial diagnostics

Important successful markers currently include:

```text
[ASSETPAK]
[BITSHAPE]
[SPRITETEX]
[PALETTE]
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
Useful commands, SD preparation, hardware measurements, architectural pivots and
validated conventions belong in `README.md`, `PORTING_STATUS.md`, or another
appropriate `.md` before merge.

## ESP32 design rules

- preserve game behaviour and data compatibility, not unnecessary PC/mobile
  memory architecture
- prefer bounded allocations with known maximum sizes
- treat SD as secondary storage and RAM as a small working/cache area
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
