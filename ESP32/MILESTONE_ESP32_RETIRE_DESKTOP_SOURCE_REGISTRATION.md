# Milestone — remove original desktop C BuildSources registration

Status: **REAL-CYD HARDWARE PASS — DOCS-ONLY CLOSURE**

Hardware-tested code SHA: `48aab6de7ef0ec06cbb9929894ea9add1b9dc086`. Normal `esp32-cyd`
GitHub Actions run #37706071148: **SUCCESS** (static RAM
45056 B, flash 774521 B). Build guard recorded the exact 21 original
TUs, compiled 0 original C TUs and registered only patched
`DoomRPG.c`/`Render.c`. The latter remain desktop dependencies.

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

## Real physical CYD acceptance (2026-10-08)

The user tested the exact code SHA in the standard `esp32-cyd`
firmware, not bringup. Fresh startup showed `Render=1532 B`,
`Game=4 B`, `Canvas=44 B`, shared 38400-byte framebuffer,
`MENU_MAIN=522dc605` and resident intro BSP arena `c3882516`.
Animation forward 1 midpoint + turn 2 previews rendered/presented,
normal committed MOVE turn cadence, successful crate SELECT transform,
Armor Shard pickup and native event-82 dialogue → opcode-19 state
mutation. HUB SYS double-confirm EXIT cleanly returned to menu:

```text
[DIALOGCHAIN] OWNER-RELEASE journal=1020 topologyCapacity=0 activeAtTeardown=1 heap8=118356->119392 recovered=1036 owner=none
[RESIDENTRESET] heap8=146176->164184 released=18008 before=1/1/1/1/1/1/1 after=0/0/0/0/0/0/0 empty=1
[MAINMENU] Runtime cleanup ... heap8=164184->164184 largest8=110580->110580 shapeData=0x0 mediaTexels=0x0
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty saveWrite=no checkpoint=unchanged
[ALIVE] ... heap8=164184 largest8=110580 ...
```

Gameplay `heap8=118288 largest8=86004` when fresh,
`heap8=117252` after lazy dialogue journal allocation,
with no drift between samples. Old `[NATIVEFRAME] LEGACY_GUARD`
logged RETRY and RECOVERED as before.

The users' test did not include LOAD, CHANGEMAP, active monsters,
blocked collision, door mutation or topology snapshot release.
Those remain unproven on this specific code SHA.

## Closure policy

This branch remains active **at the user's explicit request**;
this milestone's tested code is frozen, and this commit changes
only documentation. The next bounded retirement must get a new
code SHA and separate hardware test, without creating another branch.

## Original candidate acceptance checklist

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

Real-CYD PASS for the bounded registration retirement is recorded above.
