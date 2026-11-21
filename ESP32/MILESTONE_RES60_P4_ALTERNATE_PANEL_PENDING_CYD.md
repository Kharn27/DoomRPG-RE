# RES60-P4 — alternate-panel CYD variant of the original Intro 192x96 preview (CYD PENDING)

- Branch: `agent/esp32-res60-p1` — independent draft [PR #364](https://github.com/Kharn27/DoomRPG-RE/pull/364), issue #263; **no merge to main**.
- Exact main SHA used for the P1-P3 squashed rebase on 19 Nov 2026: `73520c22afa0e849807cd75de3540d9a5f826d45`; rebase commit `91818c2690034191bae491b968bf3f8c18198a2f`.
- Status: **double GitHub Actions CI SUCCESS — 221 steps each — on exact P4 code SHA `fd5de1c0c3f789a1eb47d530bd6d08c2e4cc6ddb`**, [push #44437351397](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44437351397) and [PR #44437361215](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44437361215). Actual alternate-panel CYD P4 test **still PENDING**. Prior P3 was physically inverted-panel hardware flashed with **classic** `esp32-cyd-res60` profile (inversion=on), visible walls `fnv=ed9b68c4`, poor colors; no P4 hardware result yet.

## Single bounded change: panel polarity/gamma, not the 3D engine

New PlatformIO environment:

```ini
[env:esp32-cyd-res60-inverted]
extends = env:esp32-cyd-res60
build_flags =
    ${env:esp32-cyd-res60.build_flags}
    -D DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION=1
    -I.pio/libdeps/esp32-cyd-res60-inverted/TFT_eSPI
```

This inherits P1/P2/P3 exactly: **one 192×144 RGB565 framebuffer (55,296 B), actual original `/intro.bsp` map via `/DoomRPG-ESP32.pak`, immutable compact arena, actual 192×96 flat shaded wall projection at y=24, calibration HUD bands 24+24, nearest 5/3 LCD output 320×240 using a single 640 B scanline, no audio, gameplay, touch, sprites or texture decoder**. No special board variant of the map renderer, geometry, line parser, HUD, or gameplay.

Only the panel profile is the pre-existing production inverted variant:
- `DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION=1` → `platformDisplay->invertDisplay(false)` and the alternate E0/E1 gamma profile, already used by the production `esp-cyd-inverted` environment.
- `esp32-cyd-res60` remains classic `invertDisplay(true)` and classic gamma.
- Production `esp32-cyd` and `esp-cyd-inverted` remain **160×120**, with **160×80** game view and identical original audio/render/etc; no native world code in those profiles.
- The independent `-I.pio/libdeps/esp32-cyd-res60-inverted/TFT_eSPI` supports a standalone PlatformIO build without a previous RES60 compilation; it is not a second framebuffer or a game-rendering difference.

## Automated guards

`ESP32/test/res60_p4_panel_profiles_host.py` checks exact PlatformIO inheritance and defines, no RES60 leakage into normal/inverted 50%, both panel inversion branches in `platform_video.cpp`, and strict C11 compile-time constants for 192×144, 192×96, 24/96/24, 55,296 B and nearest 5/3 LCD coverage. `diagnostic_profiles_host.py` and `sdl_f3_uncompiled_shim_host.py` explicitly account for six build environments and forbid an undeclared child or SDL shim regression. GitHub CI builds **both** original P3 classic and P4 alternate RES60 images, plus normal/inverted/bringup/trace firmware.

**CI candidate PASS verified in BOTH workflows on exact code SHA `fd5de1c0c3f789a1eb47d530bd6d08c2e4cc6ddb`**: 221 steps each including normal/inverted release, bringup, trace, RES60 classic, RES60 inverted, profile host and SDL source audit. This proves build/source parity only, **not** actual CYD P4 screen color fidelity. No claim of P4 real CYD PASS without new flash.

## Required real hardware test: CYD with alternate/inverted panel

Use the exact panel that showed odd colors under the classic P3 firmware, with the same microSD native PAK:

```sh
cd ESP32
pio run -e esp32-cyd-res60-inverted -t upload -t monitor
```

Expected Serial marker:

```text
[VIDEO] CYD display profile alternate-panel inversion=off gamma=ILI9341-driver-E0E1-trial ...
[RES60P3] BOOT mode=INTRO_GEOMETRY_ONLY target=192x96@y24 ...
[RES60P3] MAP source=PAK map=/intro.bsp bytes=21823 crc=623f34e4 arena=14095 arenaFNV=c3882516 nodes=223 lines=480 ...
[RES60P3] RASTER ... rayTests=92160 ... fnv=ed9b68c4 ...  (only if exact same PAK/source and spawn)
[RES60P1] PRESENT src=192x144 dst=320x240 method=floor-nearest lineBytes=640 ...
[RES60P3] FRAME ... source=PAK mapRuntime=RELEASED ...
[RES60P1] IDLE ...
```

The previous classic-profile hardware framebuffer fingerprint was **`ed9b68c4`**. **The raw RGB565 frame must be identical** under both panel modes if the same source is loaded; only the controller-side inversion/gamma differs. Do not compare photo RGB numerically without calibration. Confirm the same visible wall silhouettes and an improved-looking palette with a real CYD photo and reset-to-idle log. Different raw FNV indicates a real rendering/data mismatch, not a mere LCD polarity change. No P3 sprite/texture/HUD/gameplay/audio proof is implied even if P4 looks good.

**Gate:** keep PR #364 draft and parallel, no main merge until the real 60% game renderer is migrated and tested independently. Next technical frontier: native renderer 192-column BSP/plane/sprite ownership with bounded caches and one 192×144 framebuffer, not brute-force P3 per-column O(192×480) raycasts. Document P4 PASS only after actual alternate-panel CYD test.
