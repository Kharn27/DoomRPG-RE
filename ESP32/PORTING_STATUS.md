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
8. Real `Render_startup()` using the platform-owned shared framebuffer, plus
   `sintable.bin` and `palettes.bin`
   (`agent/esp32-render-startup-shared-framebuffer`, PR #7).
9. `Game_loadConfig()` first-boot path + real `Render_loadMappings()`
   (`agent/esp32-config-mappings-startup`, PR #8).
10. Real ZIP load/inflate of `/menu.bsp` + exact fixed 33-byte header parse,
    with full heap recovery (`agent/esp32-menu-bsp-preflight`, PR #9, merge
    commit `ad1416972353eb2a87e1e7cf1295d3db57db93c2`).

The real core graph costs 56,152 bytes of MALLOC_CAP_8BIT heap. `Game_t` is the
largest individual core allocation at 36,484 bytes.

Validated DoomCanvas geometry:

- clip: 160x120
- display: 160x120
- top HUD: 20 px
- bottom HUD: 20 px
- gameplay viewport: 160x80
- `Render_t` viewport: 160x80
- `Render_setup()` arrays: 1,280 bytes total

## Current validated increment

Branch: `agent/esp32-menu-bsp-structure-plan`

Status: **HARDWARE VALIDATED, READY TO MERGE**.

Objective: parse the complete real `menu.bsp` byte-for-byte in exactly the order
consumed by `Render_beginLoadMapData()`, and calculate the exact ESP32 runtime
allocation plan without yet creating any of those runtime structures.

No engine source file is patched by this increment. The probe only loads a
temporary decompressed `menu.bsp`, walks the serialized data, computes the
runtime budget from the real ESP32 `sizeof()` values, then frees the BSP.

## Complete menu.bsp format proved on hardware

Archive entry remains:

```text
menu.bsp c=1401 u=4494
```

The hardware compiler reports:

```text
[BSPPLAN] sizeof Node=44 Line=32 Sprite=36 ptr=4 int=4
```

The parser follows the same serialized order as `Render_beginLoadMapData()`:

1. fixed 33-byte map header
2. nodes
3. lines
4. map sprites
5. tile events
6. bytecodes
7. strings
8. 256-byte packed blockmap
9. 2 x 1024-byte plane texture maps

`DoomRPG_shiftCoordAt()` consumes one byte per serialized coordinate, so the
compact serialized record sizes are:

- node: 10 bytes
- line: 10 bytes
- map sprite: 5 bytes
- event: 4 bytes
- bytecode: 9 bytes

Hardware result:

```text
[BSPPLAN] nodes=53 serialized=530B end=565
[BSPPLAN] lines=120 serialized=1200B end=1767
[BSPPLAN] mapSprites=44 runtimeSprites=68 serialized=220B end=1989
[BSPPLAN] events=15 serialized=60B end=2051
[BSPPLAN] byteCodes=15 serialized=135B end=2188
[BSPPLAN] strings=0 runtimeChars=0B maxStringAlloc=0B end=2190
[BSPPLAN] blockmap=256B planes=2048B finalPos=4494/4494
```

The decisive proof is:

```text
finalPos=4494/4494
```

The parser consumes the complete decompressed BSP with no missing or extra byte,
which validates the reconstructed `menu.bsp` layout byte-for-byte.

## Runtime structural allocation plan

The real menu map counts are:

- 53 `Node_t`
- 120 `Line_t`
- 44 serialized map sprites
- 68 runtime `Sprite_t` entries after adding 16 custom + 8 drop sprites
- 15 tile events
- 15 bytecodes
- 0 strings

The original `Render_beginLoadMapData()` also creates two temporary ID buffers:

- `mapTextureTexels`: 256 ints = 1,024 bytes
- `mapSpriteTexels`: 1,024 ints = 4,096 bytes

Hardware-calculated allocation payload:

```text
[BSPPLAN] Runtime scratch tex=1024B sprite=4096B
[BSPPLAN] Runtime nodes=2332B lines=3840B sprites=2448B events=60B
[BSPPLAN] Runtime byteCodes=180B stringPtrs=0B stringChars=0B
[BSPPLAN] Structural payload=13980B largestAlloc=4096B whileBSP heap8=40420 largest8=23540
[BSPPLAN] Fit aggregate=yes contiguous=yes (allocator overhead not included)
```

So before bitshape/texel loading, the structural phase needs 13,980 bytes of raw
allocation payload and its largest individual allocation is only 4,096 bytes.
Both aggregate free heap and contiguous-block requirements fit the measured
hardware state with substantial margin.

Allocator overhead is not included in the 13,980-byte estimate, so the next
increment should still measure the real allocation sequence rather than assume
the calculated final heap exactly.

## Heap behavior during full BSP parse

Current post-mappings baseline for this build:

```text
heap8=44932
largest8=23540
```

While the 4,494-byte decompressed BSP is resident:

```text
heap8=40420
largest8=23540
used=4512
```

After freeing it:

```text
[BSPPLAN] Released BSP heap8=44932 largest8=23540 deltaFromStart=0
```

This again proves no leak and no additional visible fragmentation from the
preflight parser.

## Final hardware result

```text
[BSPPLAN] READY complete menu.bsp structure parsed byte-for-byte
[BSPPLAN] Render_beginLoadMapData / bitshapes / texels still NOT executed
[MENUBSP] READY menu.bsp header + complete structure plan validated
```

Heartbeat remains stable after the probe:

```text
[ALIVE] ... heap8=44932 largest8=23540 ... RENDER=ready MAPPINGS=ready MENUBSP=ready ...
```

Touch and the shared-framebuffer present path also remain operational.

This branch therefore passes its hardware merge gate.

## Previously validated persistent mapping budget

`mappings.bin` persistent payload:

- `mediaTexelOffsets`: 2,368 bytes
- `mediaBitShapeOffsets`: 5,200 bytes
- `mediaTexturesIds`: 304 bytes
- `mediaSpriteIds`: 504 bytes
- total payload: 8,376 bytes
- measured heap delta: 8,440 bytes

## Major memory work already validated

- shared 160x120 RGB565 platform / SDL / Render framebuffer: 38,400 bytes total
- desktop duplicate `piDIB` / Render framebuffer allocations removed on ESP32
- packed indexed BMP 1/4/8-bpp storage and zero-copy texture ownership
- approximately 52,000 bytes recovered at the DoomCanvas layout boundary
- miniz 10,992-byte decompressor state moved from loopTask stack to heap
- desktop `mediaPlanes[24][64*64]` removed from ESP32 `Render_t`
- ESP32 plane cells store compact texture references instead

## Current safe stop boundary

Validated and executed:

- core object graph
- `DoomCanvas_startup()` / `Hud_startup()` / `Render_setup()`
- `ParticleSystem_startup()`
- `MenuSystem_startup()`
- `EntityDef_startup()`
- `Render_startup()` through sintable, shared framebuffer and palettes
- `Game_loadConfig()` first-boot path
- `Render_loadMappings()`
- real ZIP load + inflate of `/menu.bsp`
- exact fixed 33-byte BSP header parse
- complete byte-for-byte parse of all remaining `menu.bsp` structural data
- exact structural allocation plan using ESP32 runtime struct sizes

Still intentionally NOT executed:

- real `Render_beginLoadMap()` state retention
- real `Render_beginLoadMapData()` structural allocations
- `Render_loadBitShapes()`
- `Render_loadTexels()` / `wtexels.bin` / `stexels.bin`
- main game loop

## Recommended next increment after merge

Start a new branch from the newly merged `main` and cross the first real
persistent map-data boundary while still stopping before bitshape/texel loading.

Recommended target:

1. retain the real `menu.bsp` buffer as `Render_t::ioBuffer` with the exact header
   state expected after `Render_beginLoadMap()`
2. execute or reproduce only the structural portion of `Render_beginLoadMapData()`:
   scratch ID arrays, nodes, lines, sprites, events, bytecodes, strings, blockmap,
   and plane texture IDs
3. measure actual allocator overhead and the true remaining/largest heap block
4. stop and keep `Render_loadBitShapes()` / `Render_loadTexels()` blocked
5. only after the structural runtime state is hardware-valid should the large
   graphics payloads be preflighted and loaded

Authoritative starting budget for the next increment:

```text
heap8=44932
largest8=23540
menu.bsp usize=4494
structural payload estimate=13980
largest planned allocation=4096
```

## Increment discipline

- Start each new increment from the latest hardware-validated `main`.
- One branch = one small testable objective.
- Fix failures on the same branch; do not create the next branch early.
- Hardware validation on the real CYD is the merge gate.
- Record measured hardware values here after every successful increment.
