# ESP32 bounded intro clock

Branch: `agent/esp32-intro-clock`

Base hardware-validated `main`:

```text
b934e21c7f2dbf6463a4d2dfa13d1e06614e2b96
```

Status: **IMPLEMENTED; AWAITING HARDWARE PASS**.

## Objective

Advance one bounded step beyond the validated fitted first `ST_INTRO` frame:
drive repeated intro frames from the Arduino `loop()` with an ESP32-owned clock,
without entering `DoomCanvas_run()`, processing intro input or loading gameplay.

Inherited recovery point:

```text
entry black FNV       = 485915c5
first fitted intro FNV= 56438966
state                 = ST_INTRO (9)
storyPage             = 0
storyTextPage         = 0
largest8              = 13300
shapeData             = NULL
mediaTexels           = NULL
wall/sprite caches    = inactive
```

## Clock model

The clock uses a quantized virtual timeline:

```text
step = 50 ms
virtual times = 0, 50, 100, 150, ...
target pacing ~= 20 FPS
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

The intro remains on page 0 / text page 0 for this increment. The text may finish
its progressive reveal and the scrolling background may animate, but `More` is
not actionable yet.

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

The clock also samples 8-bit heap before and after every draw. Any frame-local
change in `heap8` or `largest8` is treated as a failure and parks the clock.

## Expected Serial evidence

```text
[INTRO1] READY one deterministic ST_INTRO frame presented once FNV=56438966
[INTROCLK] ARMED step=50 ms startFNV=56438966 heap8=... largest8=13300 ...
[INTRO1] HANDOFF ... intro clock armed, input/map load still disabled
[INTROCLK] frame=1 tick=1 t=50 FNV=... heap8=... largest8=... skipped=...
[INTROCLK] frame=2 tick=2 t=100 FNV=... heap8=... largest8=... skipped=...
[INTROCLK] frame=3 tick=3 t=150 FNV=... heap8=... largest8=... skipped=...
[INTROCLK] frame=... tick=20 t=1000 FNV=... heap8=... largest8=... skipped=...
[INTROCLK] TEXT DONE tick=... t=... frames=... skipped=... heap8=... largest8=...
```

PASS requires:

- visibly animated fitted starfield / story presentation
- progressive story text reveal
- no touch-driven page transition
- no gameplay/map loading
- no reset/crash
- no `[INTROCLK] PARK` or `FAILED`
- no per-frame heap/largest-block change
- `largest8` remains compatible with the validated 13,300-byte boundary
- heartbeat continues while the intro clock runs

After hardware PASS, lock representative checkpoint FNVs, measured pacing,
skipped-tick count and RAM values into `PORTING_STATUS.md` / `README.md` before
merge.

Next milestone after merge: bounded intro input for `More` / `Continue`, still
without handing control to the general legacy game loop.
