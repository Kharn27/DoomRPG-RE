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
   (`agent/esp32-engine-layout-160x120`, PR #5, merge commit
   `9a49f032e1dbe77b10c0d2d4cbec2a9c22ce8897`).

The real core graph costs 56,152 bytes of MALLOC_CAP_8BIT heap. `Game_t` is the
largest individual core allocation at 36,484 bytes.

The validated DoomCanvas geometry is:

- clip: 160x120
- display: 160x120
- top HUD: 20 px
- bottom HUD: 20 px
- gameplay viewport: 160x80
- `Render_t` viewport: 160x80
- `Render_setup()` arrays: 1,280 bytes total

## Current validated increment

Branch: `agent/esp32-pre-render-startup`

Status: **HARDWARE VALIDATED, READY TO MERGE**.

Objective: cross the remaining startup stages immediately before
`Render_startup()` without yet entering the render-resource initialization:

1. `ParticleSystem_startup()`
2. `MenuSystem_startup()`
3. `EntityDef_startup()`

`Render_startup()` and `Game_loadConfig()` remain intentionally blocked.

## Packed indexed BMP storage

The first hardware run of this branch exposed another contiguous-allocation
boundary at `gibs_24.bmp`:

```text
[PRERENDER] Begin: heap8=34408 largest8=14324
[PRERENDER] -> ParticleSystem_startup()
[BMP] gibs_24.bmp ... w=96 h=192 bpp=4
[BMP] ERROR out of memory loading BMP
```

The old ESP32 BMP loader expanded every 1/4-bpp indexed image to one byte per
pixel. For `gibs_24.bmp`, that required a single 18,432-byte allocation while the
largest available block was only 14,324 bytes.

The ESP32 path now keeps indexed images at their native packed depth in memory:

- 1-bpp: 8 pixels per byte
- 4-bpp: 2 pixels per byte
- 8-bpp: 1 pixel per byte
- BMP row padding is removed, but packed pixel order is preserved
- SDL textures adopt the packed surface buffer and palette zero-copy
- `SDL_RenderCopy()` extracts the palette index directly from the packed byte
  and converts only the sampled pixel to RGB565

The generated ESP32 SDL texture stores the indexed bit depth so 1/4/8-bpp
textures can all share the same render path.

Hardware examples:

```text
[BMP] decode 128x512 4-bpp packed colors=8 fileRow=64 memRow=64 out=32768
[SDL] Adopt packed indexed texture 128x512 bpp=4 bytes=32768 palette=8

[BMP] decode 208x102 4-bpp packed colors=3 fileRow=104 memRow=104 out=10608
[SDL] Adopt packed indexed texture 208x102 bpp=4 bytes=10608 palette=3

[BMP] decode 96x192 4-bpp packed colors=14 fileRow=48 memRow=48 out=9216
[SDL] Adopt packed indexed texture 96x192 bpp=4 bytes=9216 palette=14
```

The berserk version of `gibs_24.bmp` is compatible with packed storage because
`DoomRPG_createImageBerserkColor()` recolors the palette rather than rewriting
pixel indices.

## Memory improvement at the existing layout boundary

Before native packed BMP storage, the validated DoomCanvas stage ended at:

```text
heap8=34408 largest8=14324
```

With packed 4-bpp storage, the same hardware stage now ends at:

```text
[LAYOUT] heap8 used=54708 remaining=86408 largest=47092
[LAYOUT] READY real engine layout fits inside 160x120
```

That recovers exactly 52,000 bytes of persistent MALLOC_CAP_8BIT heap at this
boundary and increases the largest contiguous block from 14,324 to 47,092 bytes.

The geometry remains unchanged at 160x120 / 160x80, proving the memory change is
representation-only and does not alter layout behavior.

## Pre-render resource preflight

Before executing the new startup stages, the ESP32 probe validates these archive
entries:

```text
gibs_24.bmp    c=2902 u=9328
p.bmp          c=106  u=156
q.bmp          c=96   u=136
j.bmp          c=2757 u=4264
entities.db    c=1216 u=2762
```

The tested `/DoomRPG.zip` contains all five resources and the preflight passes.

## Final hardware pre-render result

The complete new stage succeeds on the real classic CYD:

```text
[PRERENDER] Begin: heap8=86408 largest8=47092

[PRERENDER] -> ParticleSystem_startup()
[SDL] Adopt packed indexed texture 96x192 bpp=4 bytes=9216 palette=14
[SDL] Adopt packed indexed texture 96x192 bpp=4 bytes=9216 palette=14
[PRERENDER] ParticleSystem_startup   used=18712 heap8=67696 largest8=36852

[PRERENDER] -> MenuSystem_startup()
[SDL] Adopt packed indexed texture 13x10 bpp=4 bytes=70 palette=5
[SDL] Adopt packed indexed texture 7x14 bpp=4 bytes=56 palette=6
[SDL] Adopt packed indexed texture 108x74 bpp=4 bytes=3996 palette=16
[PRERENDER] MenuSystem_startup       used=4496 heap8=63200 largest8=36852

[PRERENDER] -> EntityDef_startup()
[ZIP] read entities.db method=8 c=1216 u=2762
[ZIP] inflate entities.db c=1216 u=2762 state=10992
[PRERENDER] EntityDef_startup        used=2776 heap8=60424 largest8=36852
[PRERENDER] Entity defs=115 table=2760B

[PRERENDER] READY total used=25984 heap8=60424 largest8=36852
[PRERENDER] Render_startup / Game_loadConfig still NOT executed
```

Measured persistent costs:

- `ParticleSystem_startup()`: 18,712 bytes
- `MenuSystem_startup()`: 4,496 bytes
- `EntityDef_startup()`: 2,776 bytes
- total pre-render stage: 25,984 bytes
- remaining MALLOC_CAP_8BIT heap: 60,424 bytes
- largest contiguous block: 36,852 bytes
- entity definitions: 115
- final `EntityDef_t` table: 2,760 bytes

The board remains alive after the stage, and touch still triggers the shared
SDL framebuffer diagnostic successfully:

```text
[ALIVE] ... heap8=60424 largest8=36852 ... CORE=ready LAYOUT=ready
[SDL] Sharing platform framebuffer: 38400 bytes
[VIDEO] Present 160x120 -> 320x240 exact 2x: 34514 us
[SDL] Touch-triggered shared-framebuffer test is on screen
```

This branch therefore passes its hardware merge gate.

## Exact next memory boundary: Render_startup

The original `Render_startup()` currently does the following after loading
`sintable.bin`:

```c
w = sdlVideo.rendererW;
h = sdlVideo.rendererH;
render->piDIB = SDL_CreateTexture(... RGB565 ..., w, h);
render->pitch = (((w * 2) + 3) & ~3);
render->framebuffer = SDL_calloc(1, render->pitch * h);
Render_loadPalettes(render);
```

On this ESP32 port `rendererW/H` are 160x120, so this attempts two separate
38,400-byte RGB565 pixel allocations:

1. `SDL_CreateTexture()` -> texture-owned RGB565 pixel buffer: 38,400 bytes
2. `render->framebuffer` -> second RGB565 buffer: 38,400 bytes

After the validated pre-render stage the largest contiguous block is only
36,852 bytes, so even the **first** 38,400-byte allocation is structurally
impossible. Calling the unmodified `Render_startup()` would therefore provide no
new information and should remain blocked.

The platform already owns the one 38,400-byte framebuffer required for 160x120
output, and SDL already shares it. The next increment must make `Render_t` share
that existing framebuffer and eliminate the duplicate `piDIB` / framebuffer
storage rather than trying to allocate either buffer again.

## Other memory work already validated

- correct generated resource ZIP: 241 entries
- miniz `tinfl_decompressor` state (10,992 bytes) moved from `loopTask` stack to heap
- indexed BMP 1/4/8-bpp decoder
- zero-copy ownership transfer from BMP surface to SDL texture
- native packed indexed texture storage
- desktop DoomCanvas 128-pixel minimum specialized to 120 only for ESP32
- desktop `mediaPlanes[24][64*64]` removed from ESP32 `Render_t`
- SDL and platform video already share the platform 38,400-byte logical framebuffer

## Current safe stop boundary

Validated and executed:

- core object graph
- `DoomCanvas_startup()` / `Hud_startup()` / `Render_setup()`
- `ParticleSystem_startup()`
- `MenuSystem_startup()`
- `EntityDef_startup()`

Still intentionally NOT executed:

- `Render_startup()`
- `Game_loadConfig()`
- BSP/map loading
- main game loop

## Recommended next increment after merge

Start a new branch from the newly merged `main` and enter `Render_startup()`
piecewise:

1. load and validate `sintable.bin` with before/after heap metrics
2. make `Render_t::framebuffer` alias the existing platform framebuffer instead
   of allocating another 38,400 bytes
3. avoid the second RGB565 pixel allocation currently hidden inside `piDIB`, or
   replace `piDIB` with a non-owning/shared presentation path
4. load `palettes.bin` and measure the persistent palette table
5. stop before map/BSP loading

Only after that render-resource stage passes hardware should the port attempt its
first map load.

## Increment discipline

- Start each new increment from the latest hardware-validated `main`.
- One branch = one small testable objective.
- Fix failures on the same branch; do not create the next branch early.
- Hardware validation on the real CYD is the merge gate.
- Record measured hardware values here after every successful increment.
