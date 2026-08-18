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
- Planned internal render target: 160x120 RGB565
- Planned physical upscale: exact nearest-neighbour 2x to 320x240
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

## Current increment

Branch: `agent/esp32-160x120-render-target`

Goal: validate the future game framebuffer independently from the SDL shim.

Changes:

- `platform_video_config.h` defines one canonical video geometry:
  - logical 160x120
  - physical 320x240
  - integer scale 2
- `platform_video.{h,cpp}` owns a 160x120 RGB565 framebuffer.
- Framebuffer size is 38,400 bytes.
- `PlatformVideo_present()` expands every logical pixel to exactly 2x2 physical
  pixels. There is no fractional vertical scaling.
- First touch displays a diagnostic test pattern through this new path.
- The existing ESP32 SDL shim is intentionally still present and unchanged.

### Hardware acceptance test for this increment

1. Build and upload the branch.
2. Boot must still show the normal bring-up screen.
3. `Video:` must report `160x120 x2`.
4. SD and ZIP diagnostics must remain unchanged.
5. Touch a corner and confirm touch calibration still behaves as before.
6. On the first touch, the 160x120 test pattern must fill the 320x240 panel.
7. The four logical 80x60 quadrants must appear as equal 160x120 physical
   quadrants.
8. The checker cells are 10x10 logical pixels and therefore must look like
   square 20x20 physical cells.
9. Serial should contain a line similar to:
   `Present 160x120 -> 320x240 exact 2x`.

## Known engine blocker before starting Doom RPG

The original engine still assumes a minimum view height of 128 in
`DoomCanvas_startup()`:

```c
if (doomCanvas->displayRect.h < 0x80) {
    doomCanvas->displayRect.h = 0x80;
}
```

That code must be made ESP32-aware before the engine is allowed to consume the
160x120 mode. Do not simply set the current `sdlVideo` stub to 120 while this
clamp remains active: it would produce a 128-high display rectangle inside a
120-high clip rectangle.

The next video integration increment should therefore do these together:

1. make the ESP32 engine mode 160x120;
2. adapt the `DoomCanvas_startup()` minimum-height rule only for
   `DOOMRPG_ESP32`;
3. make the SDL compatibility renderer use the shared `platform_video`
   framebuffer instead of maintaining a separate 160x128 framebuffer;
4. preserve the desktop SDL backend unchanged;
5. validate the engine layout before loading a real map.

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
