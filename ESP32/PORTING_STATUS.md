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
    allocation plan (`agent/esp32-menu-bsp-structure-plan`, PR #10).
12. Real `Render_beginLoadMap(MAP_MENU)` + real structural portion of
    `Render_beginLoadMapData()`, stopped exactly before graphics resources
    (`agent/esp32-menu-map-runtime-structures`, PR #11, merge commit
    `12e64c464e8dcfc477515a38955de116c69a8730`).

## Current validated increment

Branch: `agent/esp32-menu-resource-memory-plan`

Status: **HARDWARE VALIDATED, READY TO MERGE**.

Objective: measure the graphics-resource memory boundary implied by the real
menu-map reference lists without executing `Render_loadBitShapes()` or
`Render_loadTexels()` and without deliberately triggering OOM.

The real runtime map structures remain resident while this probe executes.

## Authoritative post-structure baseline

Hardware state entering the resource probe:

```text
[RESOURCEPLAN] Begin heap8=30736 largest8=21492 refs textures=84 sprites=284 planes=11
```

So the current graphics stage starts with:

- free MALLOC_CAP_8BIT heap: 30,736 bytes
- largest contiguous block: 21,492 bytes
- wall/floor texture refs: 84
- sprite/bitshape refs: 284
- plane textures: 11

## Real graphics resource sizes

ZIP metadata measured on the real SD archive:

```text
[RESOURCEPLAN] bitshapes.bin c=21708 u=62273 inflateTransient=94973B
[RESOURCEPLAN] wtexels.bin   c=53235 u=116740 inflateTransient=180967B
[RESOURCEPLAN] stexels.bin   c=96145 u=126618 inflateTransient=233755B
```

The current ESP32 ZIP path is a whole-file loader. Its deflate transient set is:

```text
compressed payload + decompressed payload + 10,992-byte miniz state
```

Therefore none of these three resources can currently be whole-file inflated
with the post-structure heap budget. `bitshapes.bin` alone needs a 62,273-byte
contiguous output buffer and roughly 94,973 bytes aggregate during inflation.

This is a loader-architecture boundary, not a failure of the BSP/game-state
runtime structures.

## Proven mediaTexels wall

The original `Render_loadTexels()` computes the wall-texture part of
`render->mediaTexels` as:

```text
mapTextureTexelsCount * 64 * 64 / 2
```

The real menu map has 84 referenced textures, therefore:

```text
84 * 64 * 64 / 2 = 172032 bytes
```

Hardware probe output:

```text
[RESOURCEPLAN] Render_loadTexels wall payload=172032B (84 x 2048B)
[RESOURCEPLAN] Render_loadTexels sprite-size scratch=1136B
[RESOURCEPLAN] mediaTexels lowerBound=172032B before ANY sprite texels
[RESOURCEPLAN] TEXEL WALL proven: lowerBound exceeds largest8 by 150540B
```

This is a decisive result: the original monolithic `mediaTexels` allocation is
mathematically impossible on this no-PSRAM CYD even before adding a single
sprite texel.

At this point:

```text
mediaTexels wall-only lower bound = 172032 B
largest contiguous heap block     =  21492 B
shortfall                          = 150540 B
```

Adding sprite texels would only increase the required allocation.

## Bitshape preflight result

The probe deliberately does not call the original bitshape loader because the
current whole-file ZIP loader cannot safely produce the 62,273-byte decompressed
`bitshapes.bin` buffer:

```text
[RESOURCEPLAN] BITSHAPE PREFLIGHT blocked: current whole-file loader cannot safely inflate bitshapes.bin
[RESOURCEPLAN] Exact shapeData/sprite-texel contribution intentionally not inspected
```

This does NOT yet prove that the final selected `shapeData` itself is too large.
It proves that the current implementation cannot get to the sizing pass because
it first materializes the entire `bitshapes.bin` file in RAM.

A streaming/ranged resource reader is therefore required before the exact
selected bitshape payload can be measured safely.

## Original Render_loadTexels scratch leak

The source allocates:

```text
mapSpriteTexelsCount * sizeof(int)
= 284 * 4
= 1136 bytes
```

for its sprite-size scratch array and does not free that temporary buffer before
successful return.

Probe output:

```text
[RESOURCEPLAN] NOTE original Render_loadTexels() allocates 1136B sprite-size scratch and does not free it
```

Do not fix this in isolation yet; the entire ESP32 texel loader must be redesigned
anyway. The future ESP32 path should not carry this leak forward.

## Final hardware result

```text
[RESOURCEPLAN] Current texel allocation fit aggregate=NO contiguous=NO (wall lower bound alone is sufficient)
[RESOURCEPLAN] READY resource budget measured; heavy graphics loaders remain blocked
[RESOURCEPLAN] Render_loadBitShapes / Render_loadTexels still NOT executed
[MAPSTRUCT] Resource memory plan complete; heavy graphics loaders remain blocked
```

Heartbeat remains stable with the real map runtime structures resident:

```text
[ALIVE] ... heap=96552 heap8=30736 largest8=21492 ... MENUBSP=ready ...
```

Touch and shared-framebuffer presentation also remain operational.

This branch therefore passes its hardware merge gate.

## Feasibility conclusion at this boundary

The classic no-PSRAM CYD is still a viable target, but the port can no longer
follow DoomRPG-RE's original graphics loading model.

Already proven to fit and run on the real board:

- full core object graph
- HUD/layout resources
- prerender/menu/entity resources
- shared 160x120 framebuffer
- palettes and sine table
- mappings
- complete menu BSP parsing
- real nodes/lines/sprites/events/bytecodes map structures

The blocker is specifically the original graphics resource strategy:

1. whole-file inflate of 60-126 KB resources
2. one monolithic `mediaTexels` buffer of at least 172 KB for this map

Both assumptions must be replaced on ESP32.

The likely ESP32 architecture is:

- stream/range-decompress resource data instead of materializing complete BIN
  files
- keep mapping/index metadata resident
- retain only the bitshape metadata actually required by the current map
- fetch/decode wall/sprite packed 4-bpp texels on demand or through a small
  bounded cache
- keep plane texture references as already-specialized compact IDs
- never create the desktop/mobile monolithic `mediaTexels` pool

Doom RPG is turn-based and renders a 160x80 gameplay viewport, which makes a
small texture cache / on-demand SD-backed resource path much more plausible than
for a high-frame-rate conventional Doom renderer.

Audio remains out of scope during this memory-critical bring-up. It is optional
for eventual functionality and should only be reconsidered after the full menu
map and game loop are stable.

## Current safe stop boundary

Validated and executed:

- complete engine startup through mappings
- real `Render_beginLoadMap(MAP_MENU)`
- real structural `Render_beginLoadMapData()` phase
- real nodes, lines, sprites, events, bytecodes and resource reference lists
- resource-memory diagnostic against real map references

Still intentionally NOT executed:

- real `Render_loadBitShapes()`
- real `Render_loadTexels()`
- whole-file `bitshapes.bin`, `wtexels.bin`, `stexels.bin` loading
- monolithic `mediaTexels`
- completion of map graphics loading
- game entities/player spawning
- main game loop

## Recommended next increment after merge

Do NOT attempt either original graphics loader.

Start a new branch from the newly merged `main` and attack the resource-reader
architecture first.

Recommended next objective:

1. inspect ZIP reader internals and the ZIP entry format already indexed on SD
2. design an ESP32-only streaming/ranged reader for a compressed entry that does
   not allocate the entire uncompressed output
3. prove it first on `bitshapes.bin` with a tiny fixed output window
4. use that reader to walk only the referenced 284 bitshape records and calculate
   exact selected `shapeData` + sprite texel payload without whole-file inflate
5. keep all current real runtime map structures resident during the test
6. continue to leave texel loading blocked

Authoritative starting budget:

```text
heap8=30736
largest8=21492
bitshapes.bin c=21708 u=62273
wtexels.bin c=53235 u=116740
stexels.bin c=96145 u=126618
wall mediaTexels lower bound=172032
```

## Increment discipline

- Start each new increment from the latest hardware-validated `main`.
- One branch = one small testable objective.
- Fix failures on the same branch; do not create the next branch early.
- Hardware validation on the real CYD is the merge gate.
- Record measured hardware values here after every successful increment.
