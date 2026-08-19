# Doom RPG ESP32 CYD porting status

This file is the recovery point for the ESP32-2432S028R (classic Cheap Yellow
Display, no PSRAM) port. Update it as part of every hardware-validated increment,
on the same branch as the code, before that branch is merged.

## Target

- ESP32-2432S028R / classic ESP32 CYD, no PSRAM
- ESP32-D0WD-V3, dual core, 240 MHz
- 4 MB flash
- ILI9341 320x240 landscape
- XPT2046 touch
- microSD-backed game data
- internal render target: 160x120 RGB565 = 38,400 B
- gameplay viewport: 160x80 at framebuffer y=20
- physical output: exact nearest-neighbour 2x to 320x240
- audio disabled/stubbed during bring-up

## Project direction

DoomRPG-RE is an executable specification for behaviour, data formats and useful
rendering semantics. Its desktop/reverse-engineered memory architecture is not a
contract for the ESP32 target.

The ESP32 implementation is becoming its own constrained engine:

- bounded deterministic RAM use
- SD as immutable backing store
- small measured working sets and evidence-driven caches
- no monolithic `shapeData`
- no map-wide `mediaTexels`
- one shared 160x120 RGB565 framebuffer
- canonical RGB565 palette convention for native consumers
- storage/file-format access isolated behind GFXRM
- cache/reuse policy isolated from storage and rasterization
- original projection/BSP/game semantics preserved where useful
- changes made one hardware-validated subsystem at a time

Core philosophy:

> We are no longer forcing DoomRPG-RE onto ESP32. We are building an ESP32 Doom
> RPG engine from the behaviour and data model proven by DoomRPG-RE.

## Increment discipline

1. Start from the exact latest hardware-validated `main` SHA.
2. One branch = one small measurable objective.
3. Build/flash/test on the real CYD.
4. Fix failures on the same branch.
5. Only after hardware PASS, update every relevant `.md` on that same branch.
6. Only then is the branch merge-ready.
7. Merge it.
8. Only after merge acknowledgement start the next increment.

Documentation is part of the increment, not a later cleanup task.

## Hardware-validated milestones already merged to main

1. TFT / SD / engine link bring-up.
2. Touch calibration/orientation (`agent/esp32-touch-input-layer`, PR #1).
3. Native 160x120 framebuffer + exact 2x output (PR #2).
4. SDL compatibility renderer sharing the platform framebuffer (PR #3).
5. Real 12-object Doom RPG core graph (PR #4).
6. Real `DoomCanvas_startup()` + HUD/font/legal resources + 160x120 layout (PR #5).
7. `ParticleSystem`, `MenuSystem`, `EntityDef` startup with packed indexed BMPs (PR #6).
8. Real `Render_startup()` + `sintable.bin` + `palettes.bin` on shared framebuffer (PR #7).
9. `Game_loadConfig()` + real `Render_loadMappings()` (PR #8).
10. Real `/menu.bsp` ZIP inflate + exact 33-byte header parse (PR #9).
11. Complete `menu.bsp` structural parse/allocation plan (PR #10).
12. Real map runtime structures, stopped before graphics inflation (PR #11).
13. Proof that legacy monolithic graphics cannot fit the no-PSRAM target (PR #12).
14. First native asset-pack random access primitive (PR #13).
15. Full ESP32 asset pack v2, 241 resources, direct uncompressed access (PR #14).
16. Native on-demand bitshape model, zero resident `shapeData` (PR #15).
17. Native selected sprite-texel random access (PR #16).
18. First complete native sprite render, sprite 172 (PR #17).
19. First complete native wall render, texture 112 (PR #18).
20. Shared bounded graphics resource manager GFXRM (PR #19).
21. First projected wall through bounded GFXRM compatibility bridge (PR #20).
22. Direct native projected-wall sampling, framebuffer `ad191f54` (PR #21).
23. First real `menu.bsp` walls-only frame, framebuffer `a6d87c4a` (PR #22).
24. Three-slot wall LRU: 25 requests -> 11 loads, same `a6d87c4a` (PR #23,
    merge `e39079253ef72c17a908c1c2633762d20fe5f29e`).
25. Real BSP-sorted menu sprites rendered over the real wall frame, uncached
    sprite working set measured, final framebuffer `ffe0995e` (PR #24, merge
    `0ba4ca8efa15974846bee638333e52e12bbda0e3`).

## Current validated increment

Branch: `agent/esp32-menu-sprite-lru-cache`

Base `main` SHA:

```text
0ba4ca8efa15974846bee638333e52e12bbda0e3
```

Status: **HARDWARE VALIDATED, DOCUMENTED, READY TO MERGE**.

Objective: replay the exact PR #24 real walls+sprites frame with a three-slot
sprite LRU. Resource lifetime/I/O may change; geometry, projection, request
semantics and framebuffer pixels must not.

## Why three sprite slots

PR #24 measured the exact sprite-frame request stream:

```text
562, 406, 410, 598, 172, 578, 578, 426, 410, 578, 102
```

Frame sizes:

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

LRU analysis before implementation:

```text
slots  hits  misses  evictions  peak logical resident bytes
1       1      10       9        2,112 B
2       1      10       8        4,089 B
3       2       9        6        6,038 B
4       2       9        5        6,834 B
5       3       8        3        7,437 B
```

Three slots were selected because they capture two of the three available repeat
hits while keeping the peak logical working set at 6,038 B. A fourth slot gives
no extra hit. Five slots save only one additional physical load while increasing
the reference-frame peak by 1,399 B.

The three-slot prediction is now hardware-validated exactly.

## Sprite cache architecture

The validated projected-sprite renderer and GFXRM loader were deliberately left
unchanged. Cache integration is opt-in at the linker boundary:

```text
ProjectedSpriteRenderer
        |
        v
EspNativeGraphics_loadSpriteFrame()
        |
        v
GNU ld --wrap boundary
        |
        +-- cache inactive --> __real_loadSpriteFrame() --> GFXRM
        |
        +-- cache active ----> NativeSpriteLruCache (3 slots)
                                   |
                                   +-- HIT  -> borrowed frame view
                                   |
                                   +-- MISS -> __real_loadSpriteFrame() -> GFXRM
```

`EspNativeGraphics_releaseSpriteFrame()` is wrapped symmetrically:

- cache inactive: transparent call to the real GFXRM release;
- cache active: clear only the borrowed view returned to the renderer;
- cache eviction/teardown: the cache itself calls the real release and remains the
  sole owner of resident frame storage.

PlatformIO linker flags added by this increment:

```text
-Wl,--wrap=EspNativeGraphics_loadSpriteFrame
-Wl,--wrap=EspNativeGraphics_releaseSpriteFrame
```

This preserves every previous uncached probe because the wrappers are transparent
unless `EspNativeSpriteCache_begin()` has explicitly activated the cache.

## Hard graphics invariants

Throughout the cached real-scene sprite pass:

```text
shapeData   = NULL
mediaTexels = NULL
```

And:

```text
rangeErrors          = 0
legacyPtrViolations  = 0
shapeDataViolations  = 0
mappingViolations    = 0
unsupportedFlags     = 0
unsupportedModes     = 0
```

No global resource mapping is rewritten.

## Deterministic scene contract

The wall pass remains unchanged:

```text
mapSpawnIndex              = 460
spawn tile                 = 12,14
world X/Y                  = 800,928
mapSpawnDir                = 0
camera Z                   = 36
animFrameTime              = 0
wall framebuffer FNV       = a6d87c4a
wall request FNV           = 4db9da28
wall LRU                   = 14 hits / 11 misses / 8 evictions
wall physical loads        = 11
```

The sprite semantic regression remains:

```text
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
final framebuffer FNV       = ffe0995e
```

## Authoritative hardware validation

```text
=== Doom RPG ESP32 real menu.bsp walls + native sprites + 3-slot sprite LRU ===
[MENUSPRITE] Begin wallsFNV=a6d87c4a expected=a6d87c4a shapeData=0x0 mediaTexels=0x0
[MENUSPRITE] Baseline heap8=29852 largest8=21492 numMapSprites=44 runtimeSlots=68
[MENUSPRITE] View list objects=17 outOfRange=0 listFNV=962cd657 ordering=original-BSP-sortZ
[SPRITECACHE] BEGIN slots=3 variablePayload=yes cold=yes
...
[MENUSPRITE] Objects total=17 hidden=0 lightsSkipped=5 entityUnsupported=0 resolvedDraws=14
[MENUSPRITE] Requests spriteFrames=11 unique=8 repeats=3 requestFNV=4457ac94 wallBacked=2 maxFrame=2112B
[MENUSPRITE] Sprite LRU slots=3 requests=11 hits=2 misses=9 evictions=6 resident=3 peak=3 residentBytes=3709 peakBytes=6038 maxFrame=2112B
[MENUSPRITE] Sprite LRU resident heap8=26092 largest8=17396 currentCost=3760B logicalCurrent=3709B logicalPeak=6038B
[MENUSPRITE] Cull near=1 backface=0 clipped=1 spans=389 pixels=4590
[MENUSPRITE] Invariants rangeErrors=0 legacyPtrViolations=0 shapeDataViolations=0 mappingViolations=0 unsupportedFlags=0 unsupportedModes=0
[MENUSPRITE] GFXRM spriteLoads=9 wallLoads=2 packOpenCycles=11 logicalBytes=14830 expected=14830 peakFrame=2112
[MENUSPRITE] Wall-backed projected begin=2 end=2 spans=20 pixels=340 errors=0/0/0
[MENUSPRITE] framebufferFNV=ffe0995e expected=ffe0995e wallsFNV=a6d87c4a renderMs=1038 shapeData=0x0 mediaTexels=0x0
[SPRITECACHE] END requests=11 hits=2 misses=9 evictions=6 resident=3 peak=3 residentBytes=3709 peakBytes=6038 maxFrame=2112
[MENUSPRITE] End heap8=29852 largest8=21492 deltaFromStart=0 cacheReleased=yes
[MENUSPRITE] READY framebuffer stayed bit-identical while sprite LRU reduced 11 requests to 9 physical sprite loads
[MENUSPRITE] READY measured three-slot sprite cache = 2 hits / 9 misses / 6 evictions / 6038B peak logical payload
[ALIVE] ... heap8=29852 largest8=21492 ... MENUBSP=ready ...
```

## Exact cached request behaviour

Logical request sequence is unchanged:

```text
562 MISS
406 MISS
410 MISS
562 -> 598 EVICT/MISS
406 -> 172 EVICT/MISS
410 -> 578 EVICT/MISS
578 HIT
598 -> 426 EVICT/MISS
172 -> 410 EVICT/MISS
578 HIT
426 -> 102 EVICT/MISS
```

Hardware totals:

```text
slots                 = 3
requests              = 11
hits                  = 2
misses                = 9
evictions             = 6
peak resident slots   = 3
current resident data = 3,709 B
peak logical data     = 6,038 B
max single frame      = 2,112 B
```

Final resident set immediately before teardown:

```text
410 =  796 B
578 = 1977 B
102 =  936 B
------------
      3709 B
```

The measured current allocator cost for that final resident set is 3,760 B.
There is no direct hardware snapshot of allocator cost at the earlier 6,038-byte
logical peak, so do not invent one from the logical value.

## I/O result

Uncached PR #24 sprite pass:

```text
sprite loads       = 11
sprite bytes       = 14,688 B
wall-backed loads  = 2
wall bytes         = 4,096 B
pack open cycles   = 13
GFXRM logical total= 18,784 B
```

Current three-slot cache:

```text
sprite loads       = 9
sprite bytes       = 10,734 B
wall-backed loads  = 2
wall bytes         = 4,096 B
pack open cycles   = 11
GFXRM logical total= 14,830 B
peak single frame  = 2,112 B
```

So the cache removes two sprite loads and 3,954 B of sprite payload reads
(about 27% of the uncached sprite bytes). Pack-open cycles fall from 13 to 11.

## Timing result

Measured sprite-pass time:

```text
uncached PR #24 = 1064 ms
3-slot LRU      = 1038 ms
```

This run is only about 26 ms / 2.4% faster. Timing is diagnostic, not a
regression contract: SD latency and verbose serial logging vary. The durable
proof is the exact request accounting, lower physical loads, deterministic hashes
and allocator restoration.

## Memory boundary

Current branch baseline before cache activation:

```text
heap8=29852
largest8=21492
```

With the final three resident frames still cached:

```text
heap8=26092
largest8=17396
logical resident bytes=3709
measured current cost =3760 B
```

After `EspNativeSpriteCache_end()`:

```text
heap8=29852
largest8=21492
deltaFromStart=0
cacheReleased=yes
```

The branch baseline differs from PR #24 because this increment adds cache state
and linker-wrapper integration. The per-pass leak contract is exact restoration
to the branch's own starting allocator state; hardware passes that contract.

## Current native graphics architecture

```text
                     SD / DoomRPG-ESP32.pak
                              |
                              v
                            GFXRM
                          /       \
                         /         \
               sprite frames      wall frames
                    |                  |
        NativeSpriteLruCache(3)   NativeWallLruCache(3)
                    |                  |
              borrowed frame      borrowed frame
                    |                  |
                    v                  v
          native projected       native projected
             sprite spans           wall spans
                    \                  /
                     \                /
                      shared 160x120 RGB565
                              |
                              v
                        exact 2x display
```

Current deterministic signatures:

```text
sprite 172 texel FNV         = 0c0a7acd
wall 112 texel FNV           = 92d40704
synthetic projected wall FNV = ad191f54
real menu wall request FNV   = 4db9da28
real menu walls FNV          = a6d87c4a
viewSprites list FNV         = 962cd657
sprite request FNV           = 4457ac94
real menu walls+sprites FNV  = ffe0995e
wall LRU                     = 14 / 11 / 8
sprite LRU                   = 2 / 9 / 6
```

## Current safe stop boundary

Validated and executed:

- complete engine startup through mappings
- real `Render_beginLoadMap(MAP_MENU)` structural load
- 53 nodes, 120 lines, 44 map sprites, 68 runtime sprite slots, 15 events
- native asset-pack access
- zero resident `shapeData`
- zero map-wide `mediaTexels`
- canonical RGB565 palette normalization
- bounded GFXRM sprite + wall frames
- native direct projected wall sampling
- original BSP traversal/culling/occlusion on real `menu.bsp`
- real menu spawn camera
- hardware-validated three-slot wall LRU
- original BSP-produced `viewSprites` ordering
- real projected bitshape-backed sprites
- wall-backed sprite objects
- native sprite span modes used by this frame, including mode 7
- hardware-validated three-slot sprite LRU
- exact framebuffer `ffe0995e` with both caches changing only resource lifetime
- exact heap/largest-block restoration after cache teardown

Still intentionally NOT integrated:

- original `Render_loadBitShapes()`
- original `Render_loadTexels()`
- monolithic `shapeData`
- monolithic/map-wide `mediaTexels`
- textured floor/ceiling planes
- persistent caches across normal multi-frame runtime
- persistent open native pack
- complete graphics-loader replacement
- gameplay entities/player active in the normal game loop
- final main-menu logo/text/buttons composition
- normal main-menu state-machine presentation
- main gameplay loop
- audio

## Recommended next increment after merge

The resource lifetime for the current menu walls and sprites is now measured and
stable. The next increment should become visual again instead of adding another
cache.

Best next direction: inspect and integrate **one small piece of the real main-menu
composition** over the validated `ffe0995e` scene, using the original
`DoomCanvas`/`MenuSystem` behaviour as specification. Do not combine menu UI and
textured floor/ceiling work in the same branch.

Before coding that increment, trace the exact original menu composition path
(`DoomCanvas` menu state + `MenuSystem` paint/draw calls) and select the smallest
visible element that can be placed over the existing deterministic scene while
keeping all current hashes as pre-overlay regression boundaries.

The alternative is textured floor/ceiling integration, but it should be a
separate later increment unless code analysis proves the original main-menu view
requires it for correct composition.
