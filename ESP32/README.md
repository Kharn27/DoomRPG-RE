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

During the migration period the SD card currently contains both the original
ZIP-backed data and the new ESP32-native asset pack:

```text
/DoomRPG.zip
/DoomRPG-ESP32.pak
```

`DoomRPG.zip` is still used by engine paths that have not yet been migrated.
`DoomRPG-ESP32.pak` is the beginning of the ESP32-native resource backend and is
designed for direct `seek + read` access without whole-file decompression.

## Building the ESP32 native asset pack

The pack is generated offline on the development machine. The ESP32 should not
spend RAM or CPU rebuilding or inflating these large source assets at runtime.

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

The first validated pack format contains the three graphics-heavy resources:

```text
bitshapes.bin
wtexels.bin
stexels.bin
```

For the reference game archive used during bring-up the generated pack is
305,743 bytes:

```text
pack header + index       112 B
bitshapes.bin          62,273 B
wtexels.bin           116,740 B
stexels.bin           126,618 B
--------------------------------
total                 305,743 B
```

The script prints offsets, sizes and CRC32 values. These are useful diagnostics
when validating an SD card or a future pack-format change.

## Why the native pack exists

The original DoomRPG-RE graphics path materializes large compressed resources
and then builds one large `mediaTexels` pool. This does not fit a classic ESP32
without PSRAM.

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

The ESP32-native path instead keeps large immutable data on the SD card and
reads only bounded working sets. The first hardware-validated random-access
probe reads one real 64x64 4-bpp wall texture using a 2,048-byte buffer and
returns the heap to exactly its starting value afterward.

The intended direction is therefore:

```text
PC/offline conversion
        |
        v
DoomRPG-ESP32.pak on SD
        |
        +--> small resident indexes
        +--> seek/read bounded texture data
        +--> future small texture/sprite caches
```

No runtime whole-file inflate is required for migrated assets, and the ESP32
must never recreate the monolithic desktop/mobile `mediaTexels` pool.

## Porting workflow

The project is intentionally developed in small hardware-validated increments:

1. create one branch from the latest validated `main`
2. implement one small measurable objective
3. build/flash/test it on the real CYD
4. fix failures on the same branch
5. after hardware success, update `PORTING_STATUS.md`
6. merge the branch
7. only then start the next increment

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
- avoid duplicate framebuffer/resource representations
- keep indexed/paletted graphics packed whenever possible
- do not preserve reverse-engineered implementation details merely for fidelity
  when a simpler ESP32-native design provides the same behaviour
- refactor subsystems incrementally rather than performing a large speculative
  rewrite
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
```

When reporting a hardware test, keep the complete block around the newest marker
plus the following `[ALIVE]` heartbeat. That preserves the authoritative heap
and largest-block measurements for the next increment.
