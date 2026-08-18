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

The real core graph costs 56,152 bytes of MALLOC_CAP_8BIT heap. `Game_t` is the
largest individual core allocation at 36,484 bytes.

## Current validated increment

Branch: `agent/esp32-engine-layout-160x120`

Status: **HARDWARE VALIDATED, READY TO MERGE**.

Objective: cross the first real engine startup boundary by running
`DoomCanvas_startup()` with the real resource archive, loading HUD/font/legal
BMP assets, calculating the real 160x120 layout and executing `Render_setup()`.

### Correct resource archive

The generated `/DoomRPG.zip` is now confirmed correct on hardware:

```text
[DATA] DoomRPG.zip indexed, entries=241
[DATA] HUD resource preflight OK
```

The earlier five-entry archive was the outer BREW distribution package and was
not suitable for direct engine use. The preflight remains intentionally present
to prevent `DoomRPG_Error()` reset loops when required resources are missing.

### ZIP inflate stack-overflow fix

The first real HUD load previously overflowed Arduino `loopTask`.

Hardware diagnostics proved ZIP entries are DEFLATE (`method=8`) and that
`tinfl_decompressor` is 10,992 bytes:

```text
[ZIP] read bar_lg.bmp method=8 c=294 u=456
[ZIP] inflate bar_lg.bmp c=294 u=456 state=10992
```

The ESP32 ZIP path now allocates the miniz decompressor state on heap and invokes
`tinfl_decompress()` directly instead of using `tinfl_decompress_mem_to_mem()`,
which placed the decompressor object on the task stack. The stack overflow is
hardware-confirmed fixed.

### Indexed BMP support

Original Doom RPG mobile assets are indexed BMPs, mostly 4-bpp BI_RGB.
Examples observed on hardware:

```text
bar_lg.bmp      20x28   4-bpp
k.bmp           20x20   4-bpp
n.bmp           18x54   4-bpp
l.bmp           18x180  4-bpp
g.bmp          128x512  4-bpp
a.bmp          144x72   4-bpp
larger_font.bmp 208x102 4-bpp
b.bmp            6x48   4-bpp
```

`ESP32/src/esp32_bmp.cpp` supports uncompressed indexed 1/4/8-bpp BMPs and
expands packed rows to the engine's internal 8-bpp indexed SDL surface format.

### Zero-copy indexed SDL textures

The original ESP32 SDL shim converted every indexed surface to a second RGB565
texture. This is impossible for `g.bmp`: 128x512 RGB565 would require a single
131,072-byte allocation, while the classic CYD's largest contiguous block was
110,580 bytes even before the HUD startup.

The ESP32 SDL texture path therefore keeps BMP-derived textures indexed:

- texture adopts the SDL surface pixel buffer;
- texture adopts the palette;
- surface relinquishes ownership before it is freed;
- RGB565 conversion happens only when sampling in `SDL_RenderCopy()`;
- `SDL_DestroyTexture()` owns/frees the adopted pixel buffer and palette.

Hardware proof for the largest startup asset:

```text
[BMP] decode 128x512 4-bpp -> 8-bpp indexed colors=8 row=64 out=65536
[SDL] Adopt indexed texture 128x512 pixels=65536 palette=8
```

This removes the impossible 131,072-byte RGB565 copy and keeps `g.bmp` at about
65 KB of indexed pixels plus its tiny palette.

### ESP32 120-pixel DoomCanvas rule

The desktop source historically enforces a minimum display height of 128. The
PlatformIO ESP32 build generates a Latin-1-safe build-directory copy of
`DoomCanvas.c` and replaces that minimum only for ESP32 with
`DOOMRPG_LOGICAL_HEIGHT` (120). The desktop source remains untouched.

### Final hardware layout result

The complete real startup stage now succeeds:

```text
[LAYOUT] clip    x=0 y=0 w=160 h=120
[LAYOUT] display x=0 y=0 w=160 h=120
[LAYOUT] screen  x=0 y=20 w=160 h=80
[LAYOUT] HUD top=20 bottom=20 Render=160x80 arrays=1280B
[LAYOUT] heap8 used=106708 remaining=34416 largest=14324
[LAYOUT] READY real engine layout fits inside 160x120
[LAYOUT] EntityDef_startup / Render_startup still NOT executed
[LAYOUT] Summary ready=yes used=106708 heap8=34416 largest8=14324 display=160x120 render=160x80
```

Validated geometry:

- clip: 160x120
- display: 160x120
- top HUD: 20 px
- bottom HUD: 20 px
- gameplay viewport: 160x80
- `Render_t` viewport: 160x80
- `Render_setup()` arrays: 1,280 bytes total

### Touch / framebuffer regression check

After all startup resources were resident, the board stayed alive and touch still
triggered the shared SDL framebuffer diagnostic:

```text
[READY] Bring-up remains alive; touch still runs the SDL video test.
[TOUCH] raw=2419,2163 pressure=2149 screen=168,142
[SDL] Sharing platform framebuffer: 38400 bytes
[VIDEO] Present 160x120 -> 320x240 exact 2x: 34436 us
[SDL] Touch-triggered shared-framebuffer test is on screen
[ALIVE] uptime=10004 ms heap=100448 heap8=34416 largest8=14324 SD=ready ZIP=ready VIDEO=ready CORE=ready LAYOUT=ready
```

The current milestone therefore passes its hardware merge gate.

## Critical memory boundary after this milestone

After `DoomCanvas_startup()`:

- free MALLOC_CAP_8BIT heap: 34,416 bytes
- largest contiguous MALLOC_CAP_8BIT block: 14,324 bytes

This is the main constraint for the next increment.

**Do not call the current unmodified `Render_startup()` yet.** It creates its own
160x120 RGB565 framebuffer, which requires 38,400 contiguous bytes. With a
largest free block of only 14,324 bytes this allocation is structurally
impossible, even though aggregate free heap is larger.

The next render-stage work must first remove/share/replace that duplicate
framebuffer allocation or otherwise redesign the startup memory lifetime.

## Current safe stop boundary

This validated branch intentionally stops before:

- `ParticleSystem_startup()`
- `MenuSystem_startup()`
- `EntityDef_startup()`
- `Render_startup()`
- `Game_loadConfig()`
- BSP/map loading
- main game loop

`DoomCanvas_startup()` and its internal `Render_setup()` are validated; the later
`Render_startup()` is a distinct and still-blocked stage.

## Recommended next increment after merge

Start a new branch from the newly merged `main` and advance in small measured
steps:

1. `ParticleSystem_startup()` + `MenuSystem_startup()` with before/after heap8.
2. `EntityDef_startup()` with resource/memory measurements.
3. Enter `Render_startup()` piecewise rather than as one call:
   - load/measure `sintable.bin`;
   - inspect texture/framebuffer creation;
   - replace or share the 38,400-byte duplicate render framebuffer before it is
     allocated;
   - load/measure `palettes.bin`.
4. Only after those pass, attempt the first BSP/map load.

## Memory surgery already present

`Render_t` is specialized under `DOOMRPG_ESP32`:

- desktop `short mediaPlanes[24][64 * 64]` is absent;
- ESP32 stores compact plane offsets/references;
- `PlaneTextureRef_t` is one byte;
- SDL shares the platform 38,400-byte logical framebuffer;
- BMP textures remain indexed instead of owning RGB565 copies;
- miniz DEFLATE state lives on heap instead of task stack.

These changes are the main reasons the original engine now reaches a real
160x120 startup layout on a classic CYD without PSRAM.

## Increment discipline

- Start each new increment from the latest hardware-validated `main`.
- One branch = one small testable objective.
- Fix failures on the same branch; do not create the next branch early.
- Hardware validation on the real CYD is the merge gate.
- Record measured hardware values here after every successful increment.
