# ESP32 bounded intro input

Branch: `agent/esp32-intro-input`

Base hardware-validated `main`:

```text
58edfe5d7080a7e9e64ff5b516697ddf3cca31da
```

Status: **HARDWARE PASS; END-TO-END AUTO PATH DOCUMENTED; MERGE-READY**.

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

The hardware run deliberately proved rejection outside the prompt band:

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

Initial clock hashes remain stable:

```text
t=50 ms    FNV=da9cd50e
t=100 ms   FNV=c63cf367
t=200 ms   FNV=2620e850
t=1000 ms  FNV=e76fec13
```

End-to-end hardware sequence:

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

The final `[TOUCH]` after `READY-TO-EXIT` proves the Arduino loop continued after
PARK instead of resetting or entering the map loader. Earlier 5-second heartbeats
remained stable throughout the long run.

The natural page-1 animation lasted from virtual `t=9150` to `t=19200`, i.e.
10.05 seconds. The extra 50 ms is the expected quantization of the 50 ms clock.

RAM remained:

```text
heap8    = 50656
largest8 = 13300
```

through first frame, text progression, page transition, automatic animation
completion, page-2 text completion and final PARK.

No unexpected `[INTROIN] FAILED` or `[INTROCLK] PARK` occurred before the intended
final `intro-exit-ready` park.

## Hardware coverage note

This captured run validates the complete natural end-to-end path and one active
`REVEAL` transition on page-0 text 1.

The following alternate branches are implemented but were not exercised in the
pasted Serial capture:

- reveal page-0 text 0 before its natural completion
- touch-skip page 1 (`[INTROIN] SKIP-ANIM ...`)
- reveal page-2 text before its natural completion

The two reveal cases share the exact hardware-validated `showTextDone` mutation
used by page-0 text 1. The optional page-1 skip remains a useful regression check
for a future retest, while the natural 10-second transition and the final safe
boundary are hardware validated. These alternate-path coverage gaps are recorded
rather than silently claimed.

## PASS result

Validated on real CYD:

- prompt hitbox accepts valid bottom-band touches
- out-of-band touch is rejected without state/RAM change
- `More` reaches page-0 text 1
- reveal-on-tap works
- `Continue` reaches animated page 1
- page 1 auto-advances after its original ~10-second duration
- virtual-time epochs remain coherent across transitions
- page 2 renders and naturally completes its progressive text
- final Continue parks at page 2 instead of disposing/loading
- no reset/crash
- no unexpected failure park
- no input-time or frame-time heap/largest-block delta
- intro assets remain resident at final PARK
- `shapeData == NULL` and `mediaTexels == NULL`
- `DoomCanvas_run()` is never entered
- `DoomCanvas_disposeIntro()` is not called
- `DoomCanvas_loadMap()` is not called

## Next milestone

After merge, the next bounded increment is intro disposal / loading handoff:

```text
final PARK at ST_INTRO page 2
    -> measure resources/RAM
    -> dispose intro assets deliberately
    -> verify reclaimed RAM
    -> stop before or at the first tightly-controlled gameplay-map load boundary
```

The first gameplay map must not be allowed to resurrect the old monolithic
`shapeData` / `mediaTexels` architecture.
