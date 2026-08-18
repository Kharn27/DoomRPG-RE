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
7. `ParticleSystem_startup()`, `MenuSystem_startup()` and
   `EntityDef_startup()` with packed indexed BMP storage
   (`agent/esp32-pre-render-startup`, PR #6).
8. Real `Render_startup()` using the platform-owned shared framebuffer, plus
   `sintable.bin` and `palettes.bin`
   (`agent/esp32-render-startup-shared-framebuffer`, PR #7).
9. `Game_loadConfig()` first-boot path + real `Render_loadMappings()`
   (`agent/esp32-config-mappings-startup`, PR #8, merge commit
   `6188a80aaae9fa91678cc58f1ff4b4d06fab1db8`).

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

Branch: `agent/esp32-menu-bsp-preflight`

Status: **HARDWARE VALIDATED, READY TO MERGE**.

Objective: make the first real contact with `/menu.bsp` without yet entering
`Render_beginLoadMap()` / `Render_beginLoadMapData()` or allocating runtime BSP
structures.

Executed and validated in this increment:

1. locate `/menu.bsp` in the real 241-entry DoomRPG archive
2. preflight compressed/uncompressed sizes and current contiguous heap
3. account for the ESP32 deflate transient set: compressed payload + decompressed
   payload + 10,992-byte heap-resident miniz `tinfl_decompressor`
4. call the real `DoomRPG_fileOpenRead(doomRpg, "/menu.bsp")`
5. keep the complete decompressed BSP resident temporarily
6. parse the exact 33-byte fixed header used by `Render_beginLoadMap()`
7. free the BSP immediately and verify exact heap recovery
8. leave all map runtime allocations blocked

No engine source file is patched by this increment.

## menu.bsp hardware result

Archive entry:

```text
[MENUBSP] menu.bsp c=1401 u=4494 header=33B
```

Current post-mappings budget at the start of this probe:

```text
[MENUBSP] Begin heap8=44940 largest8=23540 transientPayload=16887B
```

The calculated 16,887-byte transient payload is:

- compressed ZIP payload: 1,401 bytes
- decompressed `menu.bsp`: 4,494 bytes
- miniz inflate state: 10,992 bytes

The real ZIP/decompression path succeeds:

```text
[MENUBSP] -> DoomRPG_fileOpenRead(/menu.bsp)
[ZIP] read menu.bsp method=8 c=1401 u=4494
[ZIP] inflate menu.bsp c=1401 u=4494 state=10992
[MENUBSP] BSP resident ptr=0x3fff6b4c heap8=40428 largest8=23540 used=4512
```

The decompressed payload itself is 4,494 bytes. The measured heap delta while it
is resident is 4,512 bytes, i.e. 18 bytes of allocator/bookkeeping overhead.

## Parsed fixed BSP header

The 33-byte header matches the layout consumed by `Render_beginLoadMap()`:

```text
[MENUBSP] Header name='DoomRPG' floorRGB=68,68,68 ceilRGB=136,136,136
[MENUBSP] Header floorTex=117 ceilingTex=151 introRGB=0,0,0
[MENUBSP] Header loadMapID=0 spawn=460 dir=0 cameraSpawn=0 pos=33/33
```

Decoded values:

- map name: `DoomRPG`
- floor color RGB: 68,68,68
- ceiling color RGB: 136,136,136
- floor texture ID: 117
- ceiling texture ID: 151
- intro color RGB: 0,0,0
- `loadMapID`: 0
- spawn index: 460
- spawn direction: 0
- camera spawn index: 0
- fixed header cursor after parse: exactly 33 bytes

## Heap recovery and regression checks

After freeing the temporary BSP buffer:

```text
[MENUBSP] Released BSP heap8=44940 largest8=23540 deltaFromStart=0
[MENUBSP] READY menu.bsp read + fixed header parsed
[MENUBSP] Render_beginLoadMap / Render_beginLoadMapData still NOT executed
```

This proves:

- no persistent cost for this probe
- no leak
- no additional fragmentation visible in the largest MALLOC_CAP_8BIT block
- exact recovery to the post-mappings baseline

Heartbeat stays stable:

```text
[ALIVE] ... heap8=44940 largest8=23540 ... RENDER=ready MAPPINGS=ready MENUBSP=ready ...
```

Touch and the shared framebuffer diagnostic still work:

```text
[TOUCH] raw=1921,2192 pressure=1914 screen=171,107
[SDL] Sharing platform framebuffer: 38400 bytes
[VIDEO] Present 160x120 -> 320x240 exact 2x: 34427 us
[SDL] Touch-triggered shared-framebuffer test is on screen
```

This branch therefore passes its hardware merge gate.

## Previously validated mapping budget

`mappings.bin` header:

```text
texelOffsets=592
bitShapeOffsets=1300
textures=152
sprites=252
```

Persistent mapping payload:

- `mediaTexelOffsets`: 2,368 bytes
- `mediaBitShapeOffsets`: 5,200 bytes
- `mediaTexturesIds`: 304 bytes
- `mediaSpriteIds`: 504 bytes
- total payload: 8,376 bytes
- measured heap delta: 8,440 bytes

Post-mappings hardware baseline before this increment is approximately:

```text
heap8=44948
largest8=23540
```

The current build differs by only a few bytes of static/runtime bookkeeping;
this increment starts and returns to:

```text
heap8=44940
largest8=23540
```

## Shared Render framebuffer already validated

The ESP32 build wraps `Render_startup()` / `Render_free()` without modifying the
upstream `src/Render.c`. `Render_t::framebuffer` aliases the platform-owned
160x120 RGB565 framebuffer, while desktop `piDIB` stays absent.

The renderer therefore does not allocate either of the desktop-only 38,400-byte
RGB565 duplicates.

## Packed indexed BMP storage already validated

The ESP32 image path keeps original indexed assets in native packed form:

- 1-bpp: 8 pixels per byte
- 4-bpp: 2 pixels per byte
- 8-bpp: 1 pixel per byte
- BMP row padding removed
- SDL texture adopts pixels and palette zero-copy
- palette index is unpacked only when sampled during `SDL_RenderCopy()`

This recovered exactly 52,000 bytes at the DoomCanvas layout boundary:

```text
before: heap8=34408 largest8=14324
after:  heap8=86408 largest8=47092
```

## Other memory work already validated

- correct generated resource ZIP: 241 entries
- miniz `tinfl_decompressor` state (10,992 bytes) moved from `loopTask` stack to heap
- indexed BMP 1/4/8-bpp decoder
- zero-copy ownership transfer from BMP surface to SDL texture
- native packed indexed texture storage
- desktop DoomCanvas 128-pixel minimum specialized to 120 only for ESP32
- desktop `mediaPlanes[24][64*64]` removed from ESP32 `Render_t`
- SDL, platform video and `Render_t` all share the same 38,400-byte logical framebuffer

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
- fixed 33-byte `menu.bsp` header parse

Still intentionally NOT executed:

- `Render_beginLoadMap()`
- `Render_beginLoadMapData()`
- persistent BSP nodes / lines / sprites / events / strings
- bitshape / texel loading for the map
- main game loop

## Recommended next increment after merge

Start a new branch from the newly merged `main` and inspect the remainder of the
`menu.bsp` format consumed by `Render_beginLoadMapData()` before allocating it.

Recommended order:

1. parse the map-data counts from a temporary decompressed `menu.bsp` without
   retaining runtime structures
2. calculate exact ESP32 allocation sizes using `sizeof(Node_t)`, `sizeof(Line_t)`,
   `sizeof(Sprite_t)` and the event/bytecode/string counts found in this real BSP
3. include the fact that the 4,494-byte BSP `ioBuffer` remains resident while
   `Render_beginLoadMapData()` creates those structures
4. identify the largest single allocation and total persistent payload
5. only call the first real map-data allocation stage if that measured plan fits
   the current contiguous heap
6. continue to stop before wall texels / bitshapes if those become the next memory
   boundary

Authoritative starting budget for that increment:

```text
heap8=44940
largest8=23540
menu.bsp usize=4494
```

## Increment discipline

- Start each new increment from the latest hardware-validated `main`.
- One branch = one small testable objective.
- Fix failures on the same branch; do not create the next branch early.
- Hardware validation on the real CYD is the merge gate.
- Record measured hardware values here after every successful increment.
