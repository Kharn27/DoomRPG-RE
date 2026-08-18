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

Branch: `agent/esp32-render-startup-shared-framebuffer`

Status: **HARDWARE VALIDATED, READY TO MERGE**.

Objective: execute the real render-resource startup boundary while removing the
desktop-only duplicate RGB565 storage that cannot fit on a classic CYD.

Executed and validated in this increment:

1. load `/sintable.bin`
2. initialize the render clip rectangle
3. make `Render_t::framebuffer` alias the existing 160x120 platform framebuffer
4. keep desktop `piDIB` absent on ESP32
5. load `/palettes.bin`
6. validate framebuffer identity, pitch, palette table and remaining heap

`Game_loadConfig()`, mappings and BSP/map loading remain intentionally blocked.

## Shared Render framebuffer

The desktop implementation of `Render_startup()` allocates both:

- an RGB565 streaming `piDIB` texture with its own 160x120 pixel storage
- a second 160x120 RGB565 `render->framebuffer`

Each pixel buffer is 38,400 bytes. After the previous validated startup stage,
the largest contiguous MALLOC_CAP_8BIT block was only 36,852 bytes, so even one
new 38,400-byte allocation was structurally impossible.

For the ESP32 build, linker wrapping redirects `Render_startup()` and
`Render_free()` to a small CYD bridge while leaving the upstream `src/Render.c`
source untouched. The bridge:

- loads the original `sintable.bin`
- keeps the original render clip geometry
- sets `render->piDIB = NULL`
- sets `render->framebuffer` to the existing `PlatformVideo_framebuffer()`
- uses the real RGB565 pitch of 320 bytes
- runs the original `Render_loadPalettes()`
- prevents `Render_free()` from freeing the platform-owned framebuffer

The legacy engine bridge itself is compiled as C. A tiny C++ adapter exposes
only the platform framebuffer pointer/size to it, avoiding C++ compilation of
legacy `DoomRPG.h` (`typedef enum { false, true } boolean`).

## Final hardware Render_startup result

Hardware resource preflight:

```text
[RENDERSTART] Resource preflight (2 files)
[RENDERSTART] sintable.bin   c=622 u=1024
[RENDERSTART] palettes.bin   c=4532 u=6564
[RENDERSTART] Resource preflight OK
```

Startup begins with the previous validated memory state:

```text
[RENDERSTART] Begin: heap8=60416 largest8=36852 platformFB=0x3ffc421c bytes=38400
```

Real engine resources load successfully:

```text
[ZIP] read sintable.bin method=8 c=622 u=1024
[ZIP] inflate sintable.bin c=622 u=1024 state=10992
[RENDER] sintable loaded: 1024 bytes

[RENDER] Shared framebuffer 160x120 pitch=320 bytes=38400 ptr=0x3ffc421c

[ZIP] read palettes.bin method=8 c=4532 u=6564
[ZIP] inflate palettes.bin c=4532 u=6564 state=10992
[RENDER] palettes loaded: entries=3280 bytes=6560
```

The decisive hardware proof is that the platform framebuffer and the Doom
renderer framebuffer are exactly the same address:

```text
platformFB=0x3ffc421c
ptr=0x3ffc421c
```

No second RGB565 framebuffer or `piDIB` pixel buffer is allocated.

Final render startup metrics:

```text
[RENDERSTART] Render_startup result=1 used=6576 heap8=53840 largest8=36852
[RENDERSTART] READY shared framebuffer, sintable and palettes initialized
[RENDERSTART] fb=0x3ffc421c pitch=320 paletteEntries=3280 paletteBytes=6560
[RENDERSTART] Game_loadConfig / mappings / BSP still NOT executed
```

Measured persistent cost of this stage:

- total: 6,576 bytes
- palette table: 3,280 entries / 6,560 bytes
- remaining MALLOC_CAP_8BIT heap: 53,840 bytes
- largest contiguous block: 36,852 bytes
- shared framebuffer storage added by this stage: **0 bytes**

## Regression checks

After render startup the board remains alive and the existing shared-framebuffer
touch diagnostic still works:

```text
[TOUCH] raw=1788,2752 pressure=1633 screen=222,98
[SDL] Sharing platform framebuffer: 38400 bytes
[VIDEO] Present 160x120 -> 320x240 exact 2x: 34430 us
[SDL] Touch-triggered shared-framebuffer test is on screen
```

Heartbeat remains stable for at least 20 seconds:

```text
[ALIVE] ... heap8=53840 largest8=36852 ... CORE=ready LAYOUT=ready PRERENDER=ready RENDER=ready ...
```

This branch therefore passes its hardware merge gate.

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

It is the reason later startup resources can coexist on the no-PSRAM CYD.

## Other memory work already validated

- correct generated resource ZIP: 241 entries
- miniz `tinfl_decompressor` state (10,992 bytes) moved from `loopTask` stack to heap
- indexed BMP 1/4/8-bpp decoder
- zero-copy ownership transfer from BMP surface to SDL texture
- native packed indexed texture storage
- desktop DoomCanvas 128-pixel minimum specialized to 120 only for ESP32
- desktop `mediaPlanes[24][64*64]` removed from ESP32 `Render_t`
- SDL, platform video and now `Render_t` all share the same 38,400-byte logical framebuffer

## Current safe stop boundary

Validated and executed:

- core object graph
- `DoomCanvas_startup()` / `Hud_startup()` / `Render_setup()`
- `ParticleSystem_startup()`
- `MenuSystem_startup()`
- `EntityDef_startup()`
- `Render_startup()` through sintable, shared framebuffer and palettes

Still intentionally NOT executed:

- `Game_loadConfig()`
- `Render_loadMappings()`
- BSP/map loading
- main game loop

## Recommended next increment after merge

Start a new branch from the newly merged `main` and cross the next startup
boundary in similarly small measured steps:

1. inspect and execute `Game_loadConfig()` with resource preflight and heap metrics
2. inspect `Render_loadMappings()` and measure its persistent tables
3. stop before loading `menu.bsp` or a gameplay BSP if mapping allocation becomes
   the next memory boundary
4. only after mappings are hardware-valid should the first BSP/map load be attempted

The current post-render memory state is:

```text
heap8=53840
largest8=36852
```

Treat these values as the starting budget for the next increment.

## Increment discipline

- Start each new increment from the latest hardware-validated `main`.
- One branch = one small testable objective.
- Fix failures on the same branch; do not create the next branch early.
- Hardware validation on the real CYD is the merge gate.
- Record measured hardware values here after every successful increment.
