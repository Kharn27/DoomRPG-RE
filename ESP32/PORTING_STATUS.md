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

## Current increment

Branch: `agent/esp32-engine-video-bridge`

Goal: make the ESP32 SDL compatibility renderer consume the already validated
160x120 platform framebuffer instead of owning a second framebuffer.

Changes:

- `engine_stubs.c` now takes `sdlVideo` and all ESP32 video modes from
  `platform_video_config.h` instead of hard-coding 160x128.
- The SDL shim logical geometry is now 160x120.
- `esp32_sdl.cpp` no longer allocates a renderer framebuffer with `calloc()`.
- `ensureRendererPixels()` obtains the buffer from
  `PlatformVideo_framebuffer()`.
- `SDL_RenderPresent()` delegates the physical transfer to
  `PlatformVideo_present()`.
- First touch now renders the SDL compatibility test pattern, not the direct
  platform-video pattern. This exercises SDL clear, fill, line, circle,
  texture copy, alpha blending and present through the shared framebuffer.
- The original Doom RPG main loop is still not started in this increment.

### Expected memory effect

Before this branch, the bring-up could own both:

- `platform_video`: 160x120 RGB565 = 38,400 bytes;
- SDL shim: 160x128 RGB565 = 40,960 bytes.

After this branch, there is only the 38,400-byte platform framebuffer for the
logical screen. Individual SDL textures still allocate their own pixel storage
and will be optimized later.

### Hardware acceptance test for this increment

1. Build and upload `agent/esp32-engine-video-bridge`.
2. Boot must still report TFT, SD, ZIP and VIDEO ready.
3. Before the first touch, note the idle heap value.
4. On first touch, serial should include:
   - `[SDL] Sharing platform framebuffer: 38400 bytes`
   - `[VIDEO] Present 160x120 -> 320x240 exact 2x: ... us`
   - `[SDL] Touch-triggered shared-framebuffer test is on screen`
5. The SDL test pattern should fill the screen without corruption or vertical
   distortion.
6. The lower translucent strip must still be visible; it ends exactly on the
   160x120 framebuffer boundary.
7. Touch coordinates must continue to land at the correct physical positions.
8. Heap should not drop by another ~41 KiB when the SDL test is first shown.
   A small temporary change while the 16x16 checker texture is created is fine,
   but it is destroyed before the test returns.
9. Leave the board running for at least a minute and confirm heap remains stable.

## Known engine blocker before starting Doom RPG

The original engine still assumes a minimum view height of 128 in
`DoomCanvas_startup()`:

```c
if (doomCanvas->displayRect.h < 0x80) {
    doomCanvas->displayRect.h = 0x80;
}
```

The ESP32 stubs now advertise 160x120, but the real engine is deliberately not
started yet. Before calling `DoomRPG_Init()` / entering the game loop, this clamp
must become ESP32-aware; otherwise `displayRect.h` becomes 128 inside a 120-high
clip rectangle.

The next increment after hardware validation of the SDL bridge is therefore:

1. adapt the `DoomCanvas_startup()` minimum-height rule for `DOOMRPG_ESP32`;
2. add layout diagnostics/assertions for `clipRect`, `displayRect` and
   `screenRect`;
3. initialize the Doom RPG object graph without loading a real map yet;
4. stop at the first safe engine/menu screen or before resource-heavy map
   rendering, depending on what initialization reveals;
5. record heap before and after each major engine object allocation.

## Larger memory work still pending

These are intentionally not part of the current hardware bring-up increments.

- `Render_t::mediaPlanes` currently expands up to 24 64x64 textures to RGB565.
  Planned direction: retain source 4-bpp texels and palette at render time.
- `planeTextures[2048]` is pointer-heavy on ESP32. Planned direction: compact
  texture indices instead of pointers where practical.
- ZIP extraction currently creates avoidable compressed/decompressed memory
  peaks. Planned direction: pre-extracted/preconverted SD resources for the
  first playable ESP32 version.
- SDL still backs much of the 2D UI/image API. Planned direction: route it
  through explicit platform video/image/file/input/time layers incrementally.
- FluidSynth stays disabled. Sound effects come later and should be streamed or
  loaded on demand rather than preloading the full sound set.

## Increment discipline

- Start every increment from the latest hardware-validated `main`.
- One branch = one small testable objective.
- Do not mix memory surgery, renderer changes and input changes in the same
  branch unless they are inseparable for correctness.
- Hardware validation on the real CYD is the merge gate.
- After merge, update this file with the validated result and the next blocker.
