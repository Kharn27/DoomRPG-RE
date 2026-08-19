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
- gameplay viewport: 160x80 at framebuffer y=20
- physical output: exact nearest-neighbour 2x to 320x240
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
12. Real `Render_beginLoadMap(MAP_MENU)` + structural portion of
    `Render_beginLoadMapData()`, stopped exactly before graphics resources
    (`agent/esp32-menu-map-runtime-structures`, PR #11).
13. Graphics-resource memory plan proving the original monolithic graphics path
    cannot fit on the no-PSRAM CYD (`agent/esp32-menu-resource-memory-plan`, PR #12).
14. First ESP32-native asset-pack primitive: random access to a real 2,048-byte
    menu wall texture (`agent/esp32-native-asset-pack`, PR #13).
15. Full ESP32-native asset pack v2: all 241 ZIP resources mirrored uncompressed
    behind a hash-sorted on-disk index (`agent/esp32-full-native-asset-pack`, PR #14).
16. Native on-demand bitshape source model: 112 unique menu shapes across 284
    sprite references, zero resident `shapeData` (`agent/esp32-native-bitshape-loader`,
    PR #15, merge `76898e7990481cf630b293477958fe0c478f7f1a`).
17. Native sprite-texel random access: 143,990 B selected dataset measured without
    loading it, largest selected payload 1,600 B (`agent/esp32-native-sprite-texel-probe`,
    PR #16, merge `6252dcf7f6cdb782bbf554b7ebeae9b296086d25`).
18. First complete native sprite render: sprite 172 from `bitshapes.bin` +
    `stexels.bin`, 3,199 pixels, `shapeData == NULL`, `mediaTexels == NULL`
    (`agent/esp32-native-sprite-render-consumer`, PR #17, merge
    `bf59ee37b7242d0917ae0e4b49a83265c0b4f652`).
19. First complete native wall render: texture 112 from `wtexels.bin`, 4,096
    texels, deterministic framebuffer (`agent/esp32-native-wall-render-consumer`,
    PR #18, merge `fa957f7aca0fe315d243156600622cf9ee115277`).
20. Shared native graphics resource manager: sprite + wall loads consolidated
    behind bounded GFXRM (`agent/esp32-native-graphics-resource-manager`, PR #19,
    merge `02b61492f216c1ad3b0fed18bf01ddfc22b768d9`).
21. First projected wall using original transform/clip/project/span semantics and
    a bounded 2 KB GFXRM frame through a temporary compatibility alias
    (`agent/esp32-projected-wall-gfxrm`, PR #20, merge
    `1ec77feac2a04b1297661c3f8bb78504aec81938`).
22. Projected wall converted to direct bounded native span sampling: output stayed
    bit-identical (`ad191f54`) while `mediaTexels` remained NULL and global
    mappings remained untouched for every span
    (`agent/esp32-projected-wall-native-span-source`, PR #21, merge
    `421994bc08a7ecbd56540b723771973c766a5f46`).

## Current validated increment

Branch: `agent/esp32-menu-walls-native-frame`

Status: **HARDWARE VALIDATED, DOCUMENTED, READY TO MERGE**.

Objective: stop rendering synthetic test geometry and render a real deterministic
walls-only frame from the actual `menu.bsp` using:

- the menu BSP header spawn position and direction
- the original camera transform
- original BSP walk / culling / leaf ordering / occlusion
- real map `Line_t` objects and texture mappings
- direct native projected wall spans
- one bounded GFXRM wall frame at a time

Sprites and textured floor/ceiling are intentionally disabled for this increment.
The visible background is solid floor/ceiling color using the menu grayscale
palette convention.

## Real menu camera

The original menu loader derives its camera from `mapSpawnIndex` and
`mapSpawnDir`. The hardware-validated `menu.bsp` header contains:

```text
mapSpawnIndex = 460
tile           = 12,14
world X/Y      = 800,928
mapSpawnDir    = 0
```

World coordinates follow the original formula:

```text
viewX = ((mapSpawnIndex % 32) << 6) + 32
viewY = ((mapSpawnIndex / 32) << 6) + 32
```

For this first deterministic real scene the camera height is fixed at `36`, the
normal eye-height convention used by the engine. X/Y/direction come directly
from `menu.bsp`; only Z is an explicitly documented convention in this probe.

## Real menu walls-only path

```text
menu.bsp real runtime structures
        |
        +--> mapSpawnIndex/mapSpawnDir
        |
        v
original Render_render camera setup
        |
        v
original Render_walkNode
  (BSP traversal, culling, leaf order, occlusion)
        |
        v
real visible map Line_t objects
        |
        v
original mediaTexturesIds mapping semantics
        |
        v
GFXRM: one 2,048-byte wall frame per request
        |
        v
ESP32-native projected wall spans
        |
        v
shared 160x120 RGB565 framebuffer
```

Hard invariants remain:

```text
shapeData   = NULL
mediaTexels = NULL
```

No wall mapping is rewritten.

## Authoritative hardware validation

Key hardware output:

```text
=== Doom RPG ESP32 real menu.bsp walls-only frame ===
[MENUWALL] Begin heap8=30576 largest8=22516 shapeData=0x0 mediaTexels=0x0
[MENUWALL] BSP header spawnIndex=460 tile=12,14 world=800,928 dir=0 cameraZ=36
...
[MENUWALL] BSP visibility nodeCount=28 nodeRasterCount=10 visibleLeaves=10 sourceLines=46
[MENUWALL] Line result walls=25 backface=15 clipped=6 spriteSpanSkipped=0 occluderOnly=0
[MENUWALL] Texture requests total=25 unique=8 repeats=17 trackingErrors=0 requestFNV=4db9da28 animFrameTime=0
[MENUWALL] GFXRM wallLoads=25 packOpenCycles=25 logicalBytes=51200 expected=51200 peakFrame=2048
[MENUWALL] Native spans begin=25 end=25 spanCalls=224 pixels=5589 rangeErrors=0 legacyPtrViolations=0 mappingViolations=0
[MENUWALL] framebufferFNV=a6d87c4a renderMs=1676 floor=4208 ceiling=8c51 mediaTexels=0x0
[MENUWALL] End heap8=30576 largest8=22516 deltaFromStart=0
[MENUWALL] READY real menu spawn + original BSP visibility + native GFXRM walls rendered together
[MENUWALL] READY no cache yet; request/repeat counts are the input for the next cache decision
[ALIVE] ... heap8=30576 largest8=22516 ... MENUBSP=ready ...
```

The hardware photo shows an actual grayscale 3D menu scene with multiple walls,
perspective and texture variation. It is intentionally incomplete: no projected
sprites, no textured floor/ceiling and no final menu UI overlay yet.

## Deterministic real-scene regression markers

```text
spawnIndex                = 460
spawn tile                = 12,14
spawn world               = 800,928
spawn direction           = 0
camera Z                  = 36
BSP nodeCount             = 28
BSP nodeRasterCount       = 10
visible leaves            = 10
source lines              = 46
wall requests             = 25
backface culled           = 15
clip culled               = 6
unique wall textures      = 8
repeated wall requests    = 17
texture request FNV       = 4db9da28
native span calls         = 224
native pixels drawn       = 5589
range errors              = 0
legacy pointer violations = 0
mapping violations        = 0
GFXRM wall loads          = 25
GFXRM pack open cycles    = 25
GFXRM logical bytes       = 51200
GFXRM peak frame          = 2048
framebuffer FNV           = a6d87c4a
render time               = 1676 ms
floor RGB565              = 4208
ceiling RGB565            = 8c51
```

The animation phase is deliberately frozen (`animFrameTime=0`) so this first
real-scene framebuffer and request sequence stay deterministic.

## Exact texture request sequence

The first real menu frame requests these 25 wall frames, in order:

```text
116, 32, 40, 112, 108, 108, 116, 116, 116, 112,
116, 112, 108, 108, 108, 116, 44, 0, 40, 152,
152, 116, 116, 116, 152
```

Unique textures:

```text
0, 32, 40, 44, 108, 112, 116, 152
```

This sequence is part of the recovery data because it allows future cache
policies to be evaluated without repeating the hardware discovery step.

## Cache analysis derived from the validated sequence

This is analysis, **not yet a hardware-validated cache implementation**.
Applying a simple LRU simulation to the 25-request sequence gives:

```text
LRU slots   hits   misses   resident payload
1             8      17       2,048 B
2            11      14       4,096 B
3            14      11       6,144 B
4            14      11       8,192 B
5            16       9      10,240 B
6            17       8      12,288 B
7            17       8      14,336 B
8            17       8      16,384 B
```

Important consequence: on this reference frame, a **3-slot LRU** captures 14 of
17 possible repeat hits while a fourth slot adds no hits. That makes 3 slots a
strong candidate for the first measured wall cache experiment on a no-PSRAM
CYD, subject to real hardware memory/timing validation.

## Memory boundary

Before the real frame:

```text
heap8=30576
largest8=22516
```

GFXRM still acquires only one 2,048-byte wall payload at a time. After all 25
requests and releases:

```text
heap8=30576
largest8=22516
deltaFromStart=0
```

There is no persistent wall cache yet and no leak/fragmentation visible at the
validated end boundary.

## Architectural conclusion

This increment is the first real rendered map scene on the target. The project
has moved from isolated graphics proofs to real level geometry:

```text
real BSP + real camera + real walls + real texture mappings
                     |
                     v
          bounded native resources
                     |
                     v
              actual scene image
```

The frame is not yet the complete menu. Missing pieces are deliberate and now
well isolated:

1. projected real-scene sprites
2. floor/ceiling texture sampling
3. menu UI/logo/text overlay and normal state-machine presentation
4. resource reuse/cache policy

The lack of a Cacodemon in this frame is expected: the whole projected-sprite
path is intentionally disabled in this walls-only increment.

## Current safe stop boundary

Validated and executed:

- complete engine startup through mappings
- real `Render_beginLoadMap(MAP_MENU)` structural map load
- 53 nodes, 120 lines, 44 map sprites, 68 runtime sprites, 15 events
- full native asset-pack access
- zero resident `shapeData`
- zero map-wide `mediaTexels`
- canonical RGB565 palette normalization
- shared GFXRM backend
- native sprite diagnostic rendering
- native direct projected wall sampling
- original BSP traversal/culling/occlusion on real `menu.bsp`
- real menu spawn camera X/Y/direction
- 25 real wall draws using 8 distinct textures
- 224 native spans / 5,589 wall pixels
- deterministic real-scene framebuffer `a6d87c4a`
- exact heap/largest-block restoration after all 25 uncached resource cycles

Still intentionally NOT integrated:

- original `Render_loadBitShapes()`
- original `Render_loadTexels()`
- monolithic `shapeData`
- monolithic/map-wide `mediaTexels`
- projected real-scene sprite rendering
- textured floor/ceiling rendering
- wall cache hit/miss/eviction implementation
- persistent open native pack
- complete map graphics loader replacement
- game entities/player spawning in the active gameplay loop
- final main-menu UI composition
- main gameplay loop

## Recommended next increment after merge

Do **not** jump straight to an 8-texture cache merely because eight unique wall
textures appear in this frame. The access sequence shows that three LRU slots
already capture most reuse while costing only about 6 KB of payload RAM.

Recommended next step: introduce a tiny **3-slot wall-frame LRU cache** inside or
behind GFXRM and rerun exactly this same deterministic menu frame. The acceptance
criteria should include:

- framebuffer remains `a6d87c4a`
- request sequence remains `4db9da28`
- 25 logical wall requests remain visible to diagnostics
- expected LRU result: 14 hits / 11 misses for three slots
- pack open cycles drop from 25 to 11
- physical wall bytes read drop from 51,200 B to 22,528 B
- peak/persistent heap impact is measured on real hardware
- exact cache cleanup/restoration is proven before merge

Only after that measured resource-reuse layer is stable should the next visual
piece (real projected sprites or textured planes) be added.

## Increment discipline

- Start each new increment from the latest hardware-validated `main`.
- One branch = one small testable objective.
- Fix failures on the same branch; do not create the next branch early.
- Hardware validation on the real CYD is the merge gate.
- **Documentation is part of the increment:** after hardware success, update all
  relevant `.md` files on the same branch before merge.
- Do not say a branch is merge-ready until code + hardware proof + documentation
  are all present on that same branch.
- Keep `ESP32/README.md` updated with commands, SD preparation, conventions,
  deterministic signatures and architecture decisions so another engineer can
  resume without access to conversation history.
