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
- Firmware reports heap, PSRAM and engine-structure metrics over serial.

### 2. Touch calibration and orientation

Validated on real CYD hardware and merged to `main` in PR #1.

- XPT2046 raw samples are read through `SoftXpt2046`.
- Touch calibration values live in `board_config.h`.
- Landscape rotation requires the physical touch axes to be transposed:
  - raw Y -> screen X
  - raw X -> screen Y
- Mapping/calibration is isolated in `platform_input.{h,cpp}`.
- Top-left, top-right, bottom-left and bottom-right have been hardware-tested.

### 3. Native 160x120 render target and exact 2x output

Validated on real CYD hardware and merged to `main` in PR #2.

- `platform_video_config.h` defines one canonical geometry:
  - logical 160x120
  - physical 320x240
  - integer scale 2
- `platform_video.{h,cpp}` owns a 160x120 RGB565 framebuffer.
- Framebuffer size is 38,400 bytes.
- `PlatformVideo_present()` expands every logical pixel to exactly 2x2 physical
  pixels with no fractional vertical scaling.
- The diagnostic pattern displayed correctly on hardware.
- SD and `/DoomRPG.zip` stayed ready during the test.
- Touch remained correctly calibrated.
- Heap remained stable at about 274 KiB during a multi-minute idle test after
  video, SD and ZIP initialization.

### 4. SDL compatibility renderer shares platform framebuffer

Validated on real CYD hardware and merged to `main` in PR #3.

- ESP32 `sdlVideo` geometry is 160x120 and comes from
  `platform_video_config.h`.
- `esp32_sdl.cpp` no longer allocates a second logical framebuffer.
- SDL drawing primitives use the same 38,400-byte framebuffer owned by
  `platform_video`.
- `SDL_RenderPresent()` delegates to the exact-2x physical transfer.
- SDL diagnostic drawing, texture copy and alpha blending work on hardware.
- Touch, SD and ZIP remained functional.

## Current increment

Branch: `agent/esp32-engine-core-init`

Goal: instantiate the real Doom RPG engine object graph on the classic ESP32
and measure actual heap consumption, while stopping before resource-heavy
startup.

### What this branch does

`DoomRPG_initEngineCore()` now constructs the same root object graph used by
`DoomRPG_Init()` and stores it in the real global `doomRpg` root:

1. `DoomRPG_t`
2. `DoomCanvas_t`
3. `Render_t`
4. `Menu_t`
5. `MenuSystem_t`
6. `Hud_t`
7. `Sound_t` (ESP32 silent stub)
8. `EntityDef_t`
9. `Game_t`
10. `Player_t`
11. `ParticleSystem_t`
12. `Combat_t`

This is intentionally not a duplicate fake engine. The created objects are the
real engine structures and can be continued by later startup increments.

For every stage the serial log records:

- bytes consumed by that stage;
- remaining 8-bit-capable heap;
- largest remaining contiguous 8-bit heap block.

The final report also records total heap consumed and verifies that
`DoomCanvas_init()` sees the expected 160x120 SDL clip rectangle.

### Important stop boundary

This increment deliberately stops after `Combat_init()`.

It does **not** call:

- `DoomCanvas_startup()`;
- `EntityDef_startup()`;
- `Render_startup()`;
- `Game_loadConfig()`;
- map loading;
- the Doom RPG main loop.

The original full `DoomRPG_Init()` crosses from object construction into
resource startup. In particular `Render_startup()` loads `sintable.bin`, creates
renderer resources and loads palettes. Keeping the barrier here gives a clean
RAM measurement before resource/media work begins.

### Hardware acceptance test

1. Build and upload `agent/esp32-engine-core-init`.
2. Boot should still initialize TFT, VIDEO, SD and ZIP normally.
3. A new `Engine:` line should end in green with a value similar to:
   `OK <bytes>B 160x120`.
4. Serial should print one `[CORE]` line for each of the 12 real engine objects.
5. The final line should resemble:
   `[CORE] READY objects=12 heap used=... remaining=... largest=... clip=160x120`.
6. `CORE=ready` must remain present in heartbeat lines.
7. Leave the CYD running for at least a minute and confirm the post-core heap is
   stable.
8. Touch once: the shared SDL framebuffer diagnostic must still display
   correctly after the engine graph is resident.
9. If initialization fails, keep the complete serial log. The `FAILED at ...`
   stage plus heap/largest-block values identify the first memory blocker.

### Data worth recording from the hardware run

Keep these values in the next merge note / chat result:

- `Engine structs: Render=... Game=... Canvas=... Total=...`
- all 12 `[CORE] ... used=...` lines;
- `[CORE] READY ...` or `[CORE] FAILED ...`;
- first stable `[ALIVE]` heap after core initialization.

## Next engine blocker: true 160x120 startup layout

The original engine still assumes a minimum display height of 128 in
`DoomCanvas_startup()`:

```c
if (doomCanvas->displayRect.h < 0x80) {
    doomCanvas->displayRect.h = 0x80;
}
```

The current core probe is safe because it stops before `DoomCanvas_startup()`.
Before resource startup is allowed, the engine startup layout must become
ESP32-aware so a 120-high clip cannot be expanded to a 128-high display rect.

After hardware validation of the core object graph, the next increment should:

1. make the startup minimum height 120 only under `DOOMRPG_ESP32`;
2. run `DoomCanvas_startup()` and log `clipRect`, `displayRect` and `screenRect`;
3. validate `Render_setup()` allocations at the resulting game viewport size;
4. only then cross into `EntityDef_startup()` / `Render_startup()` and inspect
   the first real resource-loading memory peaks.

## Memory surgery already present in the ESP32 engine

`Render_t` is already partially specialized for `DOOMRPG_ESP32`:

- the desktop `short mediaPlanes[24][64 * 64]` array is absent on ESP32;
- ESP32 stores `planeTexelOffsets[24]` and `planePaletteOffsets[24]` instead;
- `PlaneTextureRef_t` is a one-byte texture index on ESP32 rather than a
  pointer.

This is important: the core-init hardware measurement validates the effect of
that optimization on the real target.

## Larger memory work still pending

- Verify the packed 4-bpp plane texture rendering path on a real map.
- ZIP extraction still creates avoidable compressed/decompressed memory peaks.
  Planned direction: pre-extracted/preconverted SD resources for the first
  playable ESP32 version if ZIP peaks become a blocker.
- `Render_startup()` still creates its own RGB565 render framebuffer in the
  original engine; this should eventually be unified with `platform_video` or
  otherwise removed to avoid framebuffer duplication.
- SDL textures still allocate their own RGB565 pixel storage. Optimize only once
  real resource measurements show which images dominate RAM.
- FluidSynth stays disabled. Sound effects come later and should be streamed or
  loaded on demand rather than preloading the full sound set.

## Increment discipline

- Start every increment from the latest hardware-validated `main`.
- One branch = one small testable objective.
- Do not mix memory surgery, renderer changes and input changes in the same
  branch unless they are inseparable for correctness.
- Hardware validation on the real CYD is the merge gate.
- After merge, update this file with the validated result and the next blocker.
