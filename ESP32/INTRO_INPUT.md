# ESP32 bounded intro input

Branch: `agent/esp32-intro-input`

Base hardware-validated `main`:

```text
58edfe5d7080a7e9e64ff5b516697ddf3cca31da
```

Status: **MERGED ARCHIVE — FULL HARDWARE PASS — ALL INTRO INPUT BRANCHES VALIDATED — PR #39**.

Merged to `main` at:

```text
7ba68955a9b0979924c5e759736fb483589be744
```

This file is the historical hardware-evidence archive for PR #39. The two real-CYD
runs below intentionally remain detailed and build-specific. For the current
recovery point, see [`PORTING_STATUS.md`](PORTING_STATUS.md); for documentation
ownership rules, see [`DOCUMENTATION.md`](DOCUMENTATION.md).

## Objective

Add the first bounded interactive progression to the ESP32-owned `ST_INTRO`
path while keeping the broad legacy game loop and gameplay loader unreachable.

The inherited intro clock, fitted 120x120 story renderer and allocation-free
per-frame contract remain unchanged. This increment adds one semantic tap
consumer through the existing `PlatformInput_setTapCallback()` boundary.

## Touch delivery semantics

The XPT2046 driver emits one semantic tap immediately on the press edge. It then
requires a stable 50 ms release before another semantic tap can be emitted.

So the correct contract is:

```text
press edge -> one semantic tap callback
hold       -> no repeat callback
release    -> 50 ms stable-release debounce
next press -> next semantic tap callback
```

Earlier implementation/log wording used the phrase `released-tap`; that wording
was inaccurate and is corrected in this milestone. The runtime behavior itself
was already correct.

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

The platform still owns XPT2046 sampling/calibration and stable-release rearm.
No second touchscreen poller is introduced.

Text pages accept taps only in the bottom prompt band of the fitted story square:

```text
logical story viewport = x20..139 y0..119
prompt hit band         = x20..139 y102..119
```

The animated page has no visible prompt, so any tap inside the 120x120 story
viewport may skip it.

The first hardware run deliberately proved rejection outside the prompt band:

```text
physical=115,59 -> logical=57,29
page=0 textPage=0 textDone=0 accepted=0
[INTROIN] MISS ... promptBandY=102..119
```

No story state or RAM changed after this miss.

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

## Hardware evidence

Validated normal-firmware baseline for this build:

```text
first fitted FNV = 56438966
heap8            = 50656
largest8         = 13300
state            = ST_INTRO (9)
menu             = MENU_NONE
```

The 16-byte difference from the prior intro-clock build (`50672 -> 50656`) is a
build-to-build baseline difference after adding the input state/callback code.
All measured frame and input transitions remained allocation-free.

The second validation run also measured the pre-Start menu and cleanup boundary:

```text
MENU_MAIN heap8        = 29008
post-cleanup heap8     = 84424
cleanup gain           = 55416 B
post-cleanup largest8  = 36852
intro heap8            = 50656
intro largest8         = 13300
```

The cleanup gain remains exactly the same 55,416 bytes as the earlier Start
milestone despite small build-to-build baseline movement.

Initial clock hashes remain stable:

```text
t=50 ms    FNV=da9cd50e
t=100 ms   FNV=c63cf367
t=200 ms   FNV=2620e850
t=1000 ms  FNV=e76fec13
```

### Natural page-1 timeout path

The first end-to-end hardware run validated the natural animation timeout:

```text
[INTROCLK] TEXT DONE page=0 textPage=0 tick=78 t=3900 ...

[INTROIN] TAP ... logical=115,112 page=0 textPage=0 textDone=1 accepted=1
[INTROIN] MORE textPage=0->1 t=5600 textEpoch=5600
[INTROIN] READY page=0 textPage=1 textDone=0 heap8=50656 largest8=13300

[INTROIN] TAP ... page=0 textPage=1 textDone=0 accepted=1
[INTROIN] REVEAL page=0 textPage=1 t=7400
[INTROIN] READY page=0 textPage=1 textDone=1 heap8=50656 largest8=13300

[INTROIN] TAP ... page=0 textPage=1 textDone=1 accepted=1
[INTROIN] CONTINUE storyPage=0->1 t=9150 epoch=9150
[INTROIN] READY page=1 textPage=0 textDone=0 heap8=50656 largest8=13300

[INTROCLK] AUTO-PAGE 1->2 t=19200 textPage=0 epoch=19200

[INTROCLK] TEXT DONE page=2 textPage=0 tick=453 t=22650 ...

[INTROIN] TAP ... page=2 textPage=0 textDone=1 accepted=1
[INTROIN] FINAL-CONTINUE page=2 textPage=0 t=23650
[INTROCLK] PARK reason=intro-exit-ready tick=473 frames=282 skipped=191 state=9 page=2 textPage=0 heap8=50656 largest8=13300
[INTROIN] READY-TO-EXIT state=9 page=2 textPage=0 heap8=50656 largest8=13300 assets=retained noDispose=yes noMapLoad=yes
[TOUCH] ...
```

The natural page-1 animation lasted from virtual `t=9150` to `t=19200`, i.e.
10.05 seconds. The extra 50 ms is the expected quantization of the 50 ms clock.

### Explicit reveal + page-1 touch-skip path

A second hardware run closed every remaining alternate branch:

```text
[INTROIN] TAP n=1 ... page=0 textPage=0 textDone=0 accepted=1
[INTROIN] REVEAL page=0 textPage=0 t=2050
[INTROIN] READY page=0 textPage=0 textDone=1 heap8=50656 largest8=13300

[INTROIN] TAP n=2 ... page=0 textPage=0 textDone=1 accepted=1
[INTROIN] MORE textPage=0->1 t=3300 textEpoch=3300
[INTROIN] READY page=0 textPage=1 textDone=0 heap8=50656 largest8=13300

[INTROIN] TAP n=3 ... page=0 textPage=1 textDone=0 accepted=1
[INTROIN] REVEAL page=0 textPage=1 t=4600
[INTROIN] READY page=0 textPage=1 textDone=1 heap8=50656 largest8=13300

[INTROIN] TAP n=4 ... page=0 textPage=1 textDone=1 accepted=1
[INTROIN] CONTINUE storyPage=0->1 t=5400 epoch=5400
[INTROIN] READY page=1 textPage=0 textDone=0 heap8=50656 largest8=13300

[INTROIN] TAP n=5 ... page=1 textPage=0 textDone=0 accepted=1
[INTROIN] SKIP-ANIM storyPage=1->2 t=7300 epoch=7300
[INTROIN] READY page=2 textPage=0 textDone=0 heap8=50656 largest8=13300

[INTROIN] TAP n=6 ... page=2 textPage=0 textDone=0 accepted=1
[INTROIN] REVEAL page=2 textPage=0 t=8400
[INTROIN] READY page=2 textPage=0 textDone=1 heap8=50656 largest8=13300

[INTROIN] TAP n=7 ... page=2 textPage=0 textDone=1 accepted=1
[INTROIN] FINAL-CONTINUE page=2 textPage=0 t=9150
[INTROCLK] PARK reason=intro-exit-ready tick=183 frames=115 skipped=68 state=9 page=2 textPage=0 heap8=50656 largest8=13300
[INTROIN] READY-TO-EXIT state=9 page=2 textPage=0 heap8=50656 largest8=13300 assets=retained noDispose=yes noMapLoad=yes
[TOUCH] ...
```

This proves on hardware that the same bounded state machine supports both page-1
exit modes:

```text
page 1 timeout    -> AUTO-PAGE 1->2   PASS
page 1 touch skip -> SKIP-ANIM 1->2   PASS
```

It also proves early reveal on every progressive text stage used by this intro:

```text
page 0 / text 0 -> REVEAL PASS
page 0 / text 1 -> REVEAL PASS
page 2 / text 0 -> REVEAL PASS
```

RAM remained:

```text
heap8    = 50656
largest8 = 13300
```

through first frame, every reveal, `More`, `Continue`, both page-1 transition
modes, page-2 progression and final PARK.

The `[TOUCH]` markers after `READY-TO-EXIT` prove the Arduino loop continued after
PARK instead of resetting or entering the map loader. Stable 5-second heartbeats
were also observed during the long natural-timeout run.

No unexpected `[INTROIN] FAILED` or `[INTROCLK] PARK` occurred before the intended
final `intro-exit-ready` park.

## Full hardware branch coverage

Across the two captured real-CYD runs, every bounded intro input branch is now
hardware validated:

- out-of-band prompt touch -> `MISS` with no state/RAM mutation
- page-0 text 0 early reveal
- page-0 text 0 complete -> `More` / text 1
- page-0 text 1 early reveal
- page-0 text 1 complete -> `Continue` / page 1
- page-1 natural ~10-second timeout -> page 2
- page-1 touch skip -> page 2
- page-2 early reveal
- page-2 final Continue -> safe PARK

There are no remaining intro-input branch coverage gaps for this milestone.

## PASS result

Validated on real CYD:

- prompt hitbox accepts valid bottom-band touches
- out-of-band touch is rejected without state/RAM change
- `More` reaches page-0 text 1
- reveal-on-tap works on page-0 text 0, page-0 text 1 and page 2
- `Continue` reaches animated page 1
- page 1 auto-advances after its original ~10-second duration
- page 1 can also be skipped immediately by semantic touch
- both page-1 exits correctly rebase the local virtual-time epoch
- final Continue parks at page 2 instead of disposing/loading
- no reset/crash
- no unexpected failure park
- no input-time or frame-time heap/largest-block delta
- intro assets remain resident at final PARK
- `shapeData == NULL` and `mediaTexels == NULL`
- `DoomCanvas_run()` is never entered
- `DoomCanvas_disposeIntro()` is not called
- `DoomCanvas_loadMap()` is not called

## Historical next boundary at merge time

At the time PR #39 merged, the next bounded increment was defined as intro
disposal / loading handoff:

```text
final PARK at ST_INTRO page 2
    -> measure resources/RAM
    -> dispose intro assets deliberately
    -> verify reclaimed RAM
    -> stop before or at the first tightly-controlled gameplay-map load boundary
```

The first gameplay map must not be allowed to resurrect the old monolithic
`shapeData` / `mediaTexels` architecture.

That section is retained as the milestone's historical handoff. The live roadmap
is in [`PORTING_STATUS.md`](PORTING_STATUS.md).
