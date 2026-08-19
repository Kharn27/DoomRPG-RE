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
record contains the normalized-name FNV-1a hash, payload offset, payload size,
CRC32 and flags. The index remains on SD and is binary-searched on demand.

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

Native consumers use canonical RGB565 in the shared framebuffer. The ESP32 path
normalizes the reconstructed legacy palette once before native rendering.

Validated diagnostic:

```text
[PALETTE] Normalized 3280 entries legacy R/B order -> framebuffer RGB565
[PALETTE] sprite172 offset=1616 first4 before=0000,ffff,c000,07ff after=0000,ffff,0018,ffe0
```

Do not compensate palette ordering by modifying `stexels.bin`, `wtexels.bin` or
nibble order. Those source bytes are already hardware-proven.

## Native graphics resource manager (GFXRM)

Sprite and wall render paths share one storage/file-format boundary:

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

The manager is deliberately **not a cache yet**. Each successful load opens the
pack, performs the bounded read and closes it before returning the frame.

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

The projected wall path no longer needs the compatibility alias introduced in
PR #20.

The previous proof temporarily did this during one projected draw:

```text
mediaTexels                : NULL -> bounded 2 KB frame -> NULL
mediaTexelOffsets[112 * 2] : 65536 -> 0 -> 65536
```

That transition is now removed completely.

The current hardware-validated path is:

```text
GFXRM wall frame (2,048 B)
        |
        v
Render_transform2DVerts()
        |
Render_clipLine()
        |
Render_projectVertex()
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

The native wall-span implementation keeps the same fixed-point branch as the
reference renderer (`FIXED_VERSION=1`) and preserves the original mode-0 wall
column interpolation semantics.

### Strong runtime invariants

During every native projected wall span:

```text
render->mediaTexels == NULL
mediaTexelOffsets[112 * 2] == 65536
```

The probe tracks violations per span, not merely before and after the draw.
Hardware result:

```text
[PROJWALL] RELEASE texture=112 spans=40 pixels=1600 rangeErrors=0 legacyPtrViolations=0 mappingViolations=0 mediaTexels=0x0
[PROJWALL] Native span stats begin=1 end=1 bound=2048B spans=40 pixels=1600 rangeErrors=0 legacyPtrViolations=0 mappingViolations=0
```

So the current mode-0 projected wall path has:

```text
legacy pointer violations = 0
mapping rewrite violations = 0
range errors = 0
```

### Bit-identical projected output

The same deterministic test wall is kept for regression:

```text
v1=(128,-32,z0)
v2=(128, 32,z64)
camera=(0,0,z32)
spanMode=0
```

Hardware result:

```text
[PROJWALL] PROJECTED columns=60..100 count=40 scale=81920/81920 z=0/5242880 lineRasterCount=1 changedPixels=1600
[PROJWALL] Source texture=112 palette=480 sourceOffset=65536 texelHash=92d40704
[PROJWALL] GFXRM stats spriteLoads=0 wallLoads=1 packOpenCycles=1 logicalBytes=2048 peakFrame=2048
[PROJWALL] framebufferFNV=ad191f54 expected=ad191f54 mediaTexelsDuring=0x0 mappingOffsetDuring=65536 mediaTexelsAfter=0x0 mappingOffsetAfter=65536
[PROJWALL] READY projected wall is bit-identical with direct bounded GFXRM sampling
[PROJWALL] READY mediaTexels stayed NULL and global mapping stayed untouched for every native span
```

The framebuffer hash is **exactly the same** as the PR #20 compatibility path:

```text
projected wall FNV = ad191f54
```

That proves the direct native sampler changed the memory architecture without
changing the projected output.

### Memory result

Current branch result:

```text
before    heap8=30576 largest8=22516
resident  heap8=28512 largest8=20468
released  heap8=30576 largest8=22516
```

The wall frame still costs 2,064 allocator bytes for 2,048 logical bytes.
`largest8` may shrink during residency depending on allocator placement; the
contract is exact restoration after release.

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
       |            +--> native projected wall spans
       |                        |
       v                        v
native sprite raster       RGB565 wall pixels
       \                        /
        \                      /
         shared 160x120 framebuffer
                    |
                    v
             exact 2x output
```

The current hard rules are:

```text
shapeData   = NULL
mediaTexels = NULL
```

for the validated native projected wall path as well as the standalone native
sprite/wall consumers.

The global mapping remains the authoritative logical resource mapping and is no
longer rewritten to fit a local frame.

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

- preserve game behaviour/data compatibility, not unnecessary PC/mobile memory
  architecture
- prefer bounded allocations with known maximum sizes
- treat SD as secondary storage and RAM as a small working/cache area
- isolate storage/file-format concerns from rasterizers
- avoid whole-file decompression for large immutable resources
- avoid duplicate framebuffer/resource representations
- keep indexed/paletted graphics packed whenever possible
- use one explicit canonical RGB565 convention at the native rendering boundary
- never recreate monolithic `shapeData` or map-wide `mediaTexels`
- do not rewrite global resource mappings merely to make a bounded frame look
  like a legacy monolithic pool
- require exact allocator-state restoration after release rather than assuming
  which free block services an allocation
- preserve deterministic framebuffer hashes when replacing a legacy renderer
  primitive with a native equivalent
- refactor subsystems incrementally rather than performing a speculative rewrite
- do not choose cache slot counts blindly; measure actual real-map access first
- keep audio out of the memory-critical bring-up until gameplay/render is stable

## Next renderer direction

The next useful step is to use the validated direct native projected-wall source
with **real `menu.bsp` wall data** instead of only the synthetic regression line.
Keep that first scene increment walls-only or otherwise tightly controlled so it
can measure actual texture request repetition before any cache policy is chosen.
