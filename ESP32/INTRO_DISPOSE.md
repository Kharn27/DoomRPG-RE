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

Status: **IMPLEMENTED; AWAITING REAL-CYD HARDWARE PASS**.

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

`native_intro_input.c` remains unchanged. Its already hardware-validated final
Continue still performs:

```text
[INTROIN] FINAL-CONTINUE ...
[INTROCLK] PARK reason=intro-exit-ready ...
[INTROIN] READY-TO-EXIT ... assets=retained noDispose=yes noMapLoad=yes
```

The clock now remembers whether the PARK reason was **exactly**
`intro-exit-ready`. On the next `Esp32IntroClock_service()` call, and only for
that intentional PARK, it hands the retained `DoomRPG_t*` to
`Esp32IntroDispose_service()`.

Therefore error/failure PARK reasons cannot accidentally destroy intro assets.

The disposal state is one-shot and is reset whenever a fresh intro clock is
armed, so a future second New Game in the same boot cannot inherit a stale
`done` flag.

## Pre-dispose safety gate

The disposer does nothing until all of these are simultaneously true:

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
nodes                   = NULL
lines                   = NULL
mapSprites              = NULL
mediaTexelOffsets       = NULL
mediaBitShapeOffsets    = NULL
mapTextureTexels        = NULL
mapSpriteTexels         = NULL
shapeData               = NULL
mediaTexels             = NULL
wall/sprite caches      = inactive
```

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
frame should remain physically visible while its backing intro resources have
already been released.

## Expected Serial shape

Exact recovered byte counts are intentionally **not predicted**; the real CYD
measurement is the point of this milestone.

Expected sequence after the already validated final Continue:

```text
[INTROCLK] PARK reason=intro-exit-ready ...
[INTROIN] READY-TO-EXIT ... assets=retained noDispose=yes noMapLoad=yes

=== Doom RPG ESP32 bounded intro disposal ===
[INTRODISP] BEGIN state=9 page=2 textPage=0 startupMap=1 frameFNV=........ heap8=..... largest8=..... clip=...
[INTRODISP] CONTRACT mirror DoomCanvas_disposeIntro resources only; DoomCanvas_loadMap is forbidden
[INTRODISP] FREE image=c.bmp/imgSpaceBG ... ptr=0x0
[INTRODISP] FREE image=d.bmp/imgLinesLayer ... ptr=0x0
[INTRODISP] FREE image=e.bmp/imgPlanetLayer ... ptr=0x0
[INTRODISP] FREE image=f.bmp/imgSpaceship ... ptr=0x0
[INTRODISP] FREE text=storyText1[0] ... ptr=0x0
[INTRODISP] FREE text=storyText1[1] ... ptr=0x0
[INTRODISP] FREE text=storyText2 ... ptr=0x0
[INTRODISP] READY state=9 page=3 textPage=0 frameFNV=........->........ heap8=.....->..... recovered=..... largest8=.....->..... assets=NULL texts=NULL clip=off noMapLoad=yes
[INTRODISP] PARK state=9 startupMap=1 shapeData=0x0 mediaTexels=0x0 nodes=0x0 lines=0x0 mapSprites=0x0; next milestone owns map loading
[ALIVE] ...
```

## Hardware PASS criteria

PASS requires all of the following on the classic CYD:

- final Continue still reaches the previously validated `intro-exit-ready` PARK;
- disposal starts only after clock and intro input are inactive;
- `storyPage` advances from 2 to 3;
- all four intro image pointers become NULL;
- all three intro text pointers become NULL;
- `heap8` increases by a measured positive amount;
- `largest8` does not regress;
- framebuffer FNV is identical before/after teardown;
- `shapeData == NULL` and `mediaTexels == NULL` remain true;
- nodes/lines/mapSprites remain NULL;
- wall/sprite caches remain inactive;
- no `DoomCanvas_loadMap()` occurs;
- no reset/crash;
- `[ALIVE]` continues after the disposal PARK.

## Next boundary after hardware PASS

Only after this teardown is measured and documented should the next branch own
the first gameplay-loading step:

```text
ST_INTRO page 3
intro assets/texts = NULL
measured reclaimed RAM available
startupMap = 1
shapeData/mediaTexels = NULL

    -> inspect/guard first gameplay map structural load
```

The next milestone must keep the no-PSRAM architecture rule: no resurrection of
monolithic `shapeData` or map-wide `mediaTexels`.
