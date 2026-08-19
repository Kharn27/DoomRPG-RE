# Doom RPG ESP32 CYD porting status

This file is the recovery point for the ESP32-2432S028R (classic Cheap Yellow
Display, no PSRAM) port. Update it after every hardware-validated increment.

## Target

- ESP32-2432S028R / classic ESP32 CYD, no PSRAM
- ESP32-D0WD-V3, 240 MHz, 4 MB flash
- ILI9341 320x240 landscape
- XPT2046 touch on separate software SPI path
- microSD-backed game data
- internal render target: 160x120 RGB565 = 38,400 bytes
- physical output: exact nearest-neighbour 2x to 320x240
- audio disabled/stubbed during bring-up

## Hardware-validated milestones already merged to main

1. TFT / SD / engine link bring-up.
2. Touch calibration/orientation (`agent/esp32-touch-input-layer`, PR #1).
3. Native 160x120 render target + exact 2x output (PR #2).
4. SDL compatibility renderer sharing the platform framebuffer (PR #3).
5. Real 12-object Doom RPG core graph (PR #4).
6. Real `DoomCanvas_startup()` + HUD/font/legal resources + 160x120 layout
   (`agent/esp32-engine-layout-160x120`, PR #5).
7. `ParticleSystem_startup()`, `MenuSystem_startup()` and `EntityDef_startup()`
   with packed indexed BMP storage (`agent/esp32-pre-render-startup`, PR #6).
8. Real `Render_startup()` using the shared platform framebuffer, plus
   `sintable.bin` and `palettes.bin` (`agent/esp32-render-startup-shared-framebuffer`, PR #7).
9. `Game_loadConfig()` first-boot path + real `Render_loadMappings()`
   (`agent/esp32-config-mappings-startup`, PR #8).
10. Real `/menu.bsp` ZIP load/inflate + exact fixed 33-byte header parse
    (`agent/esp32-menu-bsp-preflight`, PR #9).
11. Complete byte-for-byte parse of `menu.bsp` and exact ESP32 structural
    allocation plan (`agent/esp32-menu-bsp-structure-plan`, PR #10).
12. Real `Render_beginLoadMap(MAP_MENU)` + real structural portion of
    `Render_beginLoadMapData()`, stopped exactly before graphics resources
    (`agent/esp32-menu-map-runtime-structures`, PR #11, merge commit
    `12e64c464e8dcfc477515a38955de116c69a8730`).
13. Graphics-resource memory plan proving the original whole-file inflate and
    monolithic `mediaTexels` strategy cannot fit on the no-PSRAM CYD
    (`agent/esp32-menu-resource-memory-plan`, PR #12, merge commit
    `7c6e72eab56cc7696262104f206f09d8dfcbd169`).

## Current validated increment

Branch: `agent/esp32-native-asset-pack`

Status: **HARDWARE VALIDATED, READY TO MERGE**.

Objective: prove the first ESP32-native resource path using an offline-generated
pack with direct SD random access, without whole-file ZIP inflate and without
allocating the original monolithic `mediaTexels` pool.

This increment deliberately starts small: the pack contains only the three heavy
graphics BIN resources (`bitshapes.bin`, `wtexels.bin`, `stexels.bin`). The goal
is architectural validation before scaling the pack to the complete game data.

## ESP32-native asset pack v1

Offline tool:

```text
ESP32/tools/build_asset_pack.py
```

Input:

```text
DoomRPG.zip
```

Output copied to the SD root:

```text
/DoomRPG-ESP32.pak
```

The v1 pack is intentionally uncompressed at runtime. The PC performs extraction
once, and the ESP32 performs only indexed `seek + read` operations.

Validated pack layout and exact size:

```text
header + index        112 B
bitshapes.bin      62,273 B
wtexels.bin       116,740 B
stexels.bin       126,618 B
---------------------------
total             305,743 B
```

Hardware saw the expected entries and CRC32 values:

```text
[ASSETPAK] READY index entries=3 fileSize=305743 heapCost=4380B
[ASSETPAK] bitshapes.bin offset=112 size=62273 crc32=5a7c8a2b flags=0
[ASSETPAK] wtexels.bin   offset=62385 size=116740 crc32=32e758b5 flags=0
[ASSETPAK] stexels.bin   offset=179125 size=126618 crc32=a44ead7e flags=0
```

The 4,380-byte heap cost while the pack is open comes from the Arduino SD/File
stack and associated filesystem state; the asset reader itself uses a fixed
static index and performs no per-read dynamic allocation.

## Real menu texture random-access proof

The real menu runtime structures remain resident and still produce:

```text
mapTextureTexelsCount=84
mapSpriteTexelsCount=284
planeTexturesCnt=11
```

At the native-pack probe boundary:

```text
[ASSETPAK] Opening /DoomRPG-ESP32.pak heap8=30408 largest8=22516
```

The probe uses the real engine mapping arrays to select the first wall texture
actually referenced by `menu.bsp`:

```text
[ASSETPAK] wtexels source header dataSize=116736
[ASSETPAK] Real menu texture index=112 texelOffset=65536 byteOffset=32768 read=2048B
```

A packed Doom RPG wall texture is 64x64 at 4 bpp, therefore exactly 2,048 bytes.
Only that bounded working buffer is allocated:

```text
[ASSETPAK] Bounded texture buffer resident heap8=23964 largest8=20468 used=2064
```

The 16-byte difference from the 2,048-byte payload is allocator bookkeeping.

The real resource bytes are read directly from the pack using SD seek/read:

```text
[ASSETPAK] READ texture=112 bytes=2048 fnv1a=92d40704 first=aab544b4 last=e5eeeece
```

No ZIP decompression occurs and no full `wtexels.bin` buffer exists in RAM.

## Heap recovery proof

After the 2,048-byte texture buffer is freed:

```text
[ASSETPAK] Released texture buffer heap8=26028 largest8=22516 delta=0
```

After closing the pack:

```text
[ASSETPAK] Closed pack heap8=30408 deltaFromOpenStart=0
```

Therefore the complete native random-access test returns exactly to its starting
8-bit heap state.

Final hardware result:

```text
[ASSETPAK] READY random-access real wall texture read with 2048B working set
[ASSETPAK] No ZIP inflate and no monolithic mediaTexels allocation executed
[MAPSTRUCT] Native asset pack random-access probe complete
```

Heartbeat and touch/shared-framebuffer operation remain stable afterwards:

```text
[ALIVE] ... heap=96224 heap8=30408 largest8=22516 ... MENUBSP=ready ...
```

This branch therefore passes its hardware merge gate.

## Architectural conclusion

This is the first hardware proof of an ESP32-native graphics resource path.

The original DoomRPG-RE graphics model is no longer the target architecture on
ESP32. DoomRPG-RE remains the behavioral/data-format reference, while the ESP32
port is free to replace resource management and rendering internals with bounded,
platform-specific implementations.

Proven old path to be impossible:

```text
ZIP entry
  -> compressed buffer
  -> 10,992-byte miniz state
  -> whole decompressed BIN
  -> monolithic mediaTexels (>=172,032 B wall-only on menu map)
```

Proven new primitive:

```text
ESP32 asset pack on SD
  -> indexed entry
  -> seek to requested source offset
  -> read one 2,048-byte packed texture
  -> render/cache consumer
```

This makes the no-PSRAM classic CYD remain a viable target.

## Important previous memory result

The menu-map graphics baseline before the native pack probe is approximately:

```text
heap8=30,408 B
largest8=22,516 B
mapTextureTexelsCount=84
mapSpriteTexelsCount=284
planeTexturesCnt=11
```

The original wall-only `mediaTexels` lower bound remains:

```text
84 * 64 * 64 / 2 = 172,032 B
```

so the original loader must never be re-enabled on this target.

## Current safe stop boundary

Validated and executed:

- complete engine startup through mappings
- real `Render_beginLoadMap(MAP_MENU)`
- real structural `Render_beginLoadMapData()` phase
- real nodes, lines, sprites, events, bytecodes and resource reference lists
- graphics resource memory diagnostic
- offline ESP32-native asset pack generation
- native pack index parsing from SD
- direct random-access read of a real menu wall texture
- bounded 2,048-byte packed wall-texture working set
- exact heap recovery after resource read

Still intentionally NOT executed:

- original `Render_loadBitShapes()`
- original `Render_loadTexels()`
- whole-file `bitshapes.bin`, `wtexels.bin`, `stexels.bin` loading
- monolithic `mediaTexels`
- native sprite/bitshape loading
- native texture cache / renderer integration
- completion of map graphics loading
- game entities/player spawning
- main game loop

## Recommended next increment after merge

Scale the asset-pack concept now that the primitive is hardware-proven.

Recommended direction:

1. generate a complete ESP32 game pack offline rather than a three-entry probe
   pack, so normal development no longer requires repeatedly changing SD files
2. make the pack format/index suitable for all Doom RPG resources, including
   names/IDs that do not fit the v1 three-entry assumptions
3. keep resources stored in a form optimized for cheap ESP32 random access;
   runtime decompression should only be used where it provides a demonstrated
   benefit and remains strictly bounded
4. retain `/DoomRPG.zip` temporarily for legacy code paths while migration is
   incremental; remove that dependency only when all required resource classes
   have migrated
5. after the full pack is validated, implement the first real native graphics
   consumer (bitshape metadata or a tiny wall-texture cache) on top of
   `EspAssetPack_readRange()`

Do not attempt to make the original heavy graphics loaders fit. Replace their
responsibility incrementally with ESP32-native code.

## Increment discipline

- Start each new increment from the latest hardware-validated `main`.
- One branch = one small testable objective.
- Fix failures on the same branch; do not create the next branch early.
- Hardware validation on the real CYD is the merge gate.
- Record measured hardware values here after every successful increment.
