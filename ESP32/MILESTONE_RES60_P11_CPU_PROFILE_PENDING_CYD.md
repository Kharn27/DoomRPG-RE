# RES60-P11 — CPU phase profiling for native 192×96 preview (CI candidate / CYD PENDING)

**Branch:** `agent/esp32-res60-p1`, [draft PR #364](https://github.com/Kharn27/DoomRPG-RE/pull/364), `main=d3dd122cc0056109e8315f5376a24ba40fa4f685` (font SD merged). **2026-11-21.**

## Problem confirmed by the actual CYD

P10 projected **26 columns / 650 original wall texels** at angle 64, repeated 5/5, but each warm rotation costs **~27ms more than P9** at every cardinal angle. `heap8=134300`, `minHeap8=107308`, no RAM drift in 16 turns. Thus CPU analysis precedes any multi-material expansion; neither the exact cause nor a speedup has been demonstrated.

## Bounded implementation

`DOOMRPG_RES60_P11_PROFILE=1` is admitted **only** with P10+P9+P1 in the isolated RES60 profiles. Never enabled in normal `esp32-cyd`, inverted production, bringup or trace. Clock interface `EspRes60Profile_nowUs()` is an extern C bridge to Arduino `micros()`, in existing platform owner; test harness supplies a deterministic wraparound fake. No new persistent arrays, texture buffers, framebuffer, PAK bytes, heap allocations, world mutation or engine changes. Phase totals are scalar fields in the existing transient reports, not a second global owner.

The original MAP_INTRO runtime owner records **`invUs`** (inventory PAK), **`loadUs`** (native compact arena), **`p3Us`** (original P3/P9 projection), **`texUs`** (P8/P10 owner) and **`releaseUs`** (map reset). The P3 renderer records **`preflightUs`** (line validation + wedge), **`bandUs`** (HUD border fill), sums **`raysUs`** (192 lines-of-sight × candidates), **`geometryUs`** (depth, UV, column plan), **`fillUs`** (flat RGB565 per-pixel pass) and **`hashUs`** (raw P3 FNV). Texture owner records cold **`catalogUs`** (catalog build + resource selection) and **`texPakUs`** (open/header/read source), always **`swatchUs`** and **`overlayUs`**; on warm HIT the catalog and pack durations must be **0** (cache reused). **`preparedUs`** is elapsed time through native map, P3, P8, menu-strip navigation and final FNV, captured *before* diagnostic Serial printing. `[RES60P11] PROFILE angle=... cache=... rayTests=... overlayPixels=... heap8=... instrumentation=ON` logs values after rendering.

**Interpretation**: stage sums are not expected to equal the wall-clock `preparedUs` (unmeasured dispatch, source logging, timer calls, resource cleanup and LCD/header work). Microsecond clock calls are inside a loop of 192 columns, and therefore **measure with observer overhead**. Absolute P10 (unprofiled) vs P11 (profiled) times cannot be interpreted as a speedup or regression without a controlled baseline. The first cold START should be analyzed separately from all cache HIT quarter-turns. `PlatformVideo_present()` remains untouched; ~36.9ms is an independent baseline.

## Host safety and hardware gate

`ESP32/test/res60_p11_profile_host.py` compiles the actual P3/P9/P10 C sources twice with profile OFF/ON, compares **four exact framebuffer FNVs** and uses the P10 material fixture to assert same render output and canaries. The P8 C owner exercises catalog/PAK cold MISS and warm HIT with mock owner, phase witnesses and a monotonic 32-bit timer intentionally wrapping. The normal profile must have no reference to `EspRes60Profile_nowUs`; illegal P11 without P10/P9 is a compile-time error. Keep P3, P8, P9, P10 existing CI tests and both PlatformIO firmware builds.

Firmware to flash on the same alternate-panel CYD: `cd ESP32 && pio run -e esp32-cyd-res60-inverted -t upload -t monitor`. Execute START double-tap, several RIGHT/LEFT turns through four angles, MENU. **Required real Serial witnesses:** `[RES60P11] PROFILE` for each view, `cache=MISS` once then `HIT`, all four P3 FNVs **`ed9b68c4 / 48ae8442 / 2697640d / f43a2791`**, same material `logical=65` projected when visible, `heap8/minHeap8/largest8` non-drifting, `mapRuntime=RELEASED pak=closed`, no WDT.

**No CYD validation of P11 yet.** Do not mark the profiling milestone PASS or PR merge-ready until user-supplied Serial is reviewed. P10 hardware PASS remains bounded to one material.
