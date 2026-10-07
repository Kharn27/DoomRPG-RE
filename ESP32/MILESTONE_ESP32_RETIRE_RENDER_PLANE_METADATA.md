# Milestone — Retire legacy Render plane preparation metadata (292 B)

Status: **REAL-CYD PASS** (2026-10-08)

Code SHA tested on the real classic CYD:
`d712378b86528065cedf0ce153c7e3a1357b1ba1`

Base main SHA: `02baa3bddd3b52c2399e33725aa4147a3840ad6e`

Branch: `agent/esp32-render-mapflags-retirement`

## Native ownership boundary

Normal ESP32 firmware no longer stores the historical Render
plane-preparation metadata: `planeTexelOffsets[24]` (96 B),
`planePaletteOffsets[24]` (96 B), `planeTextureIds[24]` (96 B)
and `planeTexturesCnt` (4 B). The 292 B cut follows the
1024 B `mapFlags` and 2048 B `planeTextures` cuts.

The legacy BSP loader and world/plane renderer were already
production-disabled. The generator additionally excludes
`Render_drawplane` and `Render_spanPlane` implementations in the
normal build and audits the historical metadata references.
The desktop and explicit bringup-probe layouts remain available;
two historical probes now query legacy counts only in bringup.
Production uses immutable `EspMapRuntime` planes and native
rendering, and retains the shared `sinTable` (still read by
`esp_native_first_frame.c`).

`Render_t` production layout: 5040 -> 4016 -> 1968 -> **1676 B**,
an exact cumulative reduction of **3364 B**.
Hardware startup reports `Render=1676 Game=4 Canvas=44 Total=2484`.
The startup bridge statically asserts `sizeof(Render_t)==1676`.

## CI

Exact code SHA: GitHub Actions normal `esp32-cyd` run **#1688
SUCCESS**, static RAM **45056 B**, flash **772381 B**.
Earlier intermediate CI failures exposed and corrected legacy
probe consumers; they are not hardware validation points.

## Real CYD — acceptance

| Stage | Prior heap8 | New heap8 | Recovered |
| --- | ---: | ---: | ---: |
| CORE READY | 184680 | 184972 | +292 B |
| LAYOUT | 178072 | 178364 | +292 B |
| mappings installed | 158912 | 159204 | +292 B |
| gameplay steady | 117860 | 118152 | +292 B |
| after exit to menu | 163756 | 164048 | +292 B |

Relevant serial witnesses:

```text
Engine structs: Render=1676 Game=4 Canvas=44 Total=2484 bytes
[CORE] READY objects=5 heap used=2664 remaining=184972 largest=110580
[MAINBOOT] READY ... frame=522dc605
[MAPRT] READY ... arenaFNV=c3882516
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
[NATIVEFRAME] LEGACY_GUARD ...
[NATIVEFRAME] RETRY ...
[NATIVEFRAME] RECOVERED ...
[DOORANIM] COMPLETE transitions=1 frames=4 state=stable transaction=committed
[RESIDENTRESET] heap8=146040->164048 released=18008 ... after=0/0/0/0/0/0/0 empty=1
[MAINMENU] Runtime cleanup ... shapeData=0x0 mediaTexels=0x0
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty saveWrite=no checkpoint=unchanged
```

Hardware exercised fresh intro/map entry, native movement,
rotation and render-guard recovery, crate attack/transformation,
Armor Shard pickups, door 275 opening and deferred closing,
automap publication, HUB inventory->system, double-confirm EXIT,
resident cleanup and return to the exact menu framebuffer.

No save was written. Checkpoint LOAD, other maps and exhaustive
monster encounters were not re-tested on this SHA; no claim of
those specific tests is made. The code SHA above is frozen for
this milestone. Post-test commits must be docs-only; later code
cuts require their own CI and hardware validation.
