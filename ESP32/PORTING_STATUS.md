# Doom RPG ESP32 CYD porting status

This file is the recovery point for the ESP32-2432S028R (Cheap Yellow Display)
port. Keep it updated after each hardware-validated increment so work can resume
without relying on chat history.

## Target

- Board: ESP32-2432S028R / classic ESP32 CYD
- Display: ILI9341, 320x240 landscape, TFT_eSPI rotation 1
- Touch: XPT2046 on software SPI
- Storage: microSD on VSPI
- Framework: Arduino through PlatformIO
- Game data: `/DoomRPG.zip` on SD during the bring-up phase
- Internal render target: 160x120 RGB565
- Physical upscale: exact nearest-neighbour 2x to 320x240
- Audio: stubbed/disabled during early milestones

## Validated milestones

### 1. TFT / SD / engine link bring-up

Validated on real CYD hardware.

- TFT initializes at 320x240 landscape.
- RGB test bars display correctly.
- SD card mounts.
- `/DoomRPG.zip` is found and indexed.
- Original Doom RPG engine objects link through the ESP32 compatibility stubs.

### 2. Touch calibration and orientation

Validated on real CYD hardware and merged to `main` in PR #1.

- XPT2046 raw samples are read through `SoftXpt2046`.
- Touch calibration values live in `board_config.h`.
- Landscape rotation requires the physical touch axes to be transposed:
  - raw Y -> screen X
  - raw X -> screen Y
- Mapping/calibration is isolated in `platform_input.{h,cpp}`.
- All four corners have been hardware-tested.

### 3. Native 160x120 render target and exact 2x output

Validated on real CYD hardware and merged to `main` in PR #2.

- Logical framebuffer: 160x120 RGB565 = 38,400 bytes.
- Physical output: exact 2x nearest-neighbour to 320x240.
- No fractional vertical scaling remains.
- SD, ZIP and touch remained functional.
- Heap remained stable during multi-minute idle testing.

### 4. SDL compatibility renderer shares platform framebuffer

Validated on real CYD hardware and merged to `main` in PR #3.

- ESP32 `sdlVideo` geometry comes from `platform_video_config.h`.
- SDL no longer allocates a second logical framebuffer.
- SDL drawing primitives share the 38,400-byte `platform_video` framebuffer.
- `SDL_RenderPresent()` delegates to `PlatformVideo_present()`.
- SDL rectangles, lines, circles, texture copy and alpha blending work on CYD.

### 5. Real Doom RPG core object graph

Validated on real CYD hardware and merged to `main` in PR #4.

The real global `doomRpg` root successfully owns all 12 core objects:

1. `DoomRPG_t`
2. `DoomCanvas_t`
3. `Render_t`
4. `Menu_t`
5. `MenuSystem_t`
6. `Hud_t`
7. `Sound_t` (silent ESP32 stub)
8. `EntityDef_t`
9. `Game_t`
10. `Player_t`
11. `ParticleSystem_t`
12. `Combat_t`

Hardware measurement using `heap_caps_get_free_size(MALLOC_CAP_8BIT)`:

```text
[CORE] Begin real Doom RPG object graph: heap=207728 largest=110580
[CORE] DoomRPG        used=280   heap=207348 largest=110580
[CORE] DoomCanvas     used=3756  heap=203592 largest=110580
[CORE] Render         used=5056  heap=198536 largest=110580
[CORE] Menu           used=68    heap=198468 largest=110580
[CORE] MenuSystem     used=5548  heap=192920 largest=110580
[CORE] Hud            used=616   heap=192304 largest=110580
[CORE] Sound          used=220   heap=192084 largest=110580
[CORE] EntityDef      used=36    heap=192048 largest=110580
[CORE] Game           used=36484 heap=155564 largest=110580
[CORE] Player         used=656   heap=154908 largest=110580
[CORE] ParticleSystem used=2280  heap=152628 largest=110580
[CORE] Combat         used=1052  heap=151576 largest=110580
[CORE] READY objects=12 heap used=56152 remaining=151576 largest=110580 clip=160x120
```

Important observations:

- Total core-object cost is 56,152 bytes of 8-bit-capable heap.
- `Game_t` is by far the largest object allocation at 36,484 bytes because it
  embeds the 400 entities and 100 monster structures.
- Largest contiguous 8-bit block remains 110,580 bytes after all 12 objects.
- `DoomCanvas_init()` correctly sees a 160x120 clip rectangle.
- The shared SDL framebuffer still works with the full core graph resident.
- Measured full-screen exact-2x present time: 34,428 us on the tested CYD.
- Touch, SD and ZIP indexing remain functional after core initialization.

## Current increment

Branch: `agent/esp32-engine-layout-160x120`

Goal: cross the first real engine-startup boundary by running
`DoomCanvas_startup()` at a true 160x120 logical resolution, loading the first
HUD resources, and validating the viewport allocations made by `Render_setup()`.

### ESP32-only 120-pixel startup rule

The original desktop source keeps its historical minimum height of 128 pixels:

```c
if (doomCanvas->displayRect.h < 0x80) {
    doomCanvas->displayRect.h = 0x80;
}
```

The ESP32 PlatformIO build generates a build-directory copy of `DoomCanvas.c`
and changes only that rule to use `DOOMRPG_LOGICAL_HEIGHT` (120). The source-tree
desktop file is not modified.

`DoomCanvas.c` contains legacy non-UTF-8 bytes in comments. The generator reads
and writes it as Latin-1 so each source byte maps 1:1 while the ASCII patch is
applied. The build deliberately fails if the expected upstream code block is no
longer found.

### What `DoomCanvas_startup()` really does

This is the first increment that crosses into actual media loading.
`DoomCanvas_startup()` calls `Hud_startup()`, which loads the first real BMPs
from `DoomRPG.zip`, then calculates the real layout and calls `Render_setup()`.

At 160 pixels wide the normal HUD path needs these files immediately:

- `bar_lg.bmp`
- `k.bmp`
- `n.bmp`
- `o.bmp`
- `l.bmp`
- `m.bmp`

`Render_setup()` then allocates:

- `ceilingColor[screenWidth]` (`short`);
- `floorColor[screenWidth]` (`short`);
- `columnScale[screenWidth]` (`int`).

### Hardware run: incomplete game-data archive discovered

The first hardware run of this branch reached the real resource boundary and
reset-looped before layout calculation:

```text
[DATA] DoomRPG.zip indexed, entries=5
...
[LAYOUT] Begin DoomCanvas_startup: heap8=151448 largest8=110580
[LAYOUT] This stage loads the first real HUD BMP resources
[DOOM ERROR] DoomRPG Error: did not find the bar_lg.bmp file in the zip file
abort() was called ...
Rebooting...
```

This was confirmed to be a data-package problem, not a 160x120 geometry failure.
The file named `/DoomRPG.zip` on the SD card was the outer BREW distribution
archive, not the converted resource archive consumed by DoomRPG-RE.

### Resource preflight: hardware validated

A resource preflight was added after the reset-loop failure. It checks all six
initial HUD files before calling `DoomCanvas_startup()`.

This guard is now validated on real CYD hardware. With the same SD card the board
stays alive, reports every missing HUD resource, prints the complete ZIP central
directory and skips layout startup safely.

The tested archive contains exactly these five entries:

```text
[DATA] DoomRPG.zip indexed, entries=5
[DATA] MISSING required HUD resource: bar_lg.bmp
[DATA] MISSING required HUD resource: k.bmp
[DATA] MISSING required HUD resource: n.bmp
[DATA] MISSING required HUD resource: o.bmp
[DATA] MISSING required HUD resource: l.bmp
[DATA] MISSING required HUD resource: m.bmp
[DATA] ZIP directory (5 entries):
[DATA]   [0] DOOM.RPG.BREW.PATCH.txt csize=51 usize=51
[DATA]   [1] DOOM.RPG.BREW.PATCH.zip csize=11104 usize=11509
[DATA]   [2] doomrpg.bar csize=484737 usize=548869
[DATA]   [3] doomrpg.mif csize=775 usize=1090
[DATA]   [4] doomrpg.mod csize=76780 usize=156016
[DATA] HUD resource preflight FAILED
[DATA] Expected a DoomRPG.zip generated from the original doomrpg.bar with BarToZip
```

The important discovery is that the required `doomrpg.bar` is already present
inside the tested five-entry archive. The next hardware test therefore only
requires extracting that BAR on a development machine, running the DoomRPG-RE
`BarToZip` converter, and copying the generated resource `DoomRPG.zip` to the SD
card root.

After the preflight failure, the core remains stable and startup exits cleanly:

```text
[CORE] READY objects=12 heap used=56152 remaining=151448 largest=110580 clip=160x120
[LAYOUT] Prerequisite unavailable; probe skipped safely
[LAYOUT] DoomRPG.zip does not contain the HUD resources required by Hud_startup()
[READY] Bring-up remains alive; touch still runs the SDL video test.
```

The preflight therefore successfully prevents the old `DoomRPG_Error()` /
reboot loop when game data is incomplete or the wrong archive is supplied.

With a correct generated archive, expect:

```text
[DATA] HUD resource preflight OK
=== Doom RPG 160x120 layout + HUD startup probe ===
[LAYOUT] Begin DoomCanvas_startup: heap8=... largest8=...
...
[LAYOUT] clip    x=... y=... w=160 h=120
[LAYOUT] display x=... y=... w=160 h=...
[LAYOUT] screen  x=... y=... w=160 h=...
[LAYOUT] HUD top=... bottom=... Render=160x... arrays=...B
[LAYOUT] heap8 used=... remaining=... largest=...
[LAYOUT] READY real engine layout fits inside 160x120
```

The exact gameplay viewport height depends on the real HUD bitmap heights and is
therefore intentionally measured on hardware instead of hard-coded.

### Validation rules

The probe rejects the layout unless all of these remain true:

- `clipRect` is exactly 160x120;
- `displayRect` remains inside the clip rectangle;
- `screenRect` remains inside `displayRect`;
- gameplay viewport dimensions are non-zero;
- `Render_t::screenWidth/screenHeight` exactly match `screenRect`;
- `floorColor`, `ceilingColor` and `columnScale` were allocated successfully.

### Heap metrics

Heartbeat diagnostics print both metrics explicitly:

- `heap=` -> Arduino `ESP.getFreeHeap()`;
- `heap8=` -> `heap_caps_get_free_size(MALLOC_CAP_8BIT)`;
- `largest8=` -> largest contiguous MALLOC_CAP_8BIT block.

### Compiler warnings currently tracked

The generated `DoomCanvas.c` exposes legacy const-correctness warnings such as
passing string literals to functions declared with `char *`. These warnings are
not related to the reset and are not being hidden with compiler flags. They
should be fixed in a dedicated const-correctness pass by changing read-only text
parameters to `const char *` consistently in declarations and definitions.

TFT_eSPI also warns that `TOUCH_CS` is not defined. This is expected because the
port does not use TFT_eSPI's touch implementation; XPT2046 input is handled by
the separate `SoftXpt2046` / `PlatformInput` path.

### Hardware acceptance test

1. Keep `agent/esp32-engine-layout-160x120` unmerged until layout itself passes.
2. Extract `doomrpg.bar` from the current five-entry BREW distribution archive.
3. Run the DoomRPG-RE `BarToZip` converter to generate the resource
   `DoomRPG.zip`.
4. Copy that generated file to the SD card root as `/DoomRPG.zip`.
5. Reboot and confirm `[DATA] HUD resource preflight OK`.
6. Core init must still reach `READY objects=12`.
7. Keep the complete `[LAYOUT]` block.
8. `clip` must be 160x120 and `[LAYOUT] READY` must appear.
9. Record HUD top/bottom heights, final `Render=160x...`, heap8 used, remaining
   heap8 and largest8.
10. Touch once after startup; the shared SDL framebuffer diagnostic must still
    render correctly with HUD resources resident.

## Current safe stop boundary

This branch still does **not** execute:

- `ParticleSystem_startup()`;
- `MenuSystem_startup()`;
- `EntityDef_startup()`;
- `Render_startup()`;
- `Game_loadConfig()`;
- map loading;
- main game loop.

In particular, `Render_startup()` remains behind the barrier because it loads
`sintable.bin`, creates the original engine render texture/framebuffer and loads
`palettes.bin`.

## Likely next increment after layout validation

If the 160x120 HUD/layout probe passes, cross the next resource boundary in
small steps:

1. `ParticleSystem_startup()` and `MenuSystem_startup()`;
2. `EntityDef_startup()` with heap measurements;
3. `Render_startup()` with detailed measurements around `sintable.bin`, the
   engine framebuffer/texture and `palettes.bin`;
4. decide whether the original `Render_startup()` framebuffer can be removed or
   shared before attempting the first BSP/map load.

## Memory surgery already present in the ESP32 engine

`Render_t` is already specialized under `DOOMRPG_ESP32`:

- desktop `short mediaPlanes[24][64 * 64]` is absent;
- ESP32 stores `planeTexelOffsets[24]` and `planePaletteOffsets[24]`;
- `PlaneTextureRef_t` is a one-byte texture index instead of a pointer.

This optimization is a major reason the real core object graph fits comfortably.

## Larger memory work still pending

- Validate the packed 4-bpp plane texture path on a real map.
- Inspect ZIP compressed/decompressed peak memory during real resource loads.
- `Render_startup()` still creates its own RGB565 render framebuffer; eventually
  unify or remove it to avoid framebuffer duplication.
- SDL textures still own RGB565 pixel buffers; optimize after measurements show
  which assets dominate RAM.
- FluidSynth remains disabled. Add sound effects later, streamed or on-demand.

## Increment discipline

- Start every increment from the latest hardware-validated `main`.
- One branch = one small testable objective.
- Do not mix unrelated renderer, resource, input and memory surgery.
- Hardware validation on the real CYD is the merge gate.
- Record the real hardware values in this file after every successful increment.
