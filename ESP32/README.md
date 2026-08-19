# Doom RPG ESP32 port

This directory contains the ESP32-specific port and bring-up work for the classic
ESP32-2432S028R (Cheap Yellow Display, no PSRAM).

The ESP32 target is treated as its own constrained architecture, not as a PC
backend with smaller buffers. The goal is to keep gameplay/data compatibility
while progressively replacing memory-heavy desktop/mobile resource paths with
bounded ESP32-native ones.

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
when build-system/generated-source changes make it useful:

```bash
cd ESP32
pio run -t clean
pio run -t upload
```

## Game data on the SD card

During the migration period the SD card contains both the original ZIP-backed
data and the new ESP32-native asset pack:

```text
/DoomRPG.zip
/DoomRPG-ESP32.pak
```

`DoomRPG.zip` is still used by engine paths that have not yet been migrated.
`DoomRPG-ESP32.pak` is the ESP32-native resource backend and is designed for
direct `seek + read` access without runtime decompression.

The long-term goal is to remove the ZIP dependency once every required runtime
path has moved to the native pack.

## Building the ESP32 native asset pack

The pack is generated offline on the development machine. The ESP32 should not
spend RAM or CPU rebuilding or inflating immutable game resources at runtime.

From the repository root:

```bash
python3 ESP32/tools/build_asset_pack.py \
    /path/to/DoomRPG.zip \
    /path/to/DoomRPG-ESP32.pak
```

Example when writing directly to a mounted SD card:

```bash
python3 ESP32/tools/build_asset_pack.py \
    /media/$USER/DOOMRPG/DoomRPG.zip \
    /media/$USER/DOOMRPG/DoomRPG-ESP32.pak
```

The exact mount point depends on the development machine.

To print every generated asset record:

```bash
python3 ESP32/tools/build_asset_pack.py --list \
    /path/to/DoomRPG.zip \
    /path/to/DoomRPG-ESP32.pak
```

The script always performs a structural self-check after writing the file and
must finish with:

```text
[PACK] self-check: OK
```

### Pack format v2

Pack v2 mirrors **every ZIP directory entry**, not only the three large graphics
files used by the first proof-of-concept.

The source ZIP used during bring-up has 241 entries. For 241 entries the v2
metadata occupies only:

```text
header                   24 B
241 index records x 20  4820 B
------------------------------
index/data boundary     4844 B
```

The rest of the file is the original resources stored **uncompressed** and
contiguously so they are directly seekable.

Each 20-byte index record contains only:

```text
normalized-name FNV-1a hash
payload offset
payload size
CRC32
flags
```

The records are sorted by name hash. The ESP32 therefore performs binary-search
lookups directly on the SD card and does not allocate the complete 241-entry
index in RAM.

The offline builder rejects case-insensitive duplicate names and any FNV-1a name
hash collision. Runtime lookups normalize ASCII case, leading `/`, and `\\` to
`/`, so calls such as these resolve the same resource:

```text
menu.bsp
/MENU.BSP
```

The generated total pack size depends on the sum of the ZIP's uncompressed
payload sizes. The builder prints the exact entry count, payload size and final
pack size every time.

### Important after the v1 prototype

The original proof-of-concept pack contained only:

```text
bitshapes.bin
wtexels.bin
stexels.bin
```

and was 305,743 bytes for the bring-up archive.

That **v1 pack is intentionally incompatible with the v2 reader**. Regenerate
`DoomRPG-ESP32.pak` with the command above and replace the old file on the SD
card if an old v1 pack is still present.

Once v2 is on the SD card, firmware increments can migrate additional resources
to the native backend without rebuilding a different subset pack each time.

## Why the native pack exists

The original DoomRPG-RE graphics path materializes large compressed resources
and then builds large resident graphics pools. This does not fit a classic
ESP32 without PSRAM.

For the real menu map the validated values are:

```text
mapTextureTexelsCount = 84
mapSpriteTexelsCount  = 284
planeTexturesCnt      = 11
```

The wall-only part of the original `mediaTexels` allocation would already be:

```text
84 * 64 * 64 / 2 = 172,032 bytes
```

The selected menu sprite dataset is also large as a whole:

```text
selected sprite texels = 143,990 B
legacy expanded shapeData = 55,676 B
```

The ESP32-native path instead keeps immutable data on the SD card and reads only
bounded working sets.

Hardware-validated working-set ceilings so far:

```text
bitshape mask-column scratch     <= 32 B
largest selected sprite payload   = 1,600 B
one packed wall texture           = 2,048 B
```

The intended architecture is:

```text
PC/offline conversion
        |
        v
DoomRPG-ESP32.pak on SD
        |
        +--> on-disk hash index
        +--> seek/read bounded resource data
        +--> small measured runtime caches/frames
```

No runtime whole-file inflate is required for migrated assets, and the ESP32
must never recreate the monolithic desktop/mobile `shapeData` or `mediaTexels`
pools.

## Native sprite rendering contract

A complete real Doom RPG sprite has now been rendered on the physical CYD using
an ESP32-native path with both legacy graphics pools absent.

Validated worst-case menu sprite used by the first renderer consumer:

```text
sprite index        = 172
size                = 64x64
active pixels       = 3199
bitshape mask       = 512 B
packed texels       = 1600 B
logical frame data  = 2112 B
allocator cost      = 2128 B
palette offset      = 1616
```

The resource flow is:

```text
DoomRPG-ESP32.pak
        |
        +--> bitshapes.bin : source header + bounded mask
        +--> stexels.bin   : bounded 4-bpp payload
        |
        v
EspNativeSpriteFrame
        |
        +--> existing 16-color palette mapping
        +--> native palette normalization
        |
        v
ESP32-native rasterizer
        |
        v
shared 160x120 RGB565 framebuffer
        |
        v
exact 2x output to 320x240 CYD
```

The validated sprite texel payload hash is:

```text
fnv1a = 0c0a7acd
```

The first palette-correct diagnostic framebuffer signature is:

```text
framebufferFNV = 001910a9
```

These values are useful regression markers for the exact diagnostic scene.

### Palette convention

The reconstructed `Render_loadPalettes()` path leaves `mediaPalettes` in the
legacy red/blue ordering used by the old renderer. ESP32-native consumers use a
canonical RGB565 framebuffer, so the palette is normalized once at the native
rendering boundary.

Validated diagnostic:

```text
[PALETTE] Normalized 3280 entries legacy R/B order -> framebuffer RGB565
[PALETTE] sprite172 offset=1616 first4 before=0000,ffff,c000,07ff after=0000,ffff,0018,ffe0
```

Do **not** compensate palette ordering by changing `stexels.bin` data or nibble
order. Sprite texels are already hardware-proven byte-for-byte correct.

The separate DOOM-RPG-BREW-PATCH addresses a historical BREW read-corruption
problem in `wtexels`/`stexels`; it is not the palette conversion mechanism used
by this ESP32 port.

## Native-pack serial diagnostics

The full-pack bring-up probe opens the v2 pack while the real menu runtime
structures remain resident. It then:

1. validates the complete on-disk index without allocating it in RAM
2. checks the pack entry count against the already-indexed `DoomRPG.zip`
3. resolves every ZIP entry name through the pack's binary-search lookup
4. compares every uncompressed size
5. proves `pack size = index boundary + total uncompressed ZIP payload`
6. checks representative resources such as `menu.bsp` and `mappings.bin`
7. performs the validated 2,048-byte real wall-texture random read
8. closes the pack and verifies that heap usage returns to the starting value

Important successful markers include:

```text
[ASSETPAK] FULL directory cross-check matched=.../...
[ASSETPAK] READY complete ZIP mirrored as directly seekable ESP32 pack
[BITSHAPE] READY on-demand bitshape source model validated; legacy shapeData eliminated
[SPRITETEX] READY largest selected sprite payload read directly from stexels.bin
[PALETTE] READY native consumers now see canonical RGB565
[SPRITERENDER] READY real sprite rendered without shapeData or mediaTexels
```

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
Useful commands, SD preparation, measured hardware values, architectural pivots
and validated conventions should be written into `README.md`,
`PORTING_STATUS.md`, or another appropriate `.md` before merge.

This is important because many changes involve memory layout, SD access,
renderer state or linker wrapping that cannot be fully validated from a desktop
build alone.

## ESP32 design rules

These rules should guide future cleanup as DoomRPG-RE is progressively adapted
into an ESP32-oriented engine:

- preserve game behaviour and data compatibility, not unnecessary PC/mobile
  memory architecture
- prefer bounded allocations with known maximum sizes
- treat SD as secondary storage and RAM as a small working/cache area
- avoid whole-file decompression for large immutable resources
- avoid resident indexes when a compact on-disk index is sufficient
- avoid duplicate framebuffer/resource representations
- keep indexed/paletted graphics packed whenever possible
- use one explicit native RGB565 convention at the rendering boundary
- do not preserve reverse-engineered implementation details merely for fidelity
  when a simpler ESP32-native design provides the same behaviour
- refactor subsystems incrementally rather than performing a large speculative
  rewrite
- do not choose cache slot counts blindly; measure access patterns first
- keep audio out of the memory-critical bring-up until the gameplay/render path
  is stable

## Useful diagnostics

The serial monitor runs at 115200 baud. Successful bring-up increments print
named markers such as:

```text
[CORE]
[LAYOUT]
[PRERENDER]
[RENDERSTART]
[MAPPINGS]
[MENUBSP]
[BSPPLAN]
[MAPSTRUCT]
[RESOURCEPLAN]
[ASSETPAK]
[BITSHAPE]
[SPRITETEX]
[PALETTE]
[SPRITERENDER]
```

When reporting a hardware test, keep the complete block around the newest marker
plus the following `[ALIVE]` heartbeat. That preserves the authoritative heap,
largest-block and deterministic rendering measurements for the next increment.
