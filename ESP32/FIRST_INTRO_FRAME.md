# ESP32 first bounded intro frame

Branch: `agent/esp32-first-intro-frame`

Base hardware-validated `main`:

```text
5275e4a1c6eca703b51221e80f3b199178015a01
```

Status: **IMPLEMENTED; AWAITING HARDWARE PASS**.

## Objective

Advance exactly one boundary beyond the validated fresh Start Game result:
render and present one real deterministic `ST_INTRO` frame, then park.

This increment intentionally does **not** start the original general game loop.

Validated entry contract from PR #36:

```text
menu            = MENU_NONE (0)
state           = ST_INTRO (9)
framebuffer FNV = 485915c5
heap8           = 50712
largest8        = 13300
shapeData       = NULL
mediaTexels     = NULL
```

`485915c5` is the FNV-1a of the completely black 160x120 RGB565 framebuffer.

## Bounded execution

After fresh Start succeeds, `DoomRPG_esp32RenderFirstIntroFrame()` verifies that:

- state is still `ST_INTRO`
- menu is `MENU_NONE`
- story page/text page are both zero
- all three prologue text allocations exist
- all four intro BMP textures exist
- released menu runtime remains released
- `shapeData == NULL`
- `mediaTexels == NULL`
- wall and sprite LRU caches remain inactive
- the shared platform framebuffer still hashes to `485915c5`

The bridge then establishes a deterministic local intro epoch:

```text
canvas.time          = 0
canvas.storyTextTime = 0
canvas.storyAnimTime = 0
canvas.showTextDone  = false
```

and executes only:

```text
DoomCanvas_drawStory(canvas)
DoomRPG_flushGraphics(doomRpg)
```

once.

There is no call to `DoomCanvas_run()`, no input dispatch and no map load.

## Hardware PASS markers

Expected Serial sequence:

```text
[INTRO1] Begin state=9 menu=0 page=0 textPage=0 frameFNV=485915c5 ...
[INTRO1] Drawn t=0 frameFNV=<NEW HASH> heap8=<MEASURE> largest8=<MEASURE> ...
[INTRO1] READY one deterministic ST_INTRO frame presented once FNV=<same hash>
[INTRO1] PARK state=9 page=0 textPage=0; no DoomCanvas_run, no input dispatch, no map load
```

PASS requires:

- first genuine intro frame visible on the CYD instead of the black stop
- same output FNV across repeated boot/test
- `state == ST_INTRO`
- `storyPage == 0`
- `storyTextPage == 0`
- no map/runtime resurrection
- `shapeData == NULL`
- `mediaTexels == NULL`
- caches inactive
- heartbeat continues with no reset/crash

After hardware PASS, copy the measured FNV/heap/largest-block values into
`PORTING_STATUS.md` and `README.md` before merge.

Only then move to the next milestone: an ESP32-owned intro clock driving repeated
frames, still without intro input or gameplay map loading.
