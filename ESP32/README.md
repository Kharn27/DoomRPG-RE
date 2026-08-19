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

That **v1 pack is intentionally incompatible with the v2 reader**. After pulling
the full-pack branch, regenerate `DoomRPG-ESP32.pak` with the command above and
replace the old file on the SD card.

Once v2 is on the SD card, future firmware increments can migrate additional
resources to the native backend without rebuilding a different subset pack each
time.

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

The intended architecture is:

```text
PC/offline conversion
        |
        v
DoomRPG-ESP32.pak on SD
        |
        +--> on-disk hash index
        +--> seek/read bounded resource data
        +--> future small texture/sprite caches
```

No runtime whole-file inflate is required for migrated assets, and the ESP32
must never recreate the monolithic desktop/mobile `mediaTexels` pool.

## Native-pack serial diagnostics

The current bring-up probe opens the full v2 pack while the real menu runtime
structures remain resident. It then:

1. validates the complete on-disk index without allocating it in RAM
2. checks the pack entry count against the already-indexed `DoomRPG.zip`
3. resolves every ZIP entry name through the pack's binary-search lookup
4. compares every uncompressed size
5. proves `pack size = index boundary + total uncompressed ZIP payload`
6. checks representative resources such as `menu.bsp` and `mappings.bin`
7. performs the already-validated 2,048-byte real wall-texture random read
8. closes the pack and verifies that heap usage returns to the starting value

The important successful markers are:

```text
[ASSETPAK] FULL directory cross-check matched=.../...
[ASSETPAK] FULL pack size proven index+payload=...B
[ASSETPAK] READY complete ZIP mirrored as directly seekable ESP32 pack
[ASSETPAK] READY random-access real wall texture read with 2048B working set
```

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
- avoid resident indexes when a compact on-disk index is sufficient
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
