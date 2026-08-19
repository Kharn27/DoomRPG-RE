# Doom RPG ESP32 CYD porting status

This file is the recovery point for the ESP32-2432S028R (classic Cheap Yellow
Display, no PSRAM) port. Update it as part of every hardware-validated increment,
on the same branch as the code, before that branch is merged.

## Target

- ESP32-2432S028R / classic ESP32 CYD, no PSRAM
- ESP32-D0WD-V3, 240 MHz, 4 MB flash
- ILI9341 320x240 landscape
- XPT2046 touch on separate software SPI path
- microSD-backed game data
- internal render target: 160x120 RGB565 = 38,400 bytes
- physical output: exact nearest-neighbour 2x to 320x240
- gameplay viewport: 160x80 at framebuffer y=20
- audio disabled/stubbed during bring-up

## Project direction

DoomRPG-RE is treated as an executable specification for behaviour, formats and
rendering semantics, not as an architecture contract. The ESP32 implementation
is progressively becoming its own engine specialized for a classic no-PSRAM
ESP32:

- bounded deterministic RAM use
- SD-backed immutable resources
- small measured working sets / future caches
- no whole-resource graphics inflation
- no monolithic `shapeData`
- no monolithic/map-wide `mediaTexels`
- shared 160x120 RGB565 framebuffer
- canonical RGB565 palette convention for native consumers
- storage/file-format access isolated behind GFXRM
- original projection/game semantics preserved where useful
- reverse-engineered desktop memory architecture removed incrementally

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
    (`agent/esp32-native-asset-pack`, PR #13).
15. Full ESP32-native asset pack v2: all 241 ZIP resources mirrored uncompressed
    behind a hash-sorted on-disk index, with 241/241 hardware cross-check and
    exact heap recovery (`agent/esp32-full-native-asset-pack`, PR #14).
16. Native on-demand bitshape source model: 112 unique menu shapes across 284
    sprite references, zero resident `shapeData`, <=32-byte mask-column scratch,
    exact heap recovery (`agent/esp32-native-bitshape-loader`, PR #15, merge
    commit `76898e7990481cf630b293477958fe0c478f7f1a`).
17. Native sprite-texel random access: exact 143,990-byte selected sprite dataset
    measured without loading it, largest real menu sprite payload only 1,600 B,
    direct `stexels.bin` read and exact heap recovery
    (`agent/esp32-native-sprite-texel-probe`, PR #16, merge commit
    `6252dcf7f6cdb782bbf554b7ebeae9b296086d25`).
18. First complete ESP32-native sprite render: sprite 172 loaded from
    `bitshapes.bin` + `stexels.bin`, native palette normalization, 3,199 pixels
    rasterized into the shared framebuffer with `shapeData == NULL` and
    `mediaTexels == NULL` (`agent/esp32-native-sprite-render-consumer`, PR #17,
    merge commit `bf59ee37b7242d0917ae0e4b49a83265c0b4f652`).
19. First complete ESP32-native wall render: texture 112 read directly from
    `wtexels.bin`, 4,096 texels rasterized with the mapped palette, exact heap
    recovery and deterministic framebuffer output
    (`agent/esp32-native-wall-render-consumer`, PR #18, merge commit
    `fa957f7aca0fe315d243156600622cf9ee115277`).
20. First shared ESP32-native graphics resource manager: sprite + wall loaders
    consolidated behind one bounded GFXRM API, rasterizers stripped of SD/pack
    layout knowledge, zero persistent cache allocation
    (`agent/esp32-native-graphics-resource-manager`, PR #19, merge commit
    `02b61492f216c1ad3b0fed18bf01ddfc22b768d9`).
21. First projected wall through the original projection/span pipeline with one
    bounded 2,048-byte GFXRM frame temporarily exposed through the legacy
    `mediaTexels` field. This proved the original projected wall semantics work
    without recreating the 172 KB wall pool (`agent/esp32-projected-wall-gfxrm`,
    PR #20, merge commit `1ec77feac2a04b1297661c3f8bb78504aec81938`).

## Current validated increment

Branch: `agent/esp32-projected-wall-native-span-source`

Status: **HARDWARE VALIDATED, READY TO MERGE**.

Objective: remove the temporary projected-wall compatibility alias completely.
A projected wall must now consume the bounded GFXRM frame directly while:

```text
render->mediaTexels == NULL
render->mediaTexelOffsets[112 * 2] == 65536
```

for the entire projected draw.

The output must remain bit-identical to the previously hardware-validated
compatibility path.

## Native projected-wall source boundary

The validated path is now:

```text
DoomRPG-ESP32.pak
        |
        v
      GFXRM
        |
        v
EspNativeWallFrame (2,048 B packed 4 bpp)
        |
        v
original transform / clip / projection helpers
        |
        v
ESP32-native wall span geometry + direct frame sampling
        |
        v
canonical RGB565 palette
        |
        v
shared 160x120 framebuffer
```

The native wall-span implementation preserves the fixed-point branch used by the
reference renderer (`FIXED_VERSION=1`) and the original wall-column interpolation
semantics, but samples the active bounded GFXRM frame directly.

The previous transition operations are gone:

```text
NO render->mediaTexels alias
NO mediaTexelOffsets[] rebase
NO map-wide wall texel pool
```

## Authoritative hardware validation

Hardware result:

```text
=== Doom RPG ESP32 projected wall native span source ===
[PROJWALL] Begin heap8=30576 largest8=22516 viewport=160x80@0,20 texture=112 shapeData=0x0 mediaTexels=0x0 mappingOffset=65536
[GFXRM] WALL id=112 storage=2048B hash=92d40704 pack=closed
[PROJWALL] ACQUIRE texture=112 palette=480 sourceOffset=65536 packed=2048B hash=92d40704 mediaTexels=0x0 mappingOffset=65536 pack=closed
[PROJWALL] NATIVE source active; no mediaTexels alias and no mapping rewrite
[PROJWALL] Native frame heap8=28512 largest8=20468 used=2064B logicalBound=2048B mediaTexels=0x0 mappingOffset=65536
[PROJWALL] WORLD v1=(128,-32,z0) v2=(128,32,z64) camera=(0,0,z32) spanMode=0 sentinel=a55a
[PROJWALL] -> original transform -> clip -> project -> ESP32-native wall spans
[PROJWALL] RELEASE texture=112 spans=40 pixels=1600 rangeErrors=0 legacyPtrViolations=0 mappingViolations=0 mediaTexels=0x0
[PROJWALL] PROJECTED columns=60..100 count=40 scale=81920/81920 z=0/5242880 lineRasterCount=1 changedPixels=1600
[PROJWALL] Native span stats begin=1 end=1 bound=2048B spans=40 pixels=1600 rangeErrors=0 legacyPtrViolations=0 mappingViolations=0
[PROJWALL] Source texture=112 palette=480 sourceOffset=65536 texelHash=92d40704
[PROJWALL] GFXRM stats spriteLoads=0 wallLoads=1 packOpenCycles=1 logicalBytes=2048 peakFrame=2048
[PROJWALL] framebufferFNV=ad191f54 expected=ad191f54 mediaTexelsDuring=0x0 mappingOffsetDuring=65536 mediaTexelsAfter=0x0 mappingOffsetAfter=65536
[PROJWALL] End heap8=30576 largest8=22516 deltaFromStart=0 residentLargestDelta=2048
[PROJWALL] READY projected wall is bit-identical with direct bounded GFXRM sampling
[PROJWALL] READY mediaTexels stayed NULL and global mapping stayed untouched for every native span
[MAPSTRUCT] Projected wall bridge validated; full map texel loading remains blocked
[MENUBSP] READY menu.bsp plan + real runtime structures validated
[ALIVE] uptime=5003 ms heap=96392 heap8=30576 largest8=22516 ... MENUBSP=ready ...
```

## Deterministic regression contract

The native direct-source path must preserve all of these values:

```text
texture index             = 112
texture source FNV        = 92d40704
palette offset            = 480
source texel offset       = 65536
projected columns         = 60..100
projected column count    = 40
changed framebuffer px    = 1600
native span calls         = 40
native span pixels        = 1600
range errors              = 0
legacy pointer violations = 0
mapping violations        = 0
projected framebuffer FNV = ad191f54
GFXRM wallLoads           = 1
GFXRM packOpenCycles      = 1
GFXRM logicalBytes        = 2048
GFXRM peakFrame           = 2048
```

`ad191f54` is especially important: it is exactly the framebuffer signature
from the previous compatibility-bridge implementation. The direct native source
therefore reproduces the same projected wall bit-for-bit while deleting its last
`mediaTexels` dependency.

## Memory boundary

Current branch baseline:

```text
heap8=30576
largest8=22516
shapeData=0x0
mediaTexels=0x0
```

While the 2,048-byte frame is resident:

```text
heap8=28512
largest8=20468
allocator cost=2064 B
resident largest-block delta=2048 B
```

After release:

```text
heap8=30576
largest8=22516
deltaFromStart=0
```

The 8-byte baseline change from the previous branch is diagnostic/static state,
not a per-frame leak. The allocation itself still costs the same validated
2,064 bytes and releases exactly.

As established by the previous increment, the largest free block may shrink
while a frame is resident depending on allocator placement. The contract is
exact restoration after release.

## Architectural conclusion

For mode-0 projected walls, the old map-wide graphics memory model is now gone
from the active path:

```text
projection math         -> preserved/reference semantics
wall texture storage    -> GFXRM bounded frame
wall texel sampling     -> ESP32-native direct source
mediaTexels             -> NULL throughout
mapping offset mutation -> none
```

This is the first projected-renderer primitive that is genuinely native at the
resource-consumption boundary rather than merely using a bounded compatibility
alias.

`src/Render.c` remains untouched in this increment. DoomRPG-RE continues to act
as the reference implementation while the ESP32 renderer is extracted one
primitive at a time.

## Current safe stop boundary

Validated and executed:

- complete engine startup through mappings
- real `Render_beginLoadMap(MAP_MENU)`
- real structural `Render_beginLoadMapData()` phase
- real menu nodes, lines, sprites, events, bytecodes and resource reference lists
- full 241-entry native asset pack validation
- zero resident `shapeData`
- zero map-wide `mediaTexels`
- canonical RGB565 palette normalization
- bounded native sprite + wall frames
- shared GFXRM backend
- native sprite/wall diagnostic rasterizers
- original transform/clip/project helpers for a deterministic projected wall
- ESP32-native mode-0 projected wall-span geometry
- direct sampling of one bounded 2,048-byte wall frame
- `mediaTexels == NULL` for every native projected span
- global wall mapping left unchanged for every native projected span
- bit-identical projected framebuffer signature `ad191f54`
- exact heap/largest-block restoration after release

Still intentionally NOT integrated:

- original `Render_loadBitShapes()`
- original `Render_loadTexels()`
- monolithic `shapeData`
- monolithic/map-wide `mediaTexels`
- full menu BSP wall pass using the native projected wall primitive
- projected sprite replacement in the real scene
- floor/ceiling native texture consumption
- cache hit/miss/eviction policy
- persistent open native pack
- completion of map graphics loading
- game entities/player spawning
- main game loop

## Recommended next increment after merge

Use the now-proven native projected wall primitive on **real menu map wall data**
instead of the synthetic deterministic test line.

Keep the scope narrow:

- reuse the real menu `Line_t` / map texture mappings already resident
- render a controlled real wall or small walls-only subset from `menu.bsp`
- keep sprites disabled
- keep floor/ceiling simple or disabled for this proof
- acquire required wall frames through GFXRM
- keep `shapeData == NULL` and `mediaTexels == NULL`
- measure how many wall texture requests occur and how often the same texture is
  requested before choosing any cache policy
- preserve exact heap restoration

This will turn the native projected primitive into the first real scene fragment
and provide the first useful access-pattern data for a future tiny cache.

## Increment discipline

- Start each new increment from the latest hardware-validated `main`.
- One branch = one small testable objective.
- Fix failures on the same branch; do not create the next branch early.
- Hardware validation on the real CYD is the merge gate.
- **Documentation is part of the increment:** after hardware success, update all
  relevant `.md` files on the same branch before merge.
- Do not say a branch is merge-ready until code + hardware proof + documentation
  are all present on that same branch.
- Keep `ESP32/README.md` updated with commands, SD preparation, conventions and
  architecture decisions so operational knowledge is not left only in chat.
