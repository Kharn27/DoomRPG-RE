# Milestone — remove original desktop C BuildSources registration

Status: **CODE CANDIDATE — normal CI and real-CYD validation pending**

Main source-of-truth base SHA:
`38e961c65ac0fa2fb036f6dce2c58fdf86783c70` (merged animation PR #201).
Branch: `agent/esp32-retire-desktop-source-registration`.

## Audited problem and file census

At base main, the original desktop `src/` contains **21 C**
translation units and 43 C/C++ source/header files total.
The ESP32 port has **188 C/C++/header files under ESP32/src**
(169 .c, 11 .cpp and 8 .h), plus **169 headers under ESP32/include**.
This is modular native gameplay, platform, backing store and adapters;
**not 188 original engine TUs**. The actual production engine generation
registers two patched original roots, `DoomRPG.c` and `Render.c`,
and the generated packed-indexed SDL shim.

Previously `build_engine.py` also performed a second
`env.BuildSources(..., engine_dir, src_filter=[+<*.c>, -<...>])` call
for the *unmodified* desktop source tree. All 21 original C files
were already individually excluded, so that build stage generated
**zero object files**. Its wildcard was a latent risk: a future
original C file could silently enter the firmware.

## Bounded permanent change

Remove the zero-object `BuildSources` call altogether.
Instead assert the exact audited set of 21 original C filenames
at build-script execution; any subsequent original-source addition or
removal forces a deliberate review before the production build proceeds.
The patched root BuildSources call remains explicitly allowlisted to
`DoomRPG.c` and `Render.c`; the indexed SDL shim remains separate.
Original desktop files, headers, CMake and the native owner graph
are not modified.

Expect source-build output:
`[ESP32] Desktop original source registration retired;
originalC=21 compiledOriginal=0 patchedRoots=DoomRPG.c,Render.c`.

No assumption is made that the desktop *header and ABI dependencies*
are already removed. Two generated patched engine roots survive and
remain the next explicit retirement front. Do not claim a reduced
binary RAM/flash footprint from a previously zero-object stage.

## Acceptance

1. Normal `esp32-cyd` CI compiles successfully with the guard
   firing and reports expected source registration telemetry.
2. Validate that native startup/gameplay and menu fingerprints
   remain unchanged on the physical CYD; `shapeData`/`mediaTexels`
   remain NULL. No regression in rotation/move previews, movement
   turn scheduling, native SAVE/LOAD or SYS EXIT.
3. Compare static RAM and flash only as observed from CI.
4. After hardware confirmation, code is frozen and follow-on
   documentation changes only. Merge remains user-controlled
   unless explicitly authorized.

No real-CYD PASS is claimed until the user supplies Serial logs.
