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
- audio disabled/stubbed during bring-up

## Project direction

DoomRPG-RE is treated as an executable specification for behaviour, formats and
rendering semantics, not as an architecture contract. The ESP32 implementation
is progressively becoming its own engine specialized for a classic no-PSRAM
ESP32:

- bounded deterministic RAM use
- SD-backed immutable resources
- small measured working sets / caches
- no whole-resource graphics inflation
- no monolithic `shapeData`
- no monolithic `mediaTexels`
- shared 160x120 RGB565 framebuffer
- canonical RGB565 palette convention for native consumers
- storage access isolated from rasterizers
- original game data and behaviour preserved where practical

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
    recovery and deterministic framebuffer output (`agent/esp32-native-wall-render-consumer`,
    PR #18, merge commit `fa957f7aca0fe315d243156600622cf9ee115277`).

## Current validated increment

Branch: `agent/esp32-native-graphics-resource-manager`

Status: **HARDWARE VALIDATED, READY TO MERGE**.

Objective: remove duplicated SD/asset-pack/file-format loading logic from the
native sprite and wall rasterizers and introduce the first shared ESP32-native
graphics resource manager (`GFXRM`). The manager owns bounded frame loading and
release; rasterizers only consume already-loaded frames.

No cache slot count or eviction policy is introduced in this increment.

## Native graphics resource manager contract

The shared manager exposes bounded frame APIs for both currently validated
resource classes:

```text
EspNativeGraphics_loadSpriteFrame(render, spriteIndex, &frame)
EspNativeGraphics_releaseSpriteFrame(&frame)

EspNativeGraphics_loadWallFrame(render, textureIndex, &frame)
EspNativeGraphics_releaseWallFrame(&frame)
```

The rasterizers no longer know the on-disk layout of:

```text
bitshapes.bin
stexels.bin
wtexels.bin
```

and no longer open or search `DoomRPG-ESP32.pak` themselves.

Current architecture:

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

The first manager deliberately opens the native pack only for a bounded load and
closes it before returning the frame. Keeping the pack open permanently is not
yet justified because the validated open cost is about 4,376 bytes of free heap.

## Authoritative hardware validation

The sprite path still produces exactly the previously validated resource and
framebuffer signatures through the new shared backend:

```text
[SPRITERENDER] Begin sprite=172 heap8=30664 largest8=21492 shapeData=0x0 mediaTexels=0x0 backend=GFXRM
[GFXRM] SPRITE id=172 storage=2112B mask=512B texels=1600B hash=0c0a7acd pack=closed
[SPRITERENDER] Frame resident after manager load heap8=28536 largest8=21492 used=2128B logicalStorage=2112B
[SPRITERENDER] DRAW sprite=172 origin=48,28 drawn=3199 paletteOffset=1616 framebufferFNV=001910a9
[SPRITERENDER] Released manager frame heap8=30664 largest8=21492 deltaFromStart=0
[SPRITERENDER] READY real sprite rendered through shared GFXRM backend
[SPRITERENDER] Rasterizer no longer owns SD/pack/bitshape/stexels loading logic
```

The wall path also retains exactly the previous source and framebuffer
signatures:

```text
[WALLRENDER] Begin texture=112 heap8=30664 largest8=21492 shapeData=0x0 mediaTexels=0x0 backend=GFXRM
[GFXRM] WALL id=112 storage=2048B hash=92d40704 pack=closed
[WALLRENDER] Frame resident after manager load heap8=28600 largest8=21492 used=2064B logicalStorage=2048B
[WALLRENDER] DRAW texture=112 origin=48,28 drawn=4096 paletteOffset=480 framebufferFNV=e39af2c4
[WALLRENDER] Released manager frame heap8=30664 largest8=21492 deltaFromStart=0
[WALLRENDER] READY real wall texture rendered through shared GFXRM backend
[WALLRENDER] Rasterizer no longer owns SD/pack/wtexels loading logic
```

The unchanged hashes prove that the refactor preserved both source bytes and
visible output:

```text
sprite 172 texel FNV       = 0c0a7acd
sprite diagnostic FNV      = 001910a9
wall 112 texel FNV         = 92d40704
wall diagnostic FNV        = e39af2c4
```

## Shared-manager session statistics

Hardware result for one sprite load followed by one wall load:

```text
[GFXRM] Session stats spriteLoads=1 wallLoads=1 packOpenCycles=2 logicalBytes=4160 peakFrame=2112
[GFXRM] READY one shared backend served sprite + wall with zero persistent cache allocation
[MAPSTRUCT] Native graphics resource manager validated; full map texel loading remains blocked
```

The values are exactly the expected contract:

```text
spriteLoads       = 1
wallLoads         = 1
packOpenCycles    = 2
logicalBytes      = 2112 + 2048 = 4160 B
peakFrame         = 2112 B
persistent cache  = 0 B
```

These counters are static diagnostic state only; they do not allocate a runtime
cache.

## Memory boundary and the 24-byte baseline shift

The previous wall-render build returned to:

```text
heap8=30688
largest8=21492
```

The manager build starts and returns to:

```text
heap8=30664
largest8=21492
```

That is a stable 24-byte reduction in free 8-bit heap, not a per-load leak. The
manager adds five persistent `uint32_t` counters (20 bytes), and the observed
24-byte change is consistent with that static state plus alignment/accounting.
The hardware proof that matters is that every manager frame returns exactly to
the same new baseline and the largest free block remains unchanged.

Sprite frame residency:

```text
before       heap8=30664 largest8=21492
resident     heap8=28536 largest8=21492
released     heap8=30664 largest8=21492 deltaFromStart=0
```

Wall frame residency:

```text
before       heap8=30664 largest8=21492
resident     heap8=28600 largest8=21492
released     heap8=30664 largest8=21492 deltaFromStart=0
```

Heartbeat remains healthy:

```text
[ALIVE] uptime=5000 ms heap=96480 heap8=30664 largest8=21492 ... MENUBSP=ready ...
```

## Architectural conclusion

The graphics system now has a real separation of responsibilities:

```text
storage/file-format knowledge -> GFXRM
pixel sampling/rasterization  -> native rasterizers
presentation                  -> shared framebuffer / platform video
```

This is the first reusable graphics-resource boundary of our ESP32 engine rather
than another independent probe.

Still forbidden and absent:

```text
shapeData   = NULL
mediaTexels = NULL
```

The manager is intentionally **not yet a cache**. Runtime cache design must be
based on real projected-renderer access patterns instead of choosing arbitrary
slot counts.

## Current safe stop boundary

Validated and executed:

- complete engine startup through mappings
- real `Render_beginLoadMap(MAP_MENU)`
- real structural `Render_beginLoadMapData()` phase
- real nodes, lines, sprites, events, bytecodes and resource reference lists
- full 241-entry native asset pack validation
- zero resident `shapeData`
- zero resident `mediaTexels`
- on-demand bitshape source model
- canonical RGB565 palette normalization
- bounded native sprite frame and rasterizer
- bounded native wall frame and rasterizer
- one shared GFXRM backend for sprite + wall frame loading/release
- sprite/wall rasterizers free of SD/pack/file-layout knowledge
- exact preservation of all validated source/framebuffer hashes
- exact heap recovery after both manager loads
- zero persistent graphics cache allocation

Still intentionally NOT executed / integrated:

- original `Render_loadBitShapes()`
- original `Render_loadTexels()`
- monolithic `shapeData`
- monolithic `mediaTexels`
- cache hit/miss/eviction policy
- persistent open native pack
- replacement of the real projected wall-span call path
- replacement of the real `Render_renderSprite()` call path
- full floor/ceiling native texture consumption
- completion of map graphics loading
- game entities/player spawning
- main game loop

## Recommended next increment after merge

Use GFXRM from the first **real projected renderer path**, rather than adding more
diagnostic-only resource types.

A good next target is one existing projected wall-span path:

- preserve the original projection/math and wall-column semantics
- acquire the required packed wall frame through GFXRM
- sample native wall texels without `mediaTexels`
- render a real projected wall into the shared framebuffer
- measure request counts and repeated texture access
- keep cache policy disabled until those measurements exist

This should begin replacing the old renderer at the point where it actually
consumes graphics, while keeping DoomRPG-RE as the behavioural reference.

## Increment discipline

- Start each new increment from the latest hardware-validated `main`.
- One branch = one small testable objective.
- Fix failures on the same branch; do not create the next branch early.
- Hardware validation on the real CYD is the merge gate.
- **Documentation is part of the increment:** after hardware success, update all
  relevant `.md` files on the same branch before merge.
- A branch is not considered complete/merge-ready while its code and recovery
  documentation disagree.
- Keep `ESP32/README.md` updated with commands, SD preparation, conventions and
  architecture decisions so operational knowledge is not left only in chat.
