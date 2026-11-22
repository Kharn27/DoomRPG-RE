# RES60-P11 — CPU phase profiling for native 192×96 preview (bounded CYD PASS)

**Branch:** `agent/esp32-res60-p1`, [draft PR #364](https://github.com/Kharn27/DoomRPG-RE/pull/364); firmware candidate code `30a39cc3c08326087c97d6108091ddc191095ae6` directly on verified base `main=4ece448f523d32db6ddcf85d1ca4492513339dbb` (ARCH-RC23). **Code 21 Nov / hardware 22 Nov 2026.** No SHA of uploaded binary printed in Serial. The source candidate's PR-associated CI workflow [44901588058](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44901588058) reports SUCCESS. On 22 Nov the live GitHub main had moved separately to `3956064a3f553a4ef6d34f24830f7bc4247a166a`, **not** a tested RES60 rebase.

## Problem confirmed by the actual CYD

P10 projected **26 columns / 650 original wall texels** at angle 64, repeated 5/5, but each warm rotation costs **~27ms more than P9** at every cardinal angle. `heap8=134300`, `minHeap8=107308`, no RAM drift in 16 turns. Thus CPU analysis precedes any multi-material expansion; neither the exact cause nor a speedup has been demonstrated.

## Bounded implementation

`DOOMRPG_RES60_P11_PROFILE=1` is admitted **only** with P10+P9+P1 in the isolated RES60 profiles. Never enabled in normal `esp32-cyd`, inverted production, bringup or trace. Clock interface `EspRes60Profile_nowUs()` is an extern C bridge to Arduino `micros()`, in existing platform owner; test harness supplies a deterministic wraparound fake. No new persistent arrays, texture buffers, framebuffer, PAK bytes, heap allocations, world mutation or engine changes. Phase totals are scalar fields in the existing transient reports, not a second global owner.

The original MAP_INTRO runtime owner records **`invUs`** (inventory PAK), **`loadUs`** (native compact arena), **`p3Us`** (original P3/P9 projection), **`texUs`** (P8/P10 owner) and **`releaseUs`** (map reset). The P3 renderer records **`preflightUs`** (line validation + wedge), **`bandUs`** (HUD border fill), sums **`raysUs`** (192 lines-of-sight × candidates), **`geometryUs`** (depth, UV, column plan), **`fillUs`** (flat RGB565 per-pixel pass) and **`hashUs`** (raw P3 FNV). Texture owner records cold **`catalogUs`** (catalog build + resource selection) and **`texPakUs`** (open/header/read source), always **`swatchUs`** and **`overlayUs`**; on warm HIT the catalog and pack durations must be **0** (cache reused). **`preparedUs`** is elapsed time through native map, P3, P8, menu-strip navigation and final FNV, captured *before* diagnostic Serial printing. `[RES60P11] PROFILE angle=... cache=... rayTests=... overlayPixels=... heap8=... instrumentation=ON` logs values after rendering.

**Interpretation**: stage sums are not expected to equal the wall-clock `preparedUs` (unmeasured dispatch, source logging, timer calls, resource cleanup and LCD/header work). Microsecond clock calls are inside a loop of 192 columns, and therefore **measure with observer overhead**. Absolute P10 (unprofiled) vs P11 (profiled) times cannot be interpreted as a speedup or regression without a controlled baseline. The first cold START should be analyzed separately from all cache HIT quarter-turns. `PlatformVideo_present()` remains untouched; ~36.9ms is an independent baseline.

## Host safety and hardware gate

`ESP32/test/res60_p11_profile_host.py` compiles the actual P3/P9/P10 C sources twice with profile OFF/ON, compares **four exact framebuffer FNVs** and uses the P10 material fixture to assert same render output and canaries. The P8 C owner exercises catalog/PAK cold MISS and warm HIT with mock owner, phase witnesses and a monotonic 32-bit timer intentionally wrapping. The normal profile must have no reference to `EspRes60Profile_nowUs`; illegal P11 without P10/P9 is a compile-time error. Keep P3, P8, P9, P10 existing CI tests and both PlatformIO firmware builds.

Firmware to flash on the same alternate-panel CYD: `cd ESP32 && pio run -e esp32-cyd-res60-inverted -t upload -t monitor`. Execute START double-tap, several RIGHT/LEFT turns through four angles, MENU. **Required real Serial witnesses:** `[RES60P11] PROFILE` for each view, `cache=MISS` once then `HIT`, all four P3 FNVs **`ed9b68c4 / 48ae8442 / 2697640d / f43a2791`**, same material `logical=65` projected when visible, `heap8/minHeap8/largest8` non-drifting, `mapRuntime=RELEASED pak=closed`, no WDT.

## Real classic CYD evidence — 22 November 2026: bounded P11 PASS

Capture `platformio-device-monitor-261122-053555.log`, **250 lines / 31,918 bytes, SHA256 `3f399a0fda09802fbe35a398dcc4fbd5cb9da550b07ff81a17fda9394f526996`**. Device `ESP32-D0WD-V3`, 240 MHz, 4 MiB flash, `PSRAM=0`; display alternate-panel profile `inversion=off`. Attribution to candidate `30a39cc` is from the user-provided branch+SHA and the distinctive P11 tags, not a firmware build fingerprint. **11 real profiles**, 1 cold cache MISS / 10 HIT, 10 committed rotations (5 RIGHT, 5 LEFT) through all four cardinal poses, 2 native menu draws, successful final return MENU; 11 map compact arenas loaded then released, PAK closed. The P3 cardinal raw FNVs are stable: N64 `ed9b68c4`, E0 `48ae8442`, S192 `2697640d`, W128 `f43a2791`. N64 `logical=65`, **26 visible columns / 650 true texels**, `sampleFNV=e1f75a15`, stable. Other angles produce zero columns only for the intentionally selected single material, not zero wall geometry. Heap8 after map release **134204 B every frame**, largest8 **86004 B**, minHeap8 **107156 B**; no progressive leak or visible panic/WDT/FAIL. Previous P10 uninstrumented values were 134300/86004/107308 B: the small reduction is consistent with additional profiling, **not** a measured dynamic RAM saving.

### P11 phases measured on hardware

| Phase | First cold frame N64 | Warm cardinal samples |
| --- | ---: | ---: |
| `inventoryUs` | 135231 us | 134905–135450 us |
| `loadUs` | 49791 us | 49707–49748 us |
| `catalogUs` | **1314650 us** | **0 us** |
| `texPakUs` | 33183 us | **0 us** |
| `p3Us` | 146073 us | N64 ≈146026, E0 ≈55942, S192 ≈11346, W128 ≈24524 us |
| `textureUs` | 1349940 us | 529–1156 us |
| `preparedUs` | **1697582 us** | N64 ≈348301, E0 ≈257920, S192 ≈213097, W128 ≈226780 us |
| LCD `PRESENT` | ≈36930 us | ≈36930 us |

**Diagnosis, not yet optimization:** every warm rotation needlessly repeats approximately **185 ms** of full BSP inventory+compact arena load, irrespective of viewing angle; the P3 ray phase is the remaining strongly angle-dependent cost (about 3.5–138.4 ms in rays). Cold start remains dominated by a one-off catalog build (~1.315 s). Timings include P11 observation overhead; the unprofiled P10 benchmark must not be subtracted directly to assert gain. Prioritize a **separate P12 bounded inventory reuse / lifecycle** only with explicit source validation, cache invalidation, fail-closed behavior, host tests and real CYD verification. The current compact map is still released after every render and the PAK closed before present; neither full gameplay, audio, real sprite projection nor multi-material fidelity is tested. P11 is **CYD PASS for its measurement objective only**, while [PR #364](https://github.com/Kharn27/DoomRPG-RE/pull/364) remains **DRAFT / NOT MERGE-READY**.
