# RES60-P2 — 192x144 diagnostic layout 24/96/24 (CYD Serial + geometry photo PASS; panel colors differ)

- Issue: [#263](https://github.com/Kharn27/DoomRPG-RE/issues/263)
- PR: [#364](https://github.com/Kharn27/DoomRPG-RE/pull/364), still draft / independent parallel branch.
- Branch: `agent/esp32-res60-p1`.
- Integration starting main: `fe85909babd686bb2041cc74dadf69c514edb519`. The P1 net changes were replayed against it as one consolidated commit `bd62c36caa3d2b2e39d1a078976b9942caf283dc`, preserving ARCH-RC9+ session owners; this was a squashed rebase, not a merge into main.
- Status: **double GitHub Actions SUCCESS + real classic-CYD Serial PASS + P2 photographic geometry PASS** for the video-only 24/96/24 raster, FNV and LCD scanout through idle. **Strict displayed palette is NOT a PASS:** colors on the photographed classic panel closely correspond to the bitwise complements of source RGB565 colors. P1's earlier Serial/photo PASS refers to its original 192x144 checker, not P2.

## Strict bounded scope

`esp32-cyd-res60` remains the **sole experimental video-only** profile. It now explicitly sets `DOOMRPG_RES60_P2_LAYOUT=1` in addition to `DOOMRPG_RES60_P1_DIAGNOSTIC=1` and inherits all standard `esp32-cyd` build settings. Normal and inverted production builds retain their original `160x120` framebuffer, `160x80` world, 20+20 HUD, strict guards and integer 2x LCD presentation.

The new allocation-free C-compatible header `ESP32/include/esp_res60_layout.h` encodes an independent native **192x144 / 24+96+24** surface contract:

- Upper HUD *region*: logical y=0..23, physical y=0..39.
- World *region*: logical y=24..119, dimensions **192x96**, physical y=40..199.
- Lower HUD *region*: logical y=120..143, physical y=200..239.
- One **55,296-byte RGB565** framebuffer and the existing **640-byte LCD scanline**. No extra frame buffers, no PAK/ZIP access, no PSRAM, no dynamic surface allocations.
- Raster fills all pixels in native 192-pixel stride, with solid red/gold HUD diagnostic bands, cyan world region, a perspective-like pair of synthetic crossed diagonals, a white center cross and explicit 1px white boundaries. **These are calibration pixels, not Doom RPG walls, HUD assets, gameplay, or a fake enlarged 160x80 world.**
- All invalid pixel coordinates are rejected by `EspRes60Layout_regionAt`; `EspRes60Layout_paintDiagnostic` fails before any write if framebuffer pointer/footprint is invalid. `EspRes60Layout_fnv` independently fingerprints the RGB565 pixels in little-endian byte order.

The P2 boot never enters SD, PAK, menu, map, sound, touch or game rendering. `main.cpp` routes only the gated experiment to `PlatformVideo_showRes60LayoutPattern()` and idles after one presentation, leaving the prior P1 `PlatformVideo_showTestPattern()` path in source behind the P2 gate. A preprocessor error forbids using P2 without P1's strict video-only admission. The P1 LCD presenter continues to upscale actual 192x144 content to 320x240 using nearest source coordinates, not a 160x120 game image.

**P2 does not exercise real wall/plane/sprite projection or HUD art**, map loading, render caches, touch, monster turns, SD or music. No conclusion about 60% playable frame rate or audio headroom follows. The real viewport path is a later, coherent renderer milestone.

## Host / CI requirements before hardware

`python ESP32/test/res60_p2_layout_host.py` compiles strict C11 with `cc -std=c11 -O2 -Wall -Wextra -Werror` for the normal (P1=0, P2=0) and isolated diagnostic (P1=1, P2=1) flags. It checks:

- Exact region bounds (24+96+24), all 192x144 logical coordinates and invalid coordinates.
- Framebuffer exact byte size, fail-closed null/wrong-size inputs, two out-of-bounds canaries.
- Stable algorithmic host FNV witness **`9efd2d18`**; **the real CYD Serial log of 19 Nov 2026 independently reports the identical value**.
- Boundaries and center marker pixels, every physical row's nearest 5/3 source mapping, 40/160/40 LCD-band allocation.
- Compile-time rejection of an unsafe P2=1 / P1=0 combination, and preservation of the 160x120/160x80 gameplay constants.

GitHub Actions ran the host guard, original P1 test, historical profiles, and the real `esp32-cyd-res60` firmware: **double SUCCESS for code HEAD `d9af8fd9ca28f0217a848be9242fbba87464acb4`**, [push run 44419896310](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44419896310) and [PR run 44419906575](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44419906575).

## Repro / remaining visual test — normal 192x144 video-only profile

From `ESP32/`:

```sh
pio run -e esp32-cyd-res60 -t upload
pio device monitor -b 115200
```

Expected distinctive Serial markers:

```text
[RES60P1] BOOT mode=VIDEO_ONLY ... logical=192x144 ...
[RES60P2] BOOT mode=LAYOUT_ONLY logical=192x144 hudTop=24 viewport=192x96@y24 hudBottom=24 engine=NOT_STARTED touch=NOT_STARTED audio=NOT_STARTED
[RES60P2] RASTER src=192x144 top=24 world=192x96@y24 bottom=24 framebufferBytes=55296 fnv=9efd2d18 ...
[RES60P1] PRESENT src=192x144 dst=320x240 method=floor-nearest lineBytes=640 ...
[RES60P2] FRAME complete ... world3d=NOT_STARTED hudArt=NOT_STARTED map=NOT_LOADED audio=NOT_STARTED
[RES60P1] IDLE ... audio=NOT_STARTED map=NOT_LOADED
```

Reset-to-idle **Serial log and P2 screen photo have both been received**, as documented below. The P2 photo verifies orientation, rectangular bands, grid and lack of obvious cropping; it also reveals **displayed RGB565 colors inverted relative to the coded pattern**. The P1 checker photo from 18 Nov remains historical evidence for the prior firmware. Neither image proves true 3D, production colors or 60% gameplay readiness.

## First real CYD P2 Serial proof — 19 November 2026

Source: user-uploaded `platformio-device-monitor-261119-003038.log`, **1,959 bytes, 33 physical lines, SHA256 `483218633fcf109a92376ec56d1e2f5501be1fae4ec64c839a5b7af0e6e64107`**. Real classic CYD `ESP32-D0WD-V3`, dual core 240 MHz, 4 MiB flash, **PSRAM=0**. The log contains power-on reset, VIDEO-only startup, all P2 checkpoints and IDLE, without a captured panic or watchdog reset. This capture has **no upload transcript or embedded Git SHA**; the source code's double CI is established independently, but the flashed commit SHA is not asserted to be cryptographically proven from Serial.

```text
Heap8: 277092 free, largest block 114676
PSRAM: 0 bytes
[VIDEO] RES60-P1 VIDEO-ONLY framebuffer 192x144 RGB565, 55296 bytes
[RES60P1] BOOT mode=VIDEO_ONLY logical=192x144 rgb565=55296 viewport3d=NOT_STARTED HUD=NOT_STARTED MENU=NOT_STARTED AUDIO=NOT_STARTED heap8=221524 largest8=110580 minHeap8=216088
[RES60P2] BOOT mode=LAYOUT_ONLY logical=192x144 hudTop=24 viewport=192x96@y24 hudBottom=24 engine=NOT_STARTED touch=NOT_STARTED audio=NOT_STARTED
[RES60P2] RASTER src=192x144 top=24 world=192x96@y24 bottom=24 framebufferBytes=55296 fnv=9efd2d18 rasterUs=5560
[RES60P1] PRESENT src=192x144 dst=320x240 method=floor-nearest lineBytes=640 us=36859
[RES60P2] FRAME complete totalUs=44417 world3d=NOT_STARTED hudArt=NOT_STARTED map=NOT_LOADED audio=NOT_STARTED
[RES60P1] IDLE mode=VIDEO_ONLY heap8=221524 largest8=110580 minHeap8=215988 audio=NOT_STARTED map=NOT_LOADED
```

Measured evidence:

- **Geometry exactly reported**: native 192×144 framebuffer (55,296 B), logical 24+96+24 rows, viewport at y=24, and output 320×240 via fractional nearest with a **640-B** output scanline. This proves the implemented program reached the presentation call; without P2 photo it cannot prove visible pixel-perfect appearance.
- **Real `fnv=9efd2d18` matches the host C11 expected diagnostic raster**. This proves the entire 192×144 RGB565 diagnostic surface matches the deterministic P2 algorithm before LCD presentation; it is **not a Doom RPG gameplay scene hash**.
- **RAM**: heap8 initially **277,092 B**, after VIDEO allocation **221,524 B**, net **55,568 B** (55,296-B framebuffer plus 272-B other net reduction, not a controlled allocation-only measurement). Largest 8-bit contiguous free block after boot **110,580 B**, min heap8 observed by idle **215,988 B**. These are cold video-only measurements, **not** gameplay with maps/caches/audio resident.
- **Timing**: P2 native raster **5,560 µs**, LCD scanout **36,859 µs**, frame total **44,417 µs**. Difference **1,998 µs** includes time outside timed subphases and serial publication; it must not be attributed to 3D rendering. Diagnostic P2 raster is deliberately different from P1's checker. No FPS gameplay extrapolation.
- **Not started by design**: SD/PAK, map load, BSP/planes/sprites, native HUD art, gameplay/touch, music/SFX. No game-state or legacy-null-pointer claims can be inferred from their absence in this capture.

**P2 verdict: real CYD Serial + photographed geometry PASS for 192×144 24/96/24 diagnostic raster, correct FNV, LCD presentation, memory footprint, alignment, and no obvious clipping. RGB565 displayed palette differs from source (consistent with inverted LCD profile); calibrated color fidelity remains UNVALIDATED. Playable 60% world/HUD/audio remains NOT TESTED.** PR #364 stays draft, parallel to main, not merged.

## Real classic-CYD P2 photograph — 19 November 2026: geometry PASS, colors inverted

User supplied a photo of the actual CYD while the P2 calibration display was running (conversation attachment `1000055392.jpg`; container-backed uploaded payload is a WebP/RIFF image, 1536x864 pixels, SHA256 `7ce88c50d8a79daa81e91d78ea0062cf8ee16d0593f06de989e49912bc46ae13`). The LCD area is photographed at an angle, so this photo is a **qualitative** visual witness, not a calibrated screenshot or direct RGB565 readout.

**Geometry observed:** full panel filled, top and bottom bands of approximately equal visual height, central region about four times their height; horizontal boundaries continuous, grid and crossing diagonal/center marks visible, landscape orientation correct, no apparent clipping, displacement, or line-stride failure. This agrees with the independent pre-present `fnv=9efd2d18` and physical 40/160/40 nearest-neighbor mapping. **P2 visual GEOMETRY PASS**.

**Important color discrepancy, not to hide:** `ESP32/include/esp_res60_layout.h` writes red tones (`0xf800`/`0xc800`) in the top band, cyan tones (`0x07ff`/`0x05d7`) with blue grid (`0x001f`) in the middle, and yellow/orange (`0xffe0`/`0xfd20`) at the bottom; white/black center markers are `0xffff`/`0x0000`. The photo shows **cyan at the top, reddish/orange center, yellow grid, blue lower band**; black/white accents are also consistent with inversion. This closely matches a **bitwise RGB565 color complement** (`pixel XOR 0xffff`), as seen independently in the earlier P1 photo (source red/green/blue/yellow vs displayed cyan/magenta/yellow/blue). The boot log explicitly says `classic inversion=on`. This suggests panel/controller inversion or a display-profile issue, but the photo **does not establish which firmware or LCD-register step causes it**, nor whether an alternate CYD panel needs the opposite polarity. It is not evidence that 192x144 indexing or scaling is wrong. **Do not change or merge the production color profile without an isolated panel A/B hardware check.**

**Bounded verdict:** P2's requested photo gate is closed as **geometry PASS**; **palette/color fidelity FAIL against authored diagnostic colors** (cause pending separate display-profile check). No P2 code changes were required for this photographic verification. The FNV and memory metrics remain those of the Serial-tested source; commits after test must remain documentation-only. This is still **not a playable 192x96 world or a proof of audio headroom**.
