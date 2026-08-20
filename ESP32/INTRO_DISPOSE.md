# ESP32 bounded intro disposal

Branch: `agent/esp32-intro-dispose`

Base merged `main`:

```text
98378ce94da6480bbc8939830c0453514d389c82
```

Hardware-affecting recovery inherited from PR #39:

```text
state         = ST_INTRO (9)
storyPage     = 2
storyTextPage = 0
intro clock   = inactive after final Continue
intro input   = inactive after final Continue
intro assets  = resident
heap8         = 50656 on the PR #39 build
largest8      = 13300
shapeData     = NULL
mediaTexels   = NULL
```

Status: **REAL-CYD HARDWARE PASS; DOCUMENTED; MERGE-READY**.

## Objective

Advance exactly one bounded lifecycle boundary after the fully validated intro
input milestone:

```text
final Continue
    -> existing intro-exit-ready PARK
    -> next Arduino loop service
    -> free intro-only images + texts
    -> reset render clip
    -> measure reclaimed RAM
    -> PARK again before gameplay loading
```

This milestone deliberately does **not** call `DoomCanvas_loadMap()`, does not
enter `DoomCanvas_run()`, and does not resurrect any legacy map-wide graphics
pool.

## Original behavior used as specification

The reverse-engineered engine performs the final transition through
`DoomCanvas_changeStoryPage()`:

```text
storyPage 2 -> 3
    -> DoomCanvas_disposeIntro()
```

`DoomCanvas_disposeIntro()` then frees:

```text
imgSpaceBG      / c.bmp
imgLinesLayer   / d.bmp
imgPlanetLayer  / e.bmp
imgSpaceship    / f.bmp
storyText1[0]
storyText1[1]
storyText2
```

and resets the render clip before immediately calling:

```text
DoomCanvas_loadMap(doomCanvas, doomCanvas->startupMap)
```

That final map-load call is the only original behavior intentionally excluded
from this increment.

## ESP32 handoff design

`native_intro_input.c` remains unchanged. Its hardware-validated final Continue
still performs:

```text
[INTROIN] FINAL-CONTINUE ...
[INTROCLK] PARK reason=intro-exit-ready ...
[INTROIN] READY-TO-EXIT ... assets=retained noDispose=yes noMapLoad=yes
```

The clock remembers whether the PARK reason was **exactly**
`intro-exit-ready`. On the next `Esp32IntroClock_service()` call, and only for
that intentional PARK, it hands the retained `DoomRPG_t*` to
`Esp32IntroDispose_service()`.

Therefore error/failure PARK reasons cannot accidentally destroy intro assets.
The disposal state is one-shot and is reset whenever a fresh intro clock is
armed, so a future second New Game in the same boot cannot inherit a stale
`done` flag.

## Pre-dispose safety gate

The disposer requires all of these simultaneously:

```text
intro clock            = inactive
intro input            = inactive
menu                   = MENU_NONE
state                  = ST_INTRO
storyPage              = 2
storyTextPage          = 0
showTextDone           = true
all 4 intro images     = resident
all 3 intro text ptrs  = non-NULL
nodes                  = NULL
lines                  = NULL
mapSprites             = NULL
mediaTexelOffsets      = NULL
mediaBitShapeOffsets   = NULL
mapTextureTexels       = NULL
mapSpriteTexels        = NULL
shapeData              = NULL
mediaTexels            = NULL
wall/sprite caches     = inactive
```

A failed gate is fail-closed and emits one explicit `[INTRODISP] FAILED
precondition ...` diagnostic rather than silently freeing anything.

## Teardown operation

Once the gate passes:

1. record framebuffer FNV, `heap8`, and `largest8`;
2. set `storyPage = 3`, matching the original state transition;
3. free each intro image individually through `DoomRPG_freeImage()`;
4. log heap/largest-block recovery after each image;
5. free and NULL each of the three story text buffers, logging their string byte
   lengths and heap recovery;
6. call `DoomRPG_setClipFalse()`;
7. verify the framebuffer FNV did not change;
8. verify all seven resource pointers are NULL;
9. verify all map/runtime pools and native caches remain absent;
10. require total `heap8` to increase and `largest8` not to decrease;
11. park without loading `startupMap`.

The framebuffer is intentionally left untouched, so the last rendered intro
frame remains physically visible while its backing intro resources have already
been released.

## Real-CYD hardware PASS

Validated normal-firmware run reached the already-proven final intro PARK with:

```text
[INTROIN] FINAL-CONTINUE page=2 textPage=0 t=21100
[INTROCLK] PARK reason=intro-exit-ready tick=422 frames=250 skipped=172 state=9 page=2 textPage=0 heap8=50640 largest8=13300
[INTROIN] READY-TO-EXIT state=9 page=2 textPage=0 heap8=50640 largest8=13300 assets=retained noDispose=yes noMapLoad=yes
```

The next Arduino loop service then entered the bounded disposer:

```text
=== Doom RPG ESP32 bounded intro disposal ===
[INTRODISP] BEGIN state=9 page=2 textPage=0 startupMap=1 frameFNV=a7ee546a heap8=50640 largest8=13300 clip=1
[INTRODISP] CONTRACT mirror DoomCanvas_disposeIntro resources only; DoomCanvas_loadMap is forbidden
```

### Per-resource recovery

Real measured 8-bit heap recovery:

```text
c.bmp / imgSpaceBG
  heap8      50640 -> 63076
  recovered  +12436 B
  largest8   13300 -> 34804

d.bmp / imgLinesLayer
  heap8      63076 -> 75460
  recovered  +12384 B
  largest8   34804 -> 34804

e.bmp / imgPlanetLayer
  heap8      75460 -> 83800
  recovered  +8340 B
  largest8   34804 -> 36852

f.bmp / imgSpaceship
  heap8      83800 -> 83964
  recovered  +164 B
  largest8   36852 -> 36852

storyText1[0]
  string bytes = 153
  heap8        83964 -> 84136
  recovered    +172 B

storyText1[1]
  string bytes = 97
  heap8        84136 -> 84252
  recovered    +116 B

storyText2
  string bytes = 137
  heap8        84252 -> 84408
  recovered    +156 B
```

The measured recovery adds up exactly:

```text
12436
12384
 8340
  164
  172
  116
  156
-----
33768 B total
```

### Final disposal boundary

Hardware result:

```text
[INTRODISP] READY state=9 page=3 textPage=0 frameFNV=a7ee546a->a7ee546a heap8=50640->84408 recovered=33768 largest8=13300->36852 assets=NULL texts=NULL clip=off noMapLoad=yes
[INTRODISP] PARK state=9 startupMap=1 shapeData=0x0 mediaTexels=0x0 nodes=0x0 lines=0x0 mapSprites=0x0; next milestone owns map loading
```

Final contract:

```text
menu                    = MENU_NONE
state                   = ST_INTRO (9)
storyPage               = 3
storyTextPage           = 0
intro clock             = inactive
intro input             = inactive
imgSpaceBG              = NULL
imgLinesLayer           = NULL
imgPlanetLayer          = NULL
imgSpaceship            = NULL
storyText1[0]           = NULL
storyText1[1]           = NULL
storyText2              = NULL
heap8                   = 84408
largest8                = 36852
8-bit heap recovered    = 33768 B
framebuffer FNV         = a7ee546a unchanged across teardown
render clip             = off
nodes                   = NULL
lines                   = NULL
mapSprites              = NULL
mediaTexelOffsets       = NULL
mediaBitShapeOffsets    = NULL
mapTextureTexels        = NULL
mapSpriteTexels         = NULL
shapeData               = NULL
mediaTexels             = NULL
wall/sprite LRU caches  = inactive
DoomCanvas_run          = NOT called
DoomCanvas_loadMap      = NOT called
```

`a7ee546a` is the observed final-frame FNV for this run; the important teardown
invariant is equality before/after disposal, not that every intro run must finish
on this exact hash.

### Stability after disposal

The device remained alive with an unchanged post-dispose memory boundary:

```text
[ALIVE] uptime=30181 ms ... heap8=84408 largest8=36852 ...
[ALIVE] uptime=35182 ms ... heap8=84408 largest8=36852 ...
[ALIVE] uptime=40183 ms ... heap8=84408 largest8=36852 ...
```

No reset, delayed corruption, hidden map transition or resource resurrection was
observed.

## Input UX observation from the validation run

The tester reported a slight feeling of having to insist on `More` on the first
story screen. The Serial evidence does **not** show a rejected hitbox tap:

```text
TAP n=1 logical=101,109 page=0 textPage=0 textDone=0 accepted=1
  -> REVEAL

TAP n=2 logical=113,105 page=0 textPage=0 textDone=1 accepted=1
  -> MORE
```

Both touches were comfortably inside the prompt band `y=102..119`. The current
semantic behavior is therefore behaving as designed: a press while text is still
progressing first completes/reveals that text; a later press advances `More`, and
the platform requires a stable 50 ms release before the next press-edge can be
emitted.

This is recorded as a **non-blocking UX observation**, not a failure of this
lifecycle milestone. If it remains noticeable in later play testing, touch
re-arm/double-tap ergonomics can be tuned separately without changing the story
state machine.

## Hardware PASS result

Every planned criterion passed on the classic no-PSRAM CYD:

- final Continue still reaches the validated `intro-exit-ready` PARK;
- disposal starts only after clock and intro input are inactive;
- `storyPage` advances from 2 to 3;
- all four intro image pointers become NULL;
- all three intro text pointers become NULL;
- `heap8` increases by exactly **33,768 B** on this build;
- `largest8` grows from **13,300 B** to **36,852 B**;
- framebuffer FNV is identical before/after teardown;
- `shapeData == NULL` and `mediaTexels == NULL` remain true;
- nodes/lines/mapSprites remain NULL;
- wall/sprite caches remain inactive;
- no `DoomCanvas_loadMap()` occurs;
- no reset/crash;
- repeated `[ALIVE]` heartbeats remain stable after disposal.

This branch is merge-ready.

## Next bounded milestone after merge

The next branch may finally own the first gameplay-loading step from this exact
hardware-measured boundary:

```text
ST_INTRO page 3
intro assets/texts = NULL
heap8              = 84408
largest8           = 36852
startupMap         = 1
shapeData          = NULL
mediaTexels        = NULL
runtime map pools  = NULL
native caches      = inactive

    -> inspect and guard the first gameplay map structural load
```

The next milestone must keep the no-PSRAM architecture rule: no resurrection of
monolithic `shapeData` or map-wide `mediaTexels`.
