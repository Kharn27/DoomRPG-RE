# Doom RPG ESP32 port

This directory contains the ESP32-specific Doom RPG engine/port for the classic
ESP32-2432S028R Cheap Yellow Display with **no PSRAM**.

DoomRPG-RE is used as a behavioural/data-format reference while rendering,
resource management, timing and UI are progressively rebuilt around the real
target constraints.

For the exact hardware recovery point, branch status and regression hashes, see
[`PORTING_STATUS.md`](PORTING_STATUS.md).

## Current target

- ESP32-2432S028R / classic CYD
- ESP32-D0WD-V3, 240 MHz
- 4 MB flash
- no PSRAM
- ILI9341 320x240 landscape
- XPT2046 touch
- internal RGB565 framebuffer: 160x120 = 38,400 B
- exact nearest-neighbour 2x output to 320x240
- microSD-backed resources
- audio disabled during bring-up

## PlatformIO environments

### `esp32-cyd` — normal firmware

```bash
cd ESP32
pio run -e esp32-cyd -t upload
pio device monitor -e esp32-cyd
```

Normal boot runs the real engine initialization required to reach the interactive
menu and current bounded intro path. Historical graphics/resource proof probes are
skipped.

### `esp32-cyd-bringup` — diagnostic laboratory

```bash
cd ESP32
pio run -e esp32-cyd-bringup -t upload
pio device monitor -e esp32-cyd-bringup
```

Bring-up adds:

```text
DOOMRPG_ESP32_BRINGUP_PROBES=1
DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY=1
```

Use it for asset/resource regressions, memory budgets, wall/sprite consumers,
projection, LRU cache contracts and touch calibration.

## Hardware-selected CYD display profile

```text
gamma       = 1.00
saturation  = 1.15
resampling  = nearest
```

The correction is applied only at the final TFT presentation boundary:

```text
engine / palettes / GFXRM
        -> logical 160x120 RGB565 framebuffer
        -> deterministic FNV hashes
        -> saturation 1.15 output transform
        -> exact nearest-neighbour 2x
        -> ILI9341
```

Representative hardware timing:

```text
normal full-screen Present          ~= 42.7 ms
active intro Present                ~= 42.7 ms
bring-up Present + physical overlay ~= 44.3 ms
old neutral Present                 ~= 34.4 ms
```

The current 50 ms intro clock therefore does not physically render every nominal
20 FPS tick. Hardware measures about 14 rendered FPS with bounded skipped ticks.
The virtual story timeline remains correct. Compare against the original J2ME
pacing before optimizing; `PlatformVideo_present()` and the saturation transform
are the first measured candidates if optimization is later warranted.

## Engine direction

The original map-wide graphics pools do not fit the no-PSRAM target. The ESP32
path uses bounded resources and measured caches:

```text
                 SD / DoomRPG-ESP32.pak
                          |
                          v
                        GFXRM
                      /       \
            sprite frames     wall frames
                 |                 |
       Sprite LRU cache(3)    Wall LRU cache(3)
                 |                 |
                 v                 v
          projected sprites   projected walls
                  \             /
                   \           /
                    160x120 RGB565
                          |
                          v
                CYD output transform
                          |
                          v
                    TFT exact x2
```

Permanent graphics-memory invariants remain:

```text
shapeData   = NULL
mediaTexels = NULL
```

Stable bring-up references include:

```text
sprite 172 texel FNV         = 0c0a7acd
wall 112 texel FNV           = 92d40704
synthetic projected wall FNV = ad191f54
real walls framebuffer       = a6d87c4a
viewSprites list FNV         = 962cd657
sprite request FNV           = 4457ac94
walls + sprites framebuffer  = ffe0995e
```

## Normal boot sequence

```text
Platform video / SD / ZIP
    -> engine core + layout
    -> ParticleSystem / MenuSystem / EntityDef startup
    -> Render_startup
    -> config + mappings
    -> Render_beginLoadMap(MAP_MENU)
    -> structural menu-map load
    -> stop before legacy bitshape/texel inflation
    -> direct opaque MENU_MAIN
    -> synchronize DoomCanvas to ST_MENU
    -> touch armed
    -> READY
```

The native menu intentionally bypasses the original expensive painter but
preserves the state-machine contract.

## Main menu and touch

The real model is preserved:

```text
Start Game
Options
Help/About
Exit
```

Hardware hashes:

```text
MENU_MAIN model      = bbc2149b
MENU_MAIN layout     = 47b3656e
black + logo         = 0ac1f9c6
Start Game selected  = 58a11171
Options selected     = 0cf107b1
Help/About selected  = 9db82b71
Exit selected        = bdd775f9
Options model        = e1ef01f7
Options framebuffer  = 6058d47d
```

Menu touch uses calibrated XPT2046 points, a semantic press callback and stable
release rearm. Menu confirmation still uses its own double-tap gate.

Start hit geometry:

```text
Start Game logical x=28..119 y=64..78
Options    logical x=28..119 y=79..90
Help/About logical x=28..119 y=91..102
Exit       logical x=28..119 y=103..114
```

## Real fresh Start Game path

Fresh no-save Start executes the original action:

```text
MENU_MAIN / Start Game
    -> MenuSystem_select()
    -> Menu_startGame(new)
    -> Player_reset()
    -> DoomCanvas_setState(ST_INTRO)
    -> DoomCanvas_loadPrologueText()
```

Before that irreversible transition, the native path releases dead legal/menu
runtime:

```text
DoomRPG_freeImage(imgLegals)
Render_freeRuntime(render)
Game_unloadMapData(game)
```

Hardware memory recovery:

```text
heap8     29064 -> 84480
largest8  17396 -> 36852
gained    55416 B
```

The original prologue loader then successfully loads:

```text
c.bmp  192x128 4-bpp packed pixels=12288 B palette=16
d.bmp  192x128 4-bpp packed pixels=12288 B palette=3
e.bmp  128x128 4-bpp packed pixels=8192 B  palette=16
f.bmp  9x9     4-bpp packed pixels=45 B    palette=8
```

Fresh `Player_reset()` remains:

```text
level=1 xp=0 nextXP=80 credits=0 keys=0 ammo[1]=8
weapon=2 weapons=0x00000004 disabledWeapons=0 deaths=0
```

## Fitted first ST_INTRO frame

The original intro assumes a 128x128 story square. The CYD port maps that virtual
space directly into a centered 120x120 viewport:

```text
virtual story space : 128x128
ESP32 viewport      : 120x120
viewport origin     : x=20, y=0
logical framebuffer : 160x120
physical TFT        : exact 2x -> 320x240
```

No intermediate 128x128 framebuffer is used.

Regression contract:

```text
entry FNV        = 485915c5
fitted frame FNV = 56438966
heap8            = 50704 -> 50704
largest8         = 13300 -> 13300
```

Historical cropped pre-fit frame `6cf52a3e` is retained only as a recovery
reference.

## ESP32-owned intro clock

After the validated `t=0` frame, the ESP32 owns a quantized 50 ms virtual clock.
At most one due frame is rendered per Arduino loop pass; stale ticks are skipped
rather than catch-up rendered.

Stable hashes:

```text
t=0     FNV = 56438966
t=50    FNV = da9cd50e
t=100   FNV = c63cf367
t=200   FNV = 2620e850
t=1000  FNV = e76fec13
```

Merged clock-build RAM contract:

```text
heap8        50672 -> 50672
largest8     13300 -> 13300
```

At virtual `t=1000 ms`:

```text
20 ticks elapsed
14 frames rendered
6 ticks skipped
~= 14 FPS rendered
```

## Bounded intro touch progression

The current branch adds semantic intro input without entering the broad legacy
input/game loop.

Platform touch semantics are:

```text
press edge -> one semantic tap callback
hold       -> no repeat
stable release 50 ms -> rearm next tap
```

Text prompts accept touches in:

```text
story viewport = x20..139 y0..119
prompt band    = x20..139 y102..119
```

An out-of-band hardware tap at logical `57,29` correctly produced `MISS` and no
state change.

The bounded story path is:

```text
page 0 text 0
    -> More
page 0 text 1
    -> reveal-on-tap validated
    -> Continue
page 1 animation
    -> natural ~10 s transition OR optional touch skip
page 2 text 0
    -> final Continue
    -> PARK intro-exit-ready
```

Real CYD evidence:

```text
[INTROIN] MORE textPage=0->1 t=5600 textEpoch=5600
[INTROIN] REVEAL page=0 textPage=1 t=7400
[INTROIN] CONTINUE storyPage=0->1 t=9150 epoch=9150
[INTROCLK] AUTO-PAGE 1->2 t=19200 textPage=0 epoch=19200
[INTROIN] FINAL-CONTINUE page=2 textPage=0 t=23650
[INTROCLK] PARK reason=intro-exit-ready tick=473 frames=282 skipped=191 state=9 page=2 textPage=0 heap8=50656 largest8=13300
[INTROIN] READY-TO-EXIT state=9 page=2 textPage=0 heap8=50656 largest8=13300 assets=retained noDispose=yes noMapLoad=yes
```

Current intro-input build RAM remained:

```text
heap8    = 50656
largest8 = 13300
```

across rendering and measured input mutations. The 16-byte difference from the
merged clock build is a build-to-build baseline change, not a per-frame/input
allocation.

At final PARK:

```text
state                 = ST_INTRO (9)
storyPage             = 2
storyTextPage         = 0
intro clock           = inactive
intro input callback  = inactive
intro assets/texts    = retained
DoomCanvas_run        = NOT called
DoomCanvas_disposeIntro = NOT called
DoomCanvas_loadMap    = NOT called
shapeData             = NULL
mediaTexels           = NULL
```

The Arduino loop continued after final PARK; the next touch diagnostic was still
printed instead of a reset/map transition.

### Coverage note

The pasted end-to-end hardware capture used the natural page-1 timeout. The
optional `SKIP-ANIM` touch branch was not captured in that run. Early reveal on
page-0 text 0 and page-2 text were also not captured; they share the exact
`showTextDone` mutation hardware-validated on page-0 text 1.

These alternate branches remain useful regression checks and are documented
rather than silently claimed.

See [`INTRO_INPUT.md`](INTRO_INPUT.md) for the detailed milestone evidence.

## Current memory baselines

```text
interactive normal menu   heap8=29064 largest8=17396
fresh Start after cleanup heap8=84480 largest8=36852
first fitted frame build  heap8=50704 largest8=13300
merged intro-clock build  heap8=50672 largest8=13300
current intro-input build heap8=50656 largest8=13300
```

## Current safe boundary

Hardware validated:

- real engine/menu/render startup
- bounded native wall/sprite architecture and LRU contracts
- opaque deterministic `MENU_MAIN`
- calibrated touch / menu actions
- fresh Start through real `MenuSystem_select()`
- lifecycle cleanup recovering 55,416 B before intro allocation
- original prologue strings + `c/d/e/f.bmp`
- fitted first intro frame FNV `56438966`
- native 128x128 -> centered 120x120 story rendering
- ESP32-owned 50 ms intro clock
- bounded skipped-tick behavior under current TFT cost
- semantic intro touch input with prompt-band gating
- `More`, reveal and `Continue` progression
- natural page 1 -> page 2 transition with local virtual epoch rebase
- final Continue PARK at page 2
- heap8 `50656` stable in the current hardware run
- largest8 `13300` stable
- intro resources retained at final PARK
- no map/runtime resurrection
- no `DoomCanvas_run()` handoff
- no intro disposal or map load yet
- `shapeData == NULL`
- `mediaTexels == NULL`

Still deferred:

- optional page-1 touch-skip hardware regression capture
- bounded intro disposal / transition to loading
- first gameplay/map load after the intro
- existing-save Continue / New Game submenu painter/action
- Video/Input/Sound actions
- Help/About and Exit real actions
- active normal gameplay loop
- gameplay controls
- presentation optimization if J2ME comparison warrants it
- audio

## Next milestone

After this branch is merged, the next increment starts from the new `main` and
owns one narrow boundary only:

```text
final PARK at ST_INTRO page 2
    -> measure resident intro resources/RAM
    -> dispose intro resources deliberately
    -> measure reclaimed RAM
    -> approach the first gameplay map load behind a fresh explicit guard
```

The first gameplay map load must not resurrect monolithic `shapeData` or map-wide
`mediaTexels`.

## Porting workflow

1. create one branch from the latest hardware-validated `main`
2. implement one small measurable objective
3. build/flash/test on the real CYD
4. fix failures on the same branch
5. after hardware PASS update every relevant `.md` on that branch
6. only when code + documentation agree is the branch merge-ready
7. merge
8. only then start the next increment
