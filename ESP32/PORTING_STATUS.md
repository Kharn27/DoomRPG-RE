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
    (`agent/esp32-menu-map-runtime-structures`, PR #11).
13. Graphics-resource memory plan proving the original whole-file inflate and
    monolithic `mediaTexels` strategy cannot fit on the no-PSRAM CYD
    (`agent/esp32-menu-resource-memory-plan`, PR #12).
14. First ESP32-native asset-pack primitive: direct random access to a real
    2,048-byte menu wall texture with exact heap recovery
    (`agent/esp32-native-asset-pack`, PR #13, merge commit
    `0a3a9fcc0eb284b8b46f4a994a8e3c85469672f3`).

## Current validated increment

Branch: `agent/esp32-full-native-asset-pack`

Status: **HARDWARE VALIDATED, READY TO MERGE**.

Objective: replace the three-entry prototype pack with one complete ESP32-native
resource pack containing every resource from `DoomRPG.zip`, while keeping the
index on SD and preserving bounded random access.

## ESP32-native asset pack v2

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

The v2 pack stores every ZIP member uncompressed for direct seek/read. Its index
is a 24-byte header followed by 20-byte records sorted by a normalized FNV-1a
name hash. The complete index remains on SD; the ESP32 reader does not allocate
a resident table for all resources.

Hardware-validated full-pack layout:

```text
entries                  = 241
header                    = 24 B
index                     = 241 x 20 B = 4,820 B
dataOffset                = 4,844 B
uncompressed payload      = 2,452,554 B
total pack size           = 2,457,398 B
```

Hardware proof:

```text
[ASSETPAK] READY v2 entries=241 fileSize=2457398 dataOffset=4844 heapCost=4376B
[ASSETPAK] Index stays on SD: header=24B records=241 x 20B total=4844B
[ASSETPAK] FULL directory cross-check matched=241/241 payload=2452554B
[ASSETPAK] FULL pack size proven index+payload=2457398B
```

All 241 resources from the already-indexed source ZIP were found in the native
pack and matched their expected uncompressed sizes. The calculated payload plus
header/index exactly matches the physical pack size.

## Validated representative entries

```text
[ASSETPAK] bitshapes.bin    hash=2c085dbd offset=1795904 size=62273 crc32=5a7c8a2b flags=0
[ASSETPAK] wtexels.bin      hash=29d7d23c offset=2340658 size=116740 crc32=32e758b5 flags=0
[ASSETPAK] stexels.bin      hash=7d7834e8 offset=2214040 size=126618 crc32=a44ead7e flags=0
[ASSETPAK] mappings.bin     hash=bb74455d offset=2178480 size=8392 crc32=fc4fd8fa flags=0
[ASSETPAK] /MENU.BSP        hash=9b797301 offset=2186872 size=4494 crc32=0e81e02a flags=0
```

Normalized lookup is hardware-validated:

```text
[ASSETPAK] Normalized lookup '/MENU.BSP' -> hash=9b797301 size=4494 OK
```

Therefore callers are not required to reproduce ZIP filename case or a leading
slash exactly.

## Real menu wall-texture proof still passes on the full pack

The real menu map remains resident with:

```text
mapTextureTexelsCount=84
mapSpriteTexelsCount=284
planeTexturesCnt=11
```

The first real referenced wall texture still resolves through the original
mapping metadata:

```text
[ASSETPAK] wtexels source header dataSize=116736
[ASSETPAK] Real menu texture index=112 texelOffset=65536 byteOffset=32768 read=2048B
```

Only one bounded packed 64x64 4-bpp wall texture buffer is allocated:

```text
[ASSETPAK] Bounded texture buffer resident heap8=24248 largest8=21492 used=2064
```

The bytes are identical to the previously validated v1 pack result:

```text
[ASSETPAK] READ texture=112 bytes=2048 fnv1a=92d40704 first=aab544b4 last=e5eeeece
```

After freeing the texture buffer:

```text
[ASSETPAK] Released texture buffer heap8=26312 largest8=21492 delta=0
```

After closing the full pack:

```text
[ASSETPAK] Closed FULL pack heap8=30688 deltaFromOpenStart=0
```

So the complete v2 validation returns exactly to its starting 8-bit heap state.

Final hardware result:

```text
[ASSETPAK] READY complete ZIP mirrored as directly seekable ESP32 pack
[ASSETPAK] READY random-access real wall texture read with 2048B working set
[ASSETPAK] No full-pack index allocation, ZIP inflate, or monolithic mediaTexels executed
[MAPSTRUCT] Native asset pack random-access probe complete
```

Heartbeat remains stable afterwards:

```text
[ALIVE] uptime=5002 ms heap=96504 heap8=30688 largest8=21492 ...
[ALIVE] uptime=10003 ms heap=96504 heap8=30688 largest8=21492 ...
```

Touch and shared-framebuffer presentation also remain operational.

## Current authoritative memory boundary

Immediately before the native full-pack probe:

```text
heap8=30,688 B
largest8=21,492 B
```

Opening the Arduino SD `File` for the pack costs 4,376 B of 8-bit heap. That
memory is fully recovered when the pack is closed. The on-disk index itself is
not copied into a 241-entry RAM table.

The original wall-only `mediaTexels` lower bound is still:

```text
84 * 64 * 64 / 2 = 172,032 B
```

so the original `Render_loadTexels()` architecture remains forbidden on this
target.

## Architectural conclusion

The asset backend is now no longer a probe-only three-file mechanism. One stable
`DoomRPG-ESP32.pak` contains all 241 currently known game resources and is ready
for incremental migration of legacy ZIP consumers without changing SD contents
between firmware increments.

Validated resource path:

```text
DoomRPG.zip on development PC
        |
        v
build_asset_pack.py
        |
        v
DoomRPG-ESP32.pak on SD
        |
        +--> hash-sorted index kept on SD
        +--> binary-search lookup by normalized resource name
        +--> direct seek/read of bounded ranges
```

This is now the preferred ESP32 resource architecture. `DoomRPG.zip` remains on
the SD only as a temporary compatibility source for engine paths not yet
migrated.

## Current safe stop boundary

Validated and executed:

- complete engine startup through mappings
- real `Render_beginLoadMap(MAP_MENU)`
- real structural `Render_beginLoadMapData()` phase
- real nodes, lines, sprites, events, bytecodes and resource reference lists
- graphics resource memory diagnostic
- full 241-entry native pack generation
- complete on-device ZIP-directory versus native-pack cross-check
- normalized hashed resource lookup from an SD-resident index
- direct random-access read of a real menu wall texture
- bounded 2,048-byte packed wall-texture working set
- exact heap recovery after resource read and pack close

Still intentionally NOT executed:

- original `Render_loadBitShapes()`
- original `Render_loadTexels()`
- monolithic `mediaTexels`
- native bitshape metadata loading
- native sprite texel loading/cache
- native wall-texture cache / renderer integration
- completion of map graphics loading
- game entities/player spawning
- main game loop

## Recommended next increment after merge

The SD asset set is now stable. Do not redesign the pack again unless a concrete
need appears.

Recommended next objective: migrate the first real engine resource consumer to
the native backend instead of merely probing it. The graphics path remains the
priority.

A useful next step is to implement a bounded ESP32-native bitshape reader on top
of `EspAssetPack_readRange()` so the real 284 menu sprite references can be
inspected/loaded without ever materializing the 62,273-byte `bitshapes.bin`.
This should establish the exact selected bitshape metadata and sprite texel
requirements before choosing the final sprite cache representation.

Do not re-enable the original heavy graphics loaders.

## Increment discipline

- Start each new increment from the latest hardware-validated `main`.
- One branch = one small testable objective.
- Fix failures on the same branch; do not create the next branch early.
- Hardware validation on the real CYD is the merge gate.
- Record measured hardware values here after every successful increment.
- Keep `ESP32/README.md` updated with commands, SD preparation and tooling so
  operational knowledge is not left only in chat history.
