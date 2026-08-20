# ESP32 first bounded intro frame

Branch: `agent/esp32-first-intro-frame`

Base hardware-validated `main`:

```text
5275e4a1c6eca703b51221e80f3b199178015a01
```

Status: **FUNCTIONAL HARDWARE PASS; STORY FIT IMPLEMENTED; AWAITING FINAL VISUAL PASS**.

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

## First hardware result

The first bounded frame ran successfully on the classic CYD:

```text
entry FNV       = 485915c5
first-frame FNV = 6cf52a3e
heap8           = 50712 -> 50712
largest8        = 13300 -> 13300
deltaHeap       = 0
deltaLargest    = 0
state           = ST_INTRO (9)
storyPage       = 0
storyTextPage   = 0
```

`[ALIVE]` continued at 5 s and 10 s with the same heap/largest-block values, so
state, RAM and lifecycle behavior passed. The visible frame exposed one remaining
layout defect: the legacy story renderer assumes a 128x128 viewport.

On the 160x120 ESP32 framebuffer:

```text
SCR_CX = 80
SCR_CY = 60
legacy story rect = x 16..143, y -4..123
```

The horizontal axis fits, but the vertical axis extends four logical pixels above
and below the framebuffer. The original `More` / `Continue` baseline also reaches
`y=124`, so the bottom of those elements is cropped.

`6cf52a3e` is therefore kept as the **pre-fit hardware baseline**, not as the final
regression hash for this milestone.

## ESP32-native 128 -> 120 story fit

The corrected path no longer calls the original `DoomCanvas_drawStory()` from the
bounded ESP32 bridge. It calls `Esp32StoryFit_draw()` instead.

The original coordinates remain the behavioral reference, but they are treated as
a virtual square and mapped directly to the CYD framebuffer:

```text
virtual story space : 128x128
ESP32 viewport      : 120x120
viewport origin     : x=20, y=0
logical framebuffer : 160x120
physical TFT        : exact 2x -> 320x240
```

The native renderer transforms at draw time:

- scrolling space background
- progressive story font glyphs
- `More` / `Continue`
- menu hand
- animated intro layers
- spaceship
- laser lines and their clip rectangle

There is **no 128x128 intermediate framebuffer** and no heap allocation in the fit
renderer. Indexed textures are sampled directly by the existing ESP32
`SDL_RenderCopy()` scaler into the shared 160x120 RGB565 framebuffer.

Expected new Serial marker:

```text
[INTROFIT] virtual=128x128 -> viewport=120x120@(20,0) direct-to-framebuffer; no intermediate buffer
```

The output FNV must change from the pre-fit `6cf52a3e`; the corrected hash is to be
captured from the next real-CYD run.

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

The bridge establishes a deterministic local intro epoch:

```text
canvas.time          = 0
canvas.storyTextTime = 0
canvas.storyAnimTime = 0
canvas.showTextDone  = false
```

and executes only:

```text
Esp32StoryFit_draw(canvas)
DoomRPG_flushGraphics(doomRpg)
```

once.

There is no call to `DoomCanvas_run()`, no input dispatch and no map load.

## Final hardware PASS criteria

The next test must confirm:

- the full 120x120 story viewport is visible with no top/bottom crop
- `More` and the hand are entirely inside the framebuffer
- a new deterministic output FNV is stable across reboot/retest
- `heap8` remains `50712` before/after the draw, or any difference is explained
- `largest8` remains `13300` before/after the draw, or any difference is explained
- `state == ST_INTRO`
- `storyPage == 0`
- `storyTextPage == 0`
- no map/runtime resurrection
- `shapeData == NULL`
- `mediaTexels == NULL`
- caches inactive
- heartbeat continues with no reset/crash

After final hardware PASS, copy the corrected FNV/heap/largest-block values into
`PORTING_STATUS.md` and `README.md` before merge.

Only then move to the next milestone: an ESP32-owned intro clock driving repeated
frames through `Esp32StoryFit_draw()`, still without intro input or gameplay map
loading.
