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

Validated on real CYD hardware on branch `agent/esp32-engine-core-init`.
Ready to merge after this documentation commit.

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
- Touch, SD and ZIP remain functional after core initialization.

The heartbeat currently prints `ESP.getFreeHeap()` while the core probe uses
`heap_caps_get_free_size(MALLOC_CAP_8BIT)`. Those values are deliberately not
compared directly; a later increment should report both metrics consistently.

## Current safe stop boundary

The engine graph is resident, but resource startup is still intentionally not
executed. The following remain behind the barrier:

- `DoomCanvas_startup()`
- `ParticleSystem_startup()`
- `MenuSystem_startup()`
- `EntityDef_startup()`
- `Render_startup()`
- `Game_loadConfig()`
- map loading
- main game loop

This barrier is important because `Render_startup()` starts loading real files
such as `sintable.bin` and palettes and also creates renderer resources.

## Next increment: real 160x120 engine layout

The next branch must address the original 128-pixel minimum height before
calling `DoomCanvas_startup()`.

Desktop behavior currently contains:

```c
if (doomCanvas->displayRect.h < 0x80) {
    doomCanvas->displayRect.h = 0x80;
}
```

For ESP32 only, the minimum must permit the canonical 120-pixel logical height.
The next increment should:

1. make the minimum display height 120 only under `DOOMRPG_ESP32`;
2. run `DoomCanvas_startup()` on the already resident real object graph;
3. log `clipRect`, `displayRect`, `screenRect`, `SCR_CX` and `SCR_CY`;
4. validate `Render_setup()` allocations at the resulting viewport size;
5. keep the barrier before `EntityDef_startup()` / `Render_startup()` until the
   layout and allocations have been hardware-tested;
6. print both `ESP.getFreeHeap()` and MALLOC_CAP_8BIT heap metrics in diagnostics.

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
