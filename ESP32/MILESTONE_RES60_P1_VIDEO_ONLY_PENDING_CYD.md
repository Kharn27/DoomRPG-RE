# RES60-P1 — 192x144 LCD feasibility pilot (CYD Serial + visual PASS)

- Issue: [#263](https://github.com/Kharn27/DoomRPG-RE/issues/263)
- Original GitHub base: `main=1e9c1268c30c1051836be6fb066e6d927064415e`
- Rebased integration base (19 Nov 2026): `main=fe85909babd686bb2041cc74dadf69c514edb519`; replayed cumulative P1 source onto main; prior hardware proof applies only to original code
- Branch: `agent/esp32-res60-p1`
- Status: **real classic-CYD Serial + photographic visual PASS** for video allocation, LCD scanout and diagnostic image geometry. This is **not** a 60% playable game.

## Scope: genuine 192x144 LCD diagnostic, NOT a playable 60% port

All production gameplay remains **160x120, viewport 160x80**:
`esp32-cyd` and `esp-cyd-inverted` do not change their resolution, HUD,
touch or LCD 2x presentation.

`esp32-cyd-res60` is an isolated **video-only** hardware experiment.
It allocates exactly one 192x144 RGB565 framebuffer (**55,296 bytes**, versus
38,400 baseline; delta **+16,896 bytes**), fills it with genuinely rendered
192x144 pixels (quadrants, native one-pixel checker, border, diagonals), then
presents on the 320x240 panel through a **640-byte scanline** using

`sourceX=floor(physicalX*192/320)` and
`sourceY=floor(physicalY*144/240)`.

There is NO 160x120 game image to enlarge. There is no intermediate 320x240
buffer, no PSRAM, no ZIP runtime, and no additional framebuffer. The
single display frame is presented exactly once, on demand.

The pilot **returns immediately after VIDEO** before starting SD, PAK,
Render, HUD, menu, BSP, gameplay, SFX and music; `loop()` stays idle. This
is essential: none of the current 160-stride engine code may accidentally
paint a 192-stride framebuffer. A real 192x96 world renderer is **not yet
implemented or tested**. No claims about gameplay, audio headroom, map loading
or world/sprites/HUD performance can follow from P1.

## Architecture audit and work remaining

- `platform_video_config.h`: production compile-time contracts 160x120,
  20-row HUD, 160x80 world and 2x integer scaling. These stay enforced.
- `esp_native_gameplay_hud.c`: hard 160x120 preprocessor assertion,
  fixed 20px bitmap bands and positions.
- `esp_native_sprite_renderer.c`, `esp_native_bsp_visibility.c`,
  `esp_native_first_frame.c`: 160 column scratch and/or 80-row viewport
  bounds, screen projection and scratch-size assertion.
- `esp_native_plane_renderer.c` and `esp_native_gameplay_frame.c`:
  fixed 160x80 checks, FNV layout and HUD/world coordinates.
- `native_main_menu_touch_layout.c` and
  `native_main_menu_160x120_layout.h`: full 160x120/touch modal geometry.
- Graphics compositors, weapons, effects, animations, dialog, status/menus
  and touch inverse mapping still require dedicated 192x144 contracts.

The NEXT milestone must migrate an entire coherent real 192x96 world path
plus matching 24+24 HUD/touch rules, never silently bypass 160x120 assertions
or invent new 192x144 game FNV goldens.

## Host and CI

`python ESP32/test/res60_p1_geometry_host.py` compiles C11 for both profiles:
all 320 physical columns and 240 rows, exact floor mapping and coverage,
integer 2x production parity, invalid extents, guards. The CI also attempts
the genuine `pio run -e esp32-cyd-res60` firmware build, alongside the four
unchanged production/diagnostic builds. Build result must be checked before
hardware deployment.

## First real classic CYD proof — 17 November 2026

User ran `platformio run --target upload --target monitor --environment esp32-cyd-res60` on actual **ESP32-D0WD-V3** (240 MHz, 4 MB flash, **no PSRAM**) and pasted full build, flash verification and boot-to-idle Serial output into the project conversation on 17 November 2026. The flashed firmware is identified by profile, not by an embedded Git SHA in the log; PR #364 code HEAD before this docs-only report was `a59e7cb9b788abd65d68925ae82359f6a27d9379`. **Do not claim a cryptographically witnessed flashed SHA.**

Hardware evidence (raw Serial values, bytes and microseconds):

```text
Heap8: 277092 free, largest block 114676
PSRAM: 0 bytes
[VIDEO] RES60-P1 VIDEO-ONLY framebuffer 192x144 RGB565, 55296 bytes
[VIDEO] Physical output 320x240 fractional nearest 5/3; gameplay=NOT_STARTED
[RES60P1] BOOT mode=VIDEO_ONLY logical=192x144 rgb565=55296 viewport3d=NOT_STARTED HUD=NOT_STARTED MENU=NOT_STARTED AUDIO=NOT_STARTED heap8=221524 largest8=110580 minHeap8=216088
[RES60P1] RASTER src=192x144 us=2717
[RES60P1] PRESENT src=192x144 dst=320x240 method=floor-nearest lineBytes=640 us=36867
[RES60P1] FRAME totalUs=49979
[RES60P1] IDLE mode=VIDEO_ONLY heap8=221524 largest8=110580 minHeap8=215988 audio=NOT_STARTED map=NOT_LOADED
```

- **Single 55,296-B framebuffer allocation succeeded.** Measured free heap8 reduction from initial system-info read to VIDEO-only BOOT: **55,568 B** (framebuffer 55,296 B, other net reduction 272 B). This is a boot-before/after comparison, not a controlled allocator-only delta.
- **Heap8 available after VIDEO** 221,524 B (about 216.3 KiB), largest contiguous free block **110,580 B** (about 108 KiB), minimum heap8 observed by idle **215,988 B**. Before video largest was 114,676 B. These are a **cold LCD-only boot**; they do not imply safe headroom under maps, render caches or audio.
- Native 192x144 diagnostic raster **2,717 µs**, fractional nearest-neighbour scanout **36,867 µs**, `FRAME totalUs=49,979` measured around both passes **and their intervening Serial logs**. The difference **10,395 µs** is outside the two timed subphases and must not be attributed to game-frame computation.
- `lineBytes=640` matches a single 320px RGB565 output row. The Serial log confirms the scanout routine completed and reached the idle gate. A **real photo of the CYD was provided on 18 November 2026**, validating the visible full-panel geometry and pattern (see bounded visual report below); the image alone cannot certify calibrated gamma/RGB accuracy.
- Link and upload both succeeded, flash writes hash-verified, no panic/assertion/reboot reported before `[RES60P1] IDLE`. Normal and alternate builds previously compiled under double-success GitHub Actions [push 43990568450](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43990568450) / [PR 43990577555](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43990577555), including the P1 firmware.
- **Not exercised:** renderer, walls, sprites, HUD, touch, gameplay, maps, audio ring/SD SFX, min-RAM during actual play, `shapeData/mediaTexels` runtime pointer assertions. AUDIO=NOT_STARTED and no world paths are intentional; **0 underflows cannot be inferred from absence of audio logs**.

## Real CYD photograph — 18 November 2026: bounded visual PASS

The user supplied a photograph of the actual classic CYD displaying the RES60-P1 diagnostic pattern in the project conversation on 18 November 2026. Inspection of the photograph confirms all four correctly positioned visible quadrants (cyan upper left, magenta upper right, yellow lower left, blue lower right), intersecting corner-to-corner diagonals, a discernible central high-frequency 1px checker field, and rectangular coverage of the full landscape LCD without obvious crop, displaced origin or missing edge. **Color correction after comparing the P2 photograph and source:** the P1 *source* quadrants were authored as red upper left, green upper right, blue lower left and yellow lower right, not the visible cyan/magenta/yellow/blue. The observed P1 palette is consistent with an RGB565 bitwise complement (red→cyan, green→magenta, blue→yellow, yellow→blue), independently repeated in P2. The P1 photo was a layout/scanout PASS, never a color-fidelity PASS.

Fine moiré/line texture in the photo is **not, by itself, evidence of an LCD scaling fault**: photographing subpixel structure and high-frequency patterns may alias, and fractionally resampling 192 source columns to 320 physical columns inherently distributes source pixels over one or two physical columns (same for rows). A photograph is **not** a pixel-for-pixel capture or a colorimeter; do not assert quantitatively exact RGB/gamma, or gameplay rendering. No new firmware flash was required for this visual proof.

**Visual verdict: PASS for gross orientation, visible checker/diagonals, quadrant geometry and unclipped composition on real hardware; color fidelity differs from coded RGB565 and the classic panel's INVON behavior requires separate A/B validation. Pixel-level/gamma calibration was not measured.** This closes the P1 photo gate, not the real 192x96 world/HUD/touch/audio milestones.

**Verdict: RES60-P1 CYD Serial + photographic visual PASS for native 192x144 framebuffer allocation, LCD scanout and diagnostic geometry; playable 60% feasibility / audio safety UNSUPPORTED.** Keep PR experimental and unmerged pending integration review against newer `main`. The production release remains 160x120.

## Hardware test (same classic CYD)

From `ESP32/`, boot baseline first if comparing:

```sh
pio run -e esp32-cyd -t upload
pio device monitor -b 115200
# Then reset/flash the isolated RES60 video pilot:
pio run -e esp32-cyd-res60 -t upload
pio device monitor -b 115200
```

Capture the full reset-to-idle Serial log and one photo of the pilot image.
Expected bounded markers: `[RES60P1] BOOT`, `RASTER`, `PRESENT`,
`FRAME`, `IDLE`, including `heap8`, `largest8`, `minHeap8`
and real presentation microseconds. There should be **no engine/map/audio
startup**. An unavailable contiguous framebuffer must fail closed.

**One real CYD Serial test and one real CYD display photo received and documented above. Do not call this a 60% gameplay or audio PASS, and do not merge without explicit approval.**
