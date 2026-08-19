# Doom RPG ESP32 CYD porting status

This file is the recovery point for the ESP32-2432S028R (classic Cheap Yellow
Display, no PSRAM) port. Update it after every hardware-validated increment.

## Target

- ESP32-2432S028R / classic ESP32 CYD, no PSRAM
- ESP32-D0WD-V3, 240 MHz, 4 MB flash
- ILI9341 320x240 landscape
- XPT2046 touch on separate software SPI path
- microSD game data at `/DoomRPG.zip`
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
    allocation plan (`agent/esp32-menu-bsp-structure-plan`, PR #10, merge
    commit `f564e38bc205ce21116d3d866a6fdf2646d62965`).

## Current validated increment

Branch: `agent/esp32-menu-map-runtime-structures`

Status: **HARDWARE VALIDATED, READY TO MERGE**.

Objective: execute the real `Render_beginLoadMap(MAP_MENU)` and the real
structural portion of `Render_beginLoadMapData()`, keep the resulting runtime
map structures resident, and stop exactly before `Render_loadBitShapes()` /
`Render_loadTexels()`.

No source-tree engine file is modified. The ESP32 build uses GNU linker wrapping
of `DoomCanvas_updateLoadingBar()`. The wrapper is pass-through during normal
startup and is armed only around `Render_beginLoadMapData()`. At the seventh
loading-bar call, after the BSP has been fully parsed and freed and immediately
before bitshape loading, the probe returns via a controlled `longjmp`.

## Validated starting plan

Previous hardware plan for the structural phase:

```text
[BSPPLAN] nodes=53
[BSPPLAN] lines=120
[BSPPLAN] mapSprites=44 runtimeSprites=68
[BSPPLAN] events=15
[BSPPLAN] byteCodes=15
[BSPPLAN] strings=0
[BSPPLAN] Structural payload=13980B largestAlloc=4096B
```

ESP32 runtime sizes:

```text
sizeof(Node_t)=44
sizeof(Line_t)=32
sizeof(Sprite_t)=36
sizeof(void*)=4
sizeof(int)=4
```

## Real Render_beginLoadMap result

Hardware baseline at the start of the real map-runtime probe:

```text
[MAPSTRUCT] Begin heap8=44836 largest8=23540 plannedPayload=13980 largestAlloc=4096
```

The real engine path is called:

```text
[MAPSTRUCT] -> Render_beginLoadMap(MAP_MENU)
---Render_loadMappings---
[ZIP] read mappings.bin method=8 c=2156 u=8392
[ZIP] inflate mappings.bin c=2156 u=8392 state=10992
---Render_beginLoadMap---
[ZIP] read menu.bsp method=8 c=1401 u=4494
[ZIP] inflate menu.bsp c=1401 u=4494 state=10992
[MAPSTRUCT] Render_beginLoadMap result=1 heap8=40324 largest8=27636 ioBuffer=0x3fff7fac pos=33
```

The header cursor is exactly 33 as expected and the real decompressed BSP is
retained as `Render_t::ioBuffer` before the data phase.

Note: `Render_beginLoadMap()` reloads the mappings. The previous mapping arrays
are freed/reallocated rather than permanently duplicated. The resulting heap
layout happens to improve the largest contiguous block from 23,540 to 27,636
bytes while the BSP is resident.

## Real Render_beginLoadMapData structural phase

The original engine function runs unchanged:

```text
[MAPSTRUCT] -> real Render_beginLoadMapData(), armed stop before bitshapes
---Render_beginLoadMapData---
[SDL] Sharing platform framebuffer: 38400 bytes
[VIDEO] Present 160x120 -> 320x240 exact 2x: 34472 us
```

The controlled boundary is reached exactly at loading-bar call 7:

```text
[MAPSTRUCT] Boundary reached before bitshapes/texels loadingBarCalls=7 heap8=30744 largest8=21492
```

At this point:

- the full BSP structural data has been consumed
- `render->ioBuffer` has been freed and cleared by the probe path
- nodes, lines, sprites, events and bytecode are real engine allocations
- blockmap and plane texture maps have been consumed
- map texture/sprite reference lists are populated
- `Render_loadBitShapes()` has NOT executed
- `Render_loadTexels()` has NOT executed

## Real runtime counts

The real engine-produced values exactly match the preflight plan:

```text
[MAPSTRUCT] Real runtime counts nodes=53 lines=120 mapSprites=44 runtimeSprites=68 events=15
```

Persistent pointers are all valid:

```text
[MAPSTRUCT] Persistent pointers nodes=0x3ffe3400 lines=0x3ffe6074 sprites=0x3fffa15c events=0x3ffb6310 byteCode=0x3ffdf8e0
```

`menu.bsp` contains no strings, as previously parsed.

## Measured structural memory cost

The validated raw payload prediction was 13,980 bytes.

Real hardware result:

```text
[MAPSTRUCT] Persistent real structures used=14092B planPayload=13980B overhead=112B
```

So allocator/bookkeeping overhead for the complete real structural phase is only
112 bytes.

Authoritative post-structure memory baseline:

```text
heap8=30744
largest8=21492
```

This is now the starting memory budget for bitshape/texel work.

## Resource-reference counts for the next stage

The real structural loader also produced the exact graphics reference lists:

```text
[MAPSTRUCT] Resource refs mapTextures=84 mapSprites=284 planeTextures=11
[MAPSTRUCT] Scratch refs textures=0x3ffe2ff0 sprites=0x3fff914c counts=84/284 planes=11
```

These are critical for the next increment:

- unique/selected map wall/floor texture references: 84 entries
- sprite/bitshape references: 284 entries
- plane texture IDs used by the menu BSP: 11

The 1,024-byte `mapTextureTexels` and 4,096-byte `mapSpriteTexels` scratch arrays
remain resident at this boundary because the original `Render_beginLoadMapData()`
only frees them after bitshape/texel loading completes.

## Final hardware result

```text
[MAPSTRUCT] Probe-stop returned from real loader boundary=reached calls=7 heap8=30744 largest8=21492
[MAPSTRUCT] READY real menu structures resident heap8=30744 largest8=21492
[MAPSTRUCT] Render_loadBitShapes / Render_loadTexels intentionally NOT executed
[MENUBSP] READY menu.bsp plan + real runtime structures validated
[MENUBSP] Bitshapes / texels still intentionally NOT executed
```

Heartbeat remains stable:

```text
[ALIVE] ... heap=96560 heap8=30744 largest8=21492 ... RENDER=ready MAPPINGS=ready MENUBSP=ready ...
```

Touch and the shared framebuffer remain operational after the real map
structures become resident.

This branch therefore passes its hardware merge gate.

## Major memory work already validated

- shared 160x120 RGB565 platform / SDL / Render framebuffer: 38,400 bytes total
- desktop duplicate `piDIB` / Render framebuffer allocations removed on ESP32
- packed indexed BMP 1/4/8-bpp storage and zero-copy texture ownership
- approximately 52 KB recovered at the DoomCanvas layout boundary
- miniz 10,992-byte decompressor state moved from loopTask stack to heap
- desktop `mediaPlanes[24][64*64]` removed from ESP32 `Render_t`
- persistent mappings payload: 8,376 bytes / measured 8,440 bytes
- complete real menu runtime structural state: measured 14,092 bytes

## Current safe stop boundary

Validated and executed:

- core object graph
- `DoomCanvas_startup()` / HUD / layout
- pre-render startup resources
- real `Render_startup()`
- `Game_loadConfig()` first-boot path
- real persistent `Render_loadMappings()`
- real `Render_beginLoadMap(MAP_MENU)`
- real structural portion of `Render_beginLoadMapData()`
- real nodes, lines, sprites, events, bytecodes and texture/sprite reference lists
- BSP buffer consumption and release

Still intentionally NOT executed:

- `Render_loadBitShapes()` / `/bitshapes.bin`
- `Render_loadTexels()` / `/wtexels.bin` / `/stexels.bin`
- final media texel / shape-data persistent buffers
- completion of `Render_beginLoadMapData()`
- game entities / player spawning for the map
- main game loop

## Recommended next increment after merge

Start a new branch from the newly merged `main` and preflight the exact graphics
payload implied by the real reference lists before allowing either graphics
loader to allocate.

Recommended order:

1. inspect `/bitshapes.bin`, `/wtexels.bin`, `/stexels.bin` compressed and
   uncompressed ZIP sizes
2. using the already-populated real `mapSpriteTexels[284]` and mapping offsets,
   reproduce the first sizing pass of `Render_loadBitShapes()` and calculate the
   exact `shapeData` allocation size
3. using the real `mapTextureTexels[84]`, `mapSpriteTexels[284]` and mapping
   tables, reproduce the sizing pass of `Render_loadTexels()` and calculate the
   exact `mediaTexels` allocation size
4. include transient ZIP/miniz requirements while the current runtime structures
   remain resident
5. compare all individual allocations with the current largest block
6. do not execute the real graphics loaders until the plan is proven safe

Authoritative starting budget for the next increment:

```text
heap8=30744
largest8=21492
mapTextureTexelsCount=84
mapSpriteTexelsCount=284
planeTexturesCnt=11
```

This is the first point where bitshape/texel payload size, rather than BSP
structure size, is expected to be the main memory risk.

## Increment discipline

- Start each new increment from the latest hardware-validated `main`.
- One branch = one small testable objective.
- Fix failures on the same branch; do not create the next branch early.
- Hardware validation on the real CYD is the merge gate.
- Record measured hardware values here after every successful increment.
