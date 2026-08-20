# ESP32 bounded intro clock

Branch: `agent/esp32-intro-clock`

Base hardware-validated `main`:

```text
b934e21c7f2dbf6463a4d2dfa13d1e06614e2b96
```

Status: **MERGED ARCHIVE — HARDWARE PASS — PR #38**.

Merged to `main` at:

```text
58edfe5d7080a7e9e64ff5b516697ddf3cca31da
```

This file is the historical hardware-evidence archive for PR #38. Its measured
heap/timing values belong to that build. For the current recovery point, see
[`PORTING_STATUS.md`](PORTING_STATUS.md); for documentation ownership rules, see
[`DOCUMENTATION.md`](DOCUMENTATION.md).

## Objective

Advance one bounded step beyond the validated fitted first `ST_INTRO` frame:
drive repeated intro frames from the Arduino `loop()` with an ESP32-owned clock,
without entering `DoomCanvas_run()`, processing intro input or loading gameplay.

Inherited recovery point:

```text
entry black FNV        = 485915c5
first fitted intro FNV = 56438966
state                  = ST_INTRO (9)
storyPage              = 0
storyTextPage          = 0
largest8               = 13300
shapeData              = NULL
mediaTexels            = NULL
wall/sprite caches     = inactive
```

## Clock model

The clock uses a quantized virtual timeline:

```text
step = 50 ms
virtual times = 0, 50, 100, 150, ...
nominal pacing ~= 20 FPS
```

The already-validated `t=0` frame is presented first. `Esp32IntroClock_arm()` then
captures wall-clock uptime and verifies that the framebuffer still hashes to
`56438966` before enabling the clock.

`loop()` calls `Esp32IntroClock_service()` once per pass. Service computes:

```text
targetTick = (wallNow - wallStart) / 50
canvas.time = targetTick * 50
```

At most one frame is rendered per service call. If presentation takes long enough
that multiple virtual ticks elapsed, old ticks are counted as skipped rather than
rendered late. This keeps the firmware responsive and prevents catch-up bursts.

## Render path

Each due frame executes only:

```text
Esp32StoryFit_draw(canvas)
DoomRPG_flushGraphics(doomRpg)
```

There is still no call to:

```text
DoomCanvas_run()
DoomCanvas_runInputEvents()
DoomCanvas_handleStoryInput()
DoomCanvas_loadMap()
```

The intro remains on page 0 / text page 0 for this increment. The progressive
story text and fitted starfield animate, but `More` is intentionally not actionable
yet.

## Per-frame safety contract

Before and after every rendered clock frame the runtime must remain:

```text
menu                  = MENU_NONE
state                 = ST_INTRO
storyPage             = 0
storyTextPage         = 0
nodes                  = NULL
lines                  = NULL
mapSprites             = NULL
mediaTexelOffsets      = NULL
mediaBitShapeOffsets   = NULL
mapTextureTexels       = NULL
mapSpriteTexels        = NULL
shapeData              = NULL
mediaTexels            = NULL
wall/sprite LRU caches = inactive
```

The clock samples 8-bit heap before and after every draw. Any frame-local change
in `heap8` or `largest8` parks the clock.

## Hardware PASS

Validated normal-firmware sequence:

```text
[INTRO1] Begin state=9 menu=0 page=0 textPage=0 frameFNV=485915c5 expectedEntry=485915c5 heap8=50672 largest8=13300
[INTROFIT] virtual=128x128 -> viewport=120x120@(20,0) direct-to-framebuffer; no intermediate buffer
[INTRO1] Drawn t=0 frameFNV=56438966 heap8=50672 largest8=13300 deltaHeap=0 deltaLargest=0 state=9 page=0 textPage=0
[VIDEO] Present 160x120 -> 320x240 exact 2x + sat1.15: 42802 us
[INTRO1] READY one deterministic ST_INTRO frame presented once FNV=56438966
[INTROCLK] ARMED step=50 ms startFNV=56438966 heap8=50672 largest8=13300 wallStart=1313
[INTRO1] HANDOFF state=9 page=0 textPage=0; intro clock armed, input/map load still disabled
[INTROCLK] frame=1 tick=1 t=50 FNV=da9cd50e heap8=50672 largest8=13300 skipped=0 textDone=0
[INTROCLK] frame=2 tick=2 t=100 FNV=c63cf367 heap8=50672 largest8=13300 skipped=0 textDone=0
[INTROCLK] frame=3 tick=4 t=200 FNV=2620e850 heap8=50672 largest8=13300 skipped=1 textDone=0
[INTROCLK] frame=14 tick=20 t=1000 FNV=e76fec13 heap8=50672 largest8=13300 skipped=6 textDone=0
[ALIVE] uptime=5020 ms heap=116436 heap8=50672 largest8=13300 SD=ready ZIP=ready VIDEO=ready CORE=ready LAYOUT=ready PRERENDER=ready RENDER=ready MAPPINGS=ready MENUBSP=ready touchIRQ=idle
```

Representative regression checkpoints:

```text
t=0     FNV = 56438966
t=50    FNV = da9cd50e
t=100   FNV = c63cf367
t=200   FNV = 2620e850
t=1000  FNV = e76fec13
```

RAM contract on the clock build:

```text
heap8       = 50672 -> 50672 per rendered frame
largest8    = 13300 -> 13300 per rendered frame
deltaHeap   = 0
deltaLargest= 0
```

The 32-byte difference from the previous first-frame build (`50704` -> `50672`)
is a build-to-build baseline difference introduced by the clock state/code. It is
not a frame allocation; rendered frames remain allocation-free.

## Measured pacing

The nominal virtual step is 50 ms (~20 FPS), but the current physical TFT
presentation costs about 42.8 ms per full 160x120 -> 320x240 frame with the
selected saturation 1.15 output transform.

At virtual `t=1000 ms` hardware reported:

```text
ticks elapsed    = 20
frames rendered  = 14
ticks skipped    = 6
effective render ~= 14 FPS
Present          ~= 42.77..42.84 ms
```

This is a **functional/RAM PASS with a known presentation-performance limit**,
not a clock/state-machine failure. The clock behaves as designed: it keeps virtual
time current and skips stale ticks instead of producing catch-up bursts.

The perceived pacing should be compared with the original J2ME game before any
optimization is made. If optimization is later required, `PlatformVideo_present()`
and especially the final saturation transform are the first measured candidates.

## Final milestone contract

PASS was established for:

- animated fitted starfield / story presentation
- progressive story text reveal
- fixed 50 ms virtual timeline
- bounded skipped-tick behavior under presentation pressure
- no intro input or page transition
- no gameplay/map loading
- no reset/crash
- no `[INTROCLK] PARK` / `FAILED`
- zero per-frame heap/largest-block delta
- `largest8 == 13300`
- heartbeat while the clock is running
- no `DoomCanvas_run()` handoff

This evidence was merged in PR #38.

## Historical next boundary at merge time

At the time PR #38 merged, the planned next increment was bounded intro input for
`More` / `Continue`, still using the ESP32-owned story renderer/clock and still
without handing control to the broad legacy game loop or loading the first
gameplay map.

That statement is retained as historical context. The live roadmap is in
[`PORTING_STATUS.md`](PORTING_STATUS.md).
