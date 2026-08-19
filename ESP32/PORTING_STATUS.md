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
- small measured working sets and evidence-driven caches
- no whole-resource graphics inflation
- no monolithic `shapeData`
- no monolithic/map-wide `mediaTexels`
- shared 160x120 RGB565 framebuffer
- canonical RGB565 palette convention for native consumers
- storage/file-format access isolated behind GFXRM
- cache/reuse policy isolated from storage and rasterization
- original projection/BSP/game semantics preserved where useful
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
23. First real `menu.bsp` walls-only scene: real spawn camera, original BSP
    traversal/culling/occlusion, 25 wall requests across 8 textures, 224 native
    spans and deterministic framebuffer `a6d87c4a`
    (`agent/esp32-menu-walls-native-frame`, PR #22, merge
    `0f30003f61e81bae20f6c5b81bda52cea3b8ff9e`).
24. Hardware-validated three-slot wall LRU on that exact real scene: 25 logical
    requests -> 11 physical loads, 14 hits / 11 misses / 8 evictions, 6,144 B
    logical cache payload, framebuffer still exactly `a6d87c4a`
    (`agent/esp32-menu-wall-lru-cache`, PR #23, merge
    `e39079253ef72c17a908c1c2633762d20fe5f29e`).

## Current validated increment

Branch: `agent/esp32-menu-native-sprites-frame`

Status: **HARDWARE VALIDATED, DOCUMENTED, READY TO MERGE**.

Objective: preserve the validated cached real `menu.bsp` wall frame, then render
the real BSP-sorted sprite objects visible from the same deterministic menu
camera using bounded GFXRM frames, without `shapeData`, without `mediaTexels` and
without a sprite cache yet.

This increment deliberately measures the real sprite request sequence before any
sprite-cache policy is chosen.

## Real menu camera and wall baseline

The deterministic scene remains:

```text
mapSpawnIndex = 460
spawn tile    = 12,14
world X/Y     = 800,928
mapSpawnDir   = 0
camera Z      = 36
animFrameTime = 0
```

The wall pass remains the PR #23 regression:

```text
wall framebuffer FNV = a6d87c4a
wall logical requests = 25
wall LRU              = 14 hits / 11 misses / 8 evictions
wall physical loads   = 11
```

The wall cache is completely torn down before the uncached sprite pass, so the
sprite measurements have a clean allocator/resource boundary.

## Native projected sprite architecture

The original BSP walk is still responsible for visibility and depth ordering.
`Render_walkNode()` constructs `render->viewSprites` in `sortZ` order. The ESP32
path consumes that list rather than scanning map sprites in arbitrary order.

```text
menu.bsp runtime sprites
        |
        v
original Render_walkNode
(BSP visibility + sortZ ordering)
        |
        v
viewSprites
        |
        v
original mediaSpriteIds / anim semantics
        |
        v
GFXRM sprite frame
(mask + packed active texels + palette offset)
        |
        v
ESP32-native projected sprite renderer
        |
        v
shared framebuffer already containing cached real walls
```

The legacy renderer expands bitshape masks into `shapeData` run tables. The
native renderer does not rebuild that monolithic representation. For each bounded
sprite frame it reconstructs opaque runs directly from the source mask and maps
them to the already packed active texel stream.

The renderer also handles the menu objects that use wall textures instead of
bitshape sprite frames by routing them through the existing bounded projected-wall
path.

## Hard graphics invariants

Throughout the real sprite pass:

```text
shapeData   = NULL
mediaTexels = NULL
```

The native sprite diagnostics prove:

```text
rangeErrors          = 0
legacyPtrViolations  = 0
shapeDataViolations  = 0
mappingViolations    = 0
unsupportedFlags     = 0
unsupportedModes     = 0
```

No global mapping is rewritten to make a local frame resemble a legacy pool.

## Authoritative hardware validation

Hardware output:

```text
=== Doom RPG ESP32 real menu.bsp walls + native sprites ===
[MENUSPRITE] Begin wallsFNV=a6d87c4a expected=a6d87c4a shapeData=0x0 mediaTexels=0x0
[MENUSPRITE] Baseline heap8=30176 largest8=22516 numMapSprites=44 runtimeSlots=68
[MENUSPRITE] View list objects=17 outOfRange=0 listFNV=962cd657 ordering=original-BSP-sortZ
...
[MENUSPRITE] Objects total=17 hidden=0 lightsSkipped=5 entityUnsupported=0 resolvedDraws=14
[MENUSPRITE] Requests spriteFrames=11 unique=8 repeats=3 requestFNV=4457ac94 wallBacked=2 maxFrame=2112B
[MENUSPRITE] Cull near=1 backface=0 clipped=1 spans=389 pixels=4590
[MENUSPRITE] Invariants rangeErrors=0 legacyPtrViolations=0 shapeDataViolations=0 mappingViolations=0 unsupportedFlags=0 unsupportedModes=0
[MENUSPRITE] GFXRM spriteLoads=11 wallLoads=2 packOpenCycles=13 logicalBytes=18784 peakFrame=2112
[MENUSPRITE] Wall-backed projected begin=2 end=2 spans=20 pixels=340 errors=0/0/0
[MENUSPRITE] framebufferFNV=ffe0995e wallsFNV=a6d87c4a changed=yes renderMs=1064 shapeData=0x0 mediaTexels=0x0
[MENUSPRITE] End heap8=30176 largest8=22516 deltaFromStart=0
[MENUSPRITE] READY real menu sprites rendered from bounded uncached GFXRM frames
[MENUSPRITE] READY request sequence measured before any sprite cache policy is chosen
[ALIVE] ... heap8=30176 largest8=22516 ... MENUBSP=ready ...
```

## Deterministic real sprite regression contract

```text
walls framebuffer FNV       = a6d87c4a
viewSprites count           = 17
viewSprites list FNV        = 962cd657
hidden objects              = 0
menu lights skipped         = 5
entity-linked unsupported   = 0
resolved draw attempts      = 14
sprite frame requests       = 11
unique sprite frames        = 8
repeated sprite requests    = 3
sprite request FNV          = 4457ac94
wall-backed object requests = 2
largest sprite frame        = 2,112 B
near culled                 = 1
backface culled             = 0
clip culled                 = 1
native sprite span calls    = 389
native sprite pixels        = 4,590
wall-backed spans           = 20
wall-backed pixels          = 340
range errors                = 0
legacy pointer violations   = 0
shapeData violations        = 0
mapping violations          = 0
unsupported flag paths      = 0
unsupported render modes    = 0
final framebuffer FNV       = ffe0995e
```

`renderMs=1064` was measured on the validation run. Treat timing as diagnostic,
not as a deterministic regression condition: SD and serial logging timing vary.

## Exact sprite request sequence

The 11 actual bitshape-backed sprite frame requests, in order, are:

```text
request  object  mediaId  mode
1        17      562      0
2        41      406      0
3        41      410      7
4        34      598      0
5        27      172      0
6        28      578      0
7        21      578      0
8        23      426      0
9        23      410      7
10       18      578      0
11       16      102      0
```

Sequence only:

```text
562, 406, 410, 598, 172, 578, 578, 426, 410, 578, 102
```

Unique sprite frame IDs:

```text
102, 172, 406, 410, 426, 562, 578, 598
```

The repeated resources are:

```text
578 requested 3 times
410 requested 2 times
```

Two additional visible objects are wall-backed rather than bitshape-backed:

```text
object 35 -> wall texture 152
object 30 -> wall texture 108
```

The request hash `4457ac94` includes object index, media ID and render mode and is
the compact deterministic regression marker for the ordered sprite request
stream.

## Measured sprite frame sizes

Logical GFXRM frame storage observed in the request stream:

```text
mediaId  storage
562       962 B
406       603 B
410       796 B
598     1,949 B
172     2,112 B
578     1,977 B
426       603 B
102       936 B
```

The 11 physical sprite loads total 14,688 B. The two wall-backed 2,048-byte loads
add 4,096 B, giving the measured GFXRM total:

```text
sprite loads      = 11
wall loads        = 2
pack open cycles  = 13
logical bytes     = 18,784 B
peak single frame = 2,112 B
```

## Sprite-cache analysis derived from hardware data

The following is **derived analysis only**. No sprite cache exists in this
increment.

Applying an LRU simulation to the exact 11-request sprite sequence gives:

```text
slots  hits  misses  evictions  measured-sequence peak logical resident bytes
1       1      10       9        2,112 B
2       1      10       8        4,089 B
3       2       9        6        6,038 B
4       2       9        5        6,834 B
5       3       8        3        7,437 B
6       3       8        2        8,399 B
7       3       8        1        9,002 B
8       3       8        0        9,938 B
```

Important consequences:

- three slots capture 2 of the 3 possible repeat hits with about 6 KB peak
  logical payload on this sequence;
- four slots add no hit compared with three;
- five slots are the smallest cache that captures all 3 repeat hits, with a
  measured-sequence peak logical payload of 7,437 B;
- adding more than five slots gives no additional hit on this deterministic
  frame.

Do not choose a sprite cache size merely because there are eight unique frames.
The next cache experiment should explicitly choose between the 3-slot RAM-saving
point and the 5-slot all-repeat point, then validate allocator cost and exact
framebuffer preservation on hardware.

## Memory boundary

Current branch baseline before the sprite pass:

```text
heap8=30176
largest8=22516
```

After all 11 sprite-frame loads/releases and both wall-backed objects:

```text
heap8=30176
largest8=22516
deltaFromStart=0
```

The baseline is 216 B below the PR #23 branch baseline (`30392`). This is static
state/code bookkeeping introduced by the projected-sprite probe/renderer, not a
per-frame leak. The real contract is exact restoration to the current baseline,
which hardware validates.

## Architectural conclusion

The target now renders a real deterministic menu scene with both native walls
and real BSP-sorted sprites while the two legacy graphics pools remain absent:

```text
real menu BSP
   |
   +--> wall visibility/order --> 3-slot wall LRU --> native wall spans
   |
   +--> viewSprites sortZ order --> uncached GFXRM sprite frames
                                      |
                                      v
                               native sprite spans
   |                                  |
   +----------------------------------+
                    |
                    v
          shared 160x120 framebuffer
                    |
                    v
            final FNV ffe0995e
```

This is the first hardware-validated full scene containing both real map walls
and real projected sprite objects on the no-PSRAM target.

## Current safe stop boundary

Validated and executed:

- complete engine startup through mappings
- real `Render_beginLoadMap(MAP_MENU)` structural map load
- 53 nodes, 120 lines, 44 map sprites, 68 runtime sprite slots, 15 events
- full native asset-pack access
- zero resident `shapeData`
- zero map-wide `mediaTexels`
- canonical RGB565 palette normalization
- shared GFXRM backend
- native direct projected wall sampling
- original BSP traversal/culling/occlusion on real `menu.bsp`
- real menu spawn camera X/Y/direction
- deterministic walls-only framebuffer `a6d87c4a`
- hardware-validated three-slot wall LRU
- original BSP-produced `viewSprites` ordering
- real projected bitshape-backed menu sprites
- real wall-backed sprite objects through bounded wall frames
- sprite render modes used by this frame, including additive mode 7
- exact sprite request sequence and frame-size measurements
- deterministic walls+sprites framebuffer `ffe0995e`
- exact heap/largest-block restoration after every uncached sprite frame

Still intentionally NOT integrated:

- original `Render_loadBitShapes()`
- original `Render_loadTexels()`
- monolithic `shapeData`
- monolithic/map-wide `mediaTexels`
- any sprite cache
- textured floor/ceiling rendering
- persistent open native pack
- complete map graphics loader replacement
- game entities/player spawning in the active gameplay loop
- final main-menu UI/logo/text composition
- normal main-menu state-machine presentation
- main gameplay loop

## Recommended next increment after merge

Do not add another visual subsystem in the same increment as the first sprite
cache. First replay this exact deterministic walls+sprites frame with a tiny
sprite LRU and demand `ffe0995e` bit-for-bit.

Two evidence-based candidates exist:

```text
3-slot sprite LRU: expected 2 hits / 9 misses, ~6,038 B peak logical payload
5-slot sprite LRU: expected 3 hits / 8 misses, ~7,437 B peak logical payload
```

A 4-slot cache is dominated by 3 slots on this frame because it gives no extra
hit. A cache larger than 5 slots gives no additional hit.

Whichever candidate is chosen, acceptance criteria should include:

- wall regression remains `a6d87c4a`
- final walls+sprites framebuffer remains `ffe0995e`
- sprite request stream remains `4457ac94`
- exact hit/miss/eviction accounting matches simulation
- physical sprite loads/bytes fall as predicted
- allocator peak and largest-free-block impact are measured on the real CYD
- cache teardown restores exact heap/largest state
- `shapeData == NULL` and `mediaTexels == NULL` throughout

Only after the sprite resource lifetime is stable should the next visual piece
(textured planes or actual menu UI composition) be added.

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
