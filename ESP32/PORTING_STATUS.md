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
- small measured working sets and measured caches
- no whole-resource graphics inflation
- no monolithic `shapeData`
- no monolithic/map-wide `mediaTexels`
- shared 160x120 RGB565 framebuffer
- canonical RGB565 palette convention for native consumers
- storage/file-format access isolated behind GFXRM
- cache/reuse policy isolated from storage and rasterization
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
23. First real `menu.bsp` walls-only scene: real spawn camera, original BSP
    traversal/culling/occlusion, 25 real wall requests across 8 textures, 224
    native spans and deterministic framebuffer `a6d87c4a`, still with
    `shapeData == NULL` / `mediaTexels == NULL`
    (`agent/esp32-menu-walls-native-frame`, PR #22, merge
    `0f30003f61e81bae20f6c5b81bda52cea3b8ff9e`).

## Current validated increment

Branch: `agent/esp32-menu-wall-lru-cache`

Status: **HARDWARE VALIDATED, DOCUMENTED, READY TO MERGE**.

Objective: keep the exact real `menu.bsp` walls-only frame from PR #22 but add a
measured three-slot wall-frame LRU cache. The cache must reduce physical SD/pack
loads while preserving the exact request order, projection result, framebuffer
and final allocator state.

## Why three wall slots

PR #22 measured this exact request sequence:

```text
116, 32, 40, 112, 108, 108, 116, 116, 116, 112,
116, 112, 108, 108, 108, 116, 44, 0, 40, 152,
152, 116, 116, 116, 152
```

There are 25 logical requests, 8 unique textures and 17 repeat requests.
Simulation before implementation predicted:

```text
LRU slots   hits   misses   resident payload
1             8      17       2,048 B
2            11      14       4,096 B
3            14      11       6,144 B
4            14      11       8,192 B
```

A fourth slot gives no additional hit on this deterministic frame, so three
slots are the smallest measured sweet spot.

That prediction is now hardware-validated.

## Cache architecture

The cache is deliberately separate from GFXRM and the rasterizer:

```text
DoomRPG-ESP32.pak
        |
        v
      GFXRM
(storage / bounded load)
        |
        v
NativeWallLruCache - 3 slots x 2,048 B
(reuse / LRU ownership)
        |
        v
borrowed EspNativeWallFrame
        |
        v
ESP32-native projected wall spans
        |
        v
shared 160x120 RGB565 framebuffer
```

Responsibilities:

```text
GFXRM                      = knows pack/file layout and loads a wall frame
NativeWallLruCache         = owns cached wall frames and LRU replacement
ProjectedWallRenderer      = borrows a frame and rasterizes it
```

The projected-wall bridge now supports borrowed ownership. On a cache hit/miss,
`EspNativeProjectedWall_beginBorrowed()` receives a view of a cache-owned frame.
`EspNativeProjectedWall_end()` releases only the borrowed view; it does not free
the cache slot. Cache teardown remains the sole owner of slot destruction.

The original owning path remains available for previous probes, so cache use is
opt-in rather than a hidden global policy change.

## Hard graphics invariants

Throughout the cached real-scene draw:

```text
shapeData   = NULL
mediaTexels = NULL
```

Global `mediaTexelOffsets[]` are never rewritten. Per-span diagnostics remain:

```text
rangeErrors          = 0
legacyPtrViolations  = 0
mappingViolations    = 0
```

## Authoritative hardware validation

Hardware output:

```text
=== Doom RPG ESP32 real menu.bsp walls-only frame + 3-slot LRU ===
[MENUWALL] Begin heap8=30392 largest8=22516 shapeData=0x0 mediaTexels=0x0
[MENUWALL] BSP header spawnIndex=460 tile=12,14 world=800,928 dir=0 cameraZ=36
[WALLCACHE] BEGIN slots=3 payloadPerSlot=2048B maxPayload=6144B cold=yes
...
[MENUWALL] BSP visibility nodeCount=28 nodeRasterCount=10 visibleLeaves=10 sourceLines=46
[MENUWALL] Line result walls=25 backface=15 clipped=6 spriteSpanSkipped=0 occluderOnly=0
[MENUWALL] Texture requests total=25 unique=8 repeats=17 trackingErrors=0 requestFNV=4db9da28 animFrameTime=0
[MENUWALL] LRU slots=3 requests=25 hits=14 misses=11 evictions=8 resident=3 peak=3 residentBytes=6144 peakBytes=6144
[MENUWALL] LRU resident heap8=24200 largest8=16372 aggregateCost=6192B logicalPayload=6144B
[MENUWALL] GFXRM wallLoads=11 packOpenCycles=11 logicalBytes=22528 expected=22528 peakFrame=2048
[MENUWALL] Native spans begin=25 end=25 spanCalls=224 pixels=5589 rangeErrors=0 legacyPtrViolations=0 mappingViolations=0
[MENUWALL] framebufferFNV=a6d87c4a expected=a6d87c4a renderMs=1357 floor=4208 ceiling=8c51 mediaTexels=0x0
[WALLCACHE] END requests=25 hits=14 misses=11 evictions=8 resident=3 peak=3 residentBytes=6144 peakBytes=6144
[MENUWALL] End heap8=30392 largest8=22516 deltaFromStart=0 cacheReleased=yes
[MENUWALL] READY framebuffer stayed bit-identical while LRU reduced 25 requests to 11 physical wall loads
[MENUWALL] READY measured three-slot cache = 14 hits / 11 misses / 8 evictions / 6144B peak payload
[ALIVE] ... heap8=30392 largest8=22516 ... MENUBSP=ready ...
```

## Deterministic regression contract

The cached frame must preserve the complete PR #22 scene contract:

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
logical wall requests     = 25
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
framebuffer FNV           = a6d87c4a
floor RGB565              = 4208
ceiling RGB565            = 8c51
```

The wall-cache-specific contract is:

```text
slots                     = 3
requests                  = 25
hits                      = 14
misses                    = 11
evictions                 = 8
resident slots at peak    = 3
logical resident payload  = 6,144 B
measured allocator cost   = 6,192 B
GFXRM physical wall loads = 11
GFXRM pack open cycles    = 11
GFXRM logical bytes read  = 22,528 B
GFXRM peak single frame   = 2,048 B
```

Most importantly, cached and uncached real-scene framebuffer hashes are exactly
identical:

```text
uncached PR #22 framebuffer = a6d87c4a
cached current framebuffer  = a6d87c4a
```

This proves the cache changes resource lifetime/I/O only, not rendering output.

## Memory boundary

Current branch baseline before cache activation:

```text
heap8=30392
largest8=22516
```

With all three wall slots resident:

```text
heap8=24200
largest8=16372
logical cache payload = 6144 B
allocator cost        = 6192 B
```

After `NativeWallLruCache` teardown:

```text
heap8=30392
largest8=22516
deltaFromStart=0
cacheReleased=yes
```

The baseline is 184 B below the previous branch's 30,576-byte value. That is
static cache/borrowed-path bookkeeping introduced by this increment, not a
per-frame leak. The cache itself releases exactly and restores both free heap and
the largest free block.

The wall cache remains comfortably inside the no-PSRAM memory boundary while the
validated largest free block stays 16,372 B even with all three slots resident.

## I/O and timing result

Compared with the identical uncached PR #22 frame:

```text
                          uncached      3-slot LRU
logical wall requests       25              25
physical GFXRM loads         25              11
pack open cycles             25              11
wall bytes read          51,200          22,528
framebuffer FNV         a6d87c4a        a6d87c4a
measured renderMs          1676            1357
```

The measured render time improved by 319 ms (about 19%) on this hardware run.
Timing is diagnostic, not a regression assertion: SD timing and logging can vary
between boots. Request accounting, hashes and allocator restoration are the
stable contracts.

## Architectural conclusion

This is the first hardware-validated reusable graphics cache in the ESP32
engine. The project now has a clean three-layer wall resource path:

```text
storage        -> GFXRM
reuse/lifetime -> NativeWallLruCache
rasterization  -> native projected wall renderer
```

No layer needs to pretend that a map-wide `mediaTexels` pool exists.

The cache slot count is evidence-driven. It was chosen only after a real scene
provided an exact access sequence, then validated against the same scene. This
workflow should also be used for sprite caching later.

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
- deterministic real menu walls framebuffer `a6d87c4a`
- exact real wall access sequence `4db9da28`
- three-slot LRU wall cache
- borrowed wall-frame ownership between cache and projected renderer
- 14 hits / 11 misses / 8 evictions on the reference frame
- 25 -> 11 physical wall loads
- 51,200 -> 22,528 B physical wall payload reads
- 6,144 B logical / 6,192 B allocator cache cost
- exact heap/largest-block restoration after cache teardown

Still intentionally NOT integrated:

- original `Render_loadBitShapes()`
- original `Render_loadTexels()`
- monolithic `shapeData`
- monolithic/map-wide `mediaTexels`
- projected real-scene sprite rendering
- a measured real-scene sprite access sequence/cache policy
- textured floor/ceiling rendering
- persistent open native pack
- complete map graphics loader replacement
- game entities/player spawning in the active gameplay loop
- final main-menu UI composition
- main gameplay loop

## Recommended next increment after merge

Add **real projected sprites to this same deterministic `menu.bsp` camera**, but
do not invent a sprite cache yet.

Keep the scope narrow:

- keep the validated three-slot wall LRU active
- reuse the `viewSprites` ordering produced by the original BSP walk
- use the already validated bounded sprite frame contract from GFXRM
- project/rasterize only the real sprites visible in this menu frame
- keep floor/ceiling texturing disabled for this increment
- keep `shapeData == NULL` and `mediaTexels == NULL`
- measure the exact sprite request sequence, unique sprites, repeated requests,
  bytes and peak frame size
- establish a deterministic framebuffer hash for walls + sprites
- release every uncached sprite frame exactly and preserve allocator recovery

Only after the real sprite request sequence is measured should a sprite cache
size/policy be proposed. Follow the same evidence-driven process used for walls.

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
