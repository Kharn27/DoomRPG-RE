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
- no monolithic map-wide `mediaTexels`
- shared 160x120 RGB565 framebuffer
- canonical RGB565 palette convention for native consumers
- storage access isolated from rasterizers
- original game behaviour/data preserved where practical

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

## Current validated increment

Branch: `agent/esp32-projected-wall-gfxrm`

Status: **HARDWARE VALIDATED, READY TO MERGE**.

Objective: prove that the original projected wall pipeline can consume a bounded
GFXRM wall frame without recreating the legacy 172 KB wall texel pool.

This increment deliberately keeps the original projection and wall span code
unchanged:

```text
Render_drawLines
  -> Render_transform2DVerts
  -> Render_clipLine
  -> Render_projectVertex
  -> Render_drawWallSpans
  -> Render_getSpanMode
  -> Render_SpanMode0
  -> shared framebuffer
```

The only transition bridge is the texel source.

## Projected-wall compatibility bridge

GFXRM loads one real 64x64 wall texture (texture 112) as the already validated
2,048-byte packed frame. For one draw only, the bridge presents that bounded
frame through the legacy `mediaTexels` field so the untouched original
`Render_SpanMode0()` can sample it.

The matching source mapping is temporarily rebased from the original global
logical texel offset to the local bounded frame:

```text
mediaTexelOffsets[112 * 2] : 65536 -> 0
mediaTexels                : NULL  -> 2,048-byte GFXRM frame
```

Immediately after the draw:

```text
mediaTexelOffsets[112 * 2] : 0 -> 65536
mediaTexels                : bounded frame -> NULL
```

This is explicitly a compatibility bridge, not a return to the legacy memory
architecture. There is never a map-wide `mediaTexels` allocation in this path.

## Authoritative hardware validation

Hardware result:

```text
=== Doom RPG ESP32 projected wall via GFXRM ===
[PROJWALL] Begin heap8=30584 largest8=22516 viewport=160x80@0,20 texture=112 shapeData=0x0 mediaTexels=0x0
[GFXRM] WALL id=112 storage=2048B hash=92d40704 pack=closed
[PROJWALL] BIND texture=112 palette=480 sourceOffset=65536 -> localOffset=0 boundedMediaTexels=2048B hash=92d40704 pack=closed
[PROJWALL] COMPAT legacy mediaTexels field temporarily aliases one bounded wall frame only
[PROJWALL] Bound frame heap8=28520 largest8=20468 used=2064B mediaTexels=0x3fffa4b4 logicalBound=2048B
[PROJWALL] WORLD v1=(128,-32,z0) v2=(128,32,z64) camera=(0,0,z32) spanMode=0 sentinel=a55a
[PROJWALL] -> unchanged Render_drawLines -> transform -> clip -> project -> Render_drawWallSpans -> Render_SpanMode0
[PROJWALL] UNBIND texture=112 restoredOffset=65536 mediaTexels=0x0
[PROJWALL] PROJECTED columns=60..100 count=40 scale=81920/81920 z=0/5242880 lineRasterCount=1 changedPixels=1600
[PROJWALL] Bridge stats begin=1 end=1 bound=2048B texture=112 palette=480 originalOffset=65536 texelHash=92d40704
[PROJWALL] GFXRM stats spriteLoads=0 wallLoads=1 packOpenCycles=1 logicalBytes=2048 peakFrame=2048
[PROJWALL] framebufferFNV=ad191f54 mediaTexelsRestored=0x0 mappingOffsetRestored=65536
[PROJWALL] End heap8=30584 largest8=22516 deltaFromStart=0
[PROJWALL] Resident largest-block delta=2048B is allocator-placement dependent; final restoration is the contract
[PROJWALL] READY unchanged projection + Render_drawWallSpans + Render_SpanMode0 consumed one bounded GFXRM frame
[PROJWALL] READY legacy mediaTexels alias existed only during draw and is restored to NULL
[MAPSTRUCT] Projected wall bridge validated; full map texel loading remains blocked
[MENUBSP] READY menu.bsp plan + real runtime structures validated
[ALIVE] uptime=5004 ms heap=96400 heap8=30584 largest8=22516 ... MENUBSP=ready ...
```

Validated deterministic markers:

```text
texture                 = 112
source texel FNV         = 92d40704
palette offset           = 480
projected columns        = 60..100
projected column count   = 40
changed framebuffer px   = 1600
projected framebuffer FNV= ad191f54
GFXRM wallLoads          = 1
GFXRM packOpenCycles     = 1
GFXRM logicalBytes       = 2048
GFXRM peakFrame          = 2048
```

This is the first wall rendered by the original projection/span geometry while
its texture bytes come from the bounded ESP32 resource system.

## Memory boundary

Current branch baseline:

```text
heap8=30584
largest8=22516
shapeData=0x0
mediaTexels=0x0
```

While the 2,048-byte wall frame is resident:

```text
heap8=28520
largest8=20468
allocator cost=2064 B
```

After release:

```text
heap8=30584
largest8=22516
deltaFromStart=0
```

### Largest-block allocator lesson

The first hardware run produced a false probe failure because the test assumed
that `largest8` must remain unchanged while a small frame is resident. On this
build the allocator placed the 2,064-byte allocation inside the largest free
region, so the largest block temporarily changed:

```text
22516 -> 20468 -> 22516
```

That is valid allocator behaviour, not fragmentation or a leak. The hardware
contract is **exact restoration after release**, not preservation of the largest
block during residency. The probe was corrected on the same branch.

## Architectural conclusion

The wall path has now crossed from asset-viewer diagnostics into the actual
projected renderer:

```text
DoomRPG-ESP32.pak
        |
        v
      GFXRM
        |
  2,048 B wall frame
        |
        v
compatibility bind (temporary only)
        |
        v
unchanged original wall projection / SpanMode0
        |
        v
shared RGB565 framebuffer
```

Important nuance:

- `shapeData` remains `NULL` throughout.
- `mediaTexels` starts `NULL` and ends `NULL`.
- during one draw, `mediaTexels` temporarily aliases only one bounded 2,048-byte
  wall frame.
- the legacy map-wide `mediaTexels` pool is still never created.

This bridge is intentionally transitional. Its value is proving that the
original geometry/raster math works with a tiny on-demand texture working set.

## Current safe stop boundary

Validated and executed:

- complete engine startup through mappings
- real `Render_beginLoadMap(MAP_MENU)`
- real structural `Render_beginLoadMapData()` phase
- real nodes, lines, sprites, events, bytecodes and resource reference lists
- full 241-entry native asset pack validation
- zero resident `shapeData`
- zero map-wide `mediaTexels`
- native RGB565 palette normalization
- bounded native sprite + wall frames
- shared GFXRM backend
- native sprite/wall diagnostic rasterizers
- original `Render_drawLines()` transform/clip/project path for a test wall
- original `Render_drawWallSpans()` wall-column geometry
- original `Render_SpanMode0()` sampling a bounded 2 KB GFXRM frame through a
  temporary compatibility alias
- deterministic projected framebuffer hash `ad191f54`
- exact heap and largest-block recovery after the projected wall draw

Still intentionally NOT executed / integrated:

- original `Render_loadBitShapes()`
- original `Render_loadTexels()`
- monolithic `shapeData`
- monolithic/map-wide `mediaTexels`
- full menu BSP wall rendering through GFXRM
- cache hit/miss/eviction policy
- persistent open native pack
- native replacement for the temporary `mediaTexels` compatibility alias
- replacement of the real `Render_renderSprite()` call path
- full floor/ceiling native texture consumption
- completion of map graphics loading
- game entities/player spawning
- main game loop

## Recommended next increment after merge

Remove the temporary `mediaTexels` alias from the projected wall path while
preserving the now-proven projection/span semantics.

A good next step is to introduce a small ESP32-native wall span sampler that:

- receives the current bounded GFXRM wall frame explicitly
- consumes the same `param_4 / param_5 / param_6` span coordinates as
  `Render_SpanMode0()`
- reproduces the exact 4-bit nibble selection and palette lookup
- leaves `Render_drawWallSpans()` projection/math untouched
- keeps `render->mediaTexels == NULL` even during the draw
- reproduces the hardware reference scene (`40` columns, `1600` pixels) and a
  deterministic framebuffer signature

Only after that native source boundary is proven should the project attempt a
full BSP wall pass or choose a real cache policy.

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
