# ESP32 bounded intro input

Branch: `agent/esp32-intro-input`

Base hardware-validated `main`:

```text
58edfe5d7080a7e9e64ff5b516697ddf3cca31da
```

Status: **IMPLEMENTED; AWAITING HARDWARE PASS**.

## Objective

Add the first bounded interactive progression to the ESP32-owned `ST_INTRO`
path while keeping the broad legacy game loop and gameplay loader unreachable.

The inherited intro clock, fitted 120x120 story renderer and allocation-free
per-frame contract remain unchanged. This increment adds one released-tap
consumer through the existing `PlatformInput_setTapCallback()` boundary.

## Original behavior used as the specification

The reverse-engineered engine routes `SELECT` during `ST_INTRO` to
`DoomCanvas_handleStoryInput()`:

- tap/select while progressive text is incomplete -> reveal the full text
- tap/select after page-0 text 0 -> advance to text 1 (`More`)
- tap/select after page-0 text 1 -> advance to animated story page 1 (`Continue`)
- tap/select on story page 1 -> skip the animation and advance to page 2
- tap/select while page-2 text is incomplete -> reveal the full text
- final Continue would normally advance to story page 3

The original `DoomCanvas_changeStoryPage()` calls `DoomCanvas_disposeIntro()`
when page 3 is reached, and `DoomCanvas_disposeIntro()` immediately calls
`DoomCanvas_loadMap()`. That final transition is deliberately NOT executed by
this increment.

## ESP32 bounded progression

The native input path reproduces the visible/story-state semantics through page 2:

```text
page 0 / text 0
  tap during reveal -> full text
  tap when done     -> page 0 / text 1

page 0 / text 1
  tap during reveal -> full text
  tap when done     -> page 1 animation

page 1 animation
  tap inside story viewport -> page 2 immediately
  OR clock timeout (~10 s)  -> page 2 through existing story renderer

page 2 / text 0
  tap during reveal -> full text
  tap when done     -> PARK intro-exit-ready
```

At the final PARK boundary:

```text
state         = ST_INTRO
storyPage     = 2
storyTextPage = 0
intro clock   = inactive
intro input   = inactive
intro images  = retained
intro texts   = retained
map load      = NOT called
```

This creates the exact next safe boundary for a later dedicated
intro-disposal/loading increment.

## Touch geometry

The platform still owns XPT2046 sampling, calibration and released-tap debounce.
No second touchscreen poller is introduced.

Text pages accept taps only in the bottom prompt band of the fitted story square:

```text
logical story viewport = x20..139 y0..119
prompt hit band         = x20..139 y102..119
```

The animated page has no visible prompt, so any tap inside the 120x120 story
viewport may skip it.

## Virtual-time epochs

The clock timeline remains globally quantized at 50 ms, but story-local epochs
are rebased to the current `canvas.time` when text/page state changes.

This prevents the original `-1 -> DoomRPG_GetUpTimeMS()` lazy initialization from
mixing wall uptime with the ESP32 virtual intro timeline.

Rules:

```text
More (text 0 -> text 1): reset storyTextTime only
page 0 -> page 1:         reset storyTextTime + storyAnimTime
page 1 -> page 2:         reset storyTextTime + storyAnimTime
```

The automatic 10-second page-1 transition is detected after the fitted draw and
rebased before the next frame.

## Safety contract

Throughout interactive progression the clock/input boundary requires:

```text
menu                  = MENU_NONE
state                 = ST_INTRO
storyPage             = 0, 1 or 2
valid textPage        = 0..1 on page 0; 0 on pages 1/2
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
intro images/texts     = resident
```

Every input mutation samples `heap8` and `largest8`; a change parks the clock and
disarms input. Existing clock frames retain their allocation-free checks.

## Expected Serial evidence

Startup:

```text
[INTROCLK] ARMED step=50 ms ...
[INTROIN] READY released-tap input armed promptLogical=x20..139 y102..119 animLogical=x20..139 y0..119
[INTROIN] CONTRACT reveal -> More -> page1 animation -> page2 -> final PARK; dispose/map load blocked
[INTRO1] HANDOFF ... intro clock+input armed, dispose/map load still blocked
```

Representative interaction:

```text
[INTROIN] TAP ... page=0 textPage=0 textDone=0 accepted=1
[INTROIN] REVEAL page=0 textPage=0 ...
[INTROIN] TAP ... page=0 textPage=0 textDone=1 accepted=1
[INTROIN] MORE textPage=0->1 ...
[INTROIN] CONTINUE storyPage=0->1 ...
[INTROIN] SKIP-ANIM storyPage=1->2 ...
[INTROIN] REVEAL page=2 textPage=0 ...
[INTROIN] FINAL-CONTINUE page=2 textPage=0 ...
[INTROCLK] PARK reason=intro-exit-ready ...
[INTROIN] READY-TO-EXIT ... assets=retained noDispose=yes noMapLoad=yes
```

If page 1 is allowed to run to completion instead of being tapped:

```text
[INTROCLK] AUTO-PAGE 1->2 ...
```

PASS requires:

- text reveal-on-tap works
- `More` reaches page-0 text 1
- `Continue` reaches the animated page 1
- page 1 can be skipped by touch
- page 1 can also auto-advance after its existing timeout
- page-2 reveal works
- final Continue parks at page 2 instead of disposing/loading
- no reset/crash
- no unexpected `[INTROCLK] PARK` / `[INTROIN] FAILED`
- no input-time or frame-time heap/largest-block delta
- `shapeData == NULL` and `mediaTexels == NULL`
- heartbeat continues after final PARK

After hardware PASS, record measured RAM/FNV/state evidence in
`PORTING_STATUS.md`, `README.md` and this file before merge.

Next milestone after merge: bounded intro disposal and transition toward the first
real gameplay-map load, with a fresh RAM/resource measurement before allowing any
large map graphics working set.
