# Doom RPG ESP32 CYD porting status

This file is the recovery point for the ESP32-2432S028R classic Cheap Yellow
Display port. Update it on the same branch as every hardware-validated increment,
before merge.

## Target

- ESP32-2432S028R / classic CYD, no PSRAM
- ESP32-D0WD-V3, dual core, 240 MHz
- 4 MB flash
- ILI9341 320x240 landscape
- XPT2046 touch
- microSD-backed game data
- internal framebuffer: 160x120 RGB565 = 38,400 B
- physical output: exact nearest-neighbour 2x to 320x240
- audio still disabled during bring-up

## Project direction

DoomRPG-RE is an executable specification for behaviour, data formats and useful
rendering semantics, not an architecture contract. The ESP32 port is becoming its
own constrained engine:

- bounded deterministic RAM use
- SD as immutable backing store
- measured working sets and evidence-driven caches
- no monolithic `shapeData`
- no map-wide `mediaTexels`
- one shared 160x120 RGB565 framebuffer
- original BSP, projection, game and menu behaviour preserved where useful
- ESP32-specific presentation/resource architecture where the original design is
  unsuitable for the target
- one small hardware-validated subsystem per increment

Core philosophy:

> We are no longer forcing DoomRPG-RE onto ESP32. We are building an ESP32 Doom
> RPG engine from the behaviour and data model proven by DoomRPG-RE.

## Increment discipline

1. Start from the exact latest hardware-validated `main` SHA.
2. One branch = one small measurable objective.
3. Build/flash/test on the real CYD.
4. Fix failures on the same branch.
5. Update all relevant `.md` files on that branch before merge.
6. Only when code, hardware evidence and documentation agree is the branch merge-ready.
7. Merge.
8. Only after merge acknowledgement start the next increment.

Documentation is part of the increment, not a later cleanup task.

## Important merged milestones

- native 160x120 framebuffer + exact 2x TFT output
- real engine object graph / HUD / MenuSystem / Render startup
- real `menu.bsp` structural load stopped before legacy graphics inflation
- native asset pack v2, 241 random-access resources
- zero resident `shapeData`
- zero map-wide `mediaTexels`
- bounded GFXRM wall/sprite frames
- native projected wall + sprite rasterization
- real `menu.bsp` BSP traversal, camera and visible scene
- wall LRU3: 25 logical requests -> 11 physical loads
- sprite LRU3: 11 logical requests -> 9 physical loads
- native scene framebuffer `ffe0995e`
- fitted 160x120 `MENU_MAIN` geometry / layout FNV `47b3656e`
- touch-ready hand-only main menu with real XPT2046 selection
- released double-tap confirmation
- real Options action through `MenuSystem_select()`
- real `MENU_MAIN_OPTIONS` model (`Back / Video / Input / Sound`)
- real Back action through `MenuSystem_back()`
- opaque black main-menu presentation with deterministic touch hashes
- fast Options -> Main return without replaying MENUWALL/MENUSPRITE
- fast-menu branch merged as PR #32 at
  `cc2cb40cf026b5a5e232dba67f884905aca42488`
- normal/bring-up boot split and loading-bar flicker suppression merged as PR #33
  at `1035d4413686624feb07aaf208821946cead5869`
- permanent bring-up hitbox overlay merged as PR #34 at
  `2b29ca7f3479c9add022ccc803bdbff7dd5ade34`
- hardware-selected CYD color profile merged as PR #35 at
  `a87b50747fa69bba6624870f944cbb1111014276`
- fresh Start Game entry / lifecycle cleanup merged as PR #36 at
  `5275e4a1c6eca703b51221e80f3b199178015a01`
- first fitted deterministic `ST_INTRO` frame merged as PR #37 at
  `b934e21c7f2dbf6463a4d2dfa13d1e06614e2b96`
- bounded 50 ms multi-frame intro clock merged as PR #38 at
  `58edfe5d7080a7e9e64ff5b516697ddf3cca31da`

## Current increment

Branch: `agent/esp32-intro-input`

Base `main` SHA:

```text
58edfe5d7080a7e9e64ff5b516697ddf3cca31da
```

Status: **HARDWARE PASS; DOCUMENTED; MERGE-READY**.

Objective: add bounded semantic touch progression through the intro while keeping
`DoomCanvas_run()`, intro disposal and gameplay map loading unreachable.

Current recovery contract:

```text
entry fitted intro FNV = 56438966
clock step             = 50 ms nominal
heap8                  = 50656 stable
largest8               = 13300 stable
state                   = ST_INTRO (9)
menu                    = MENU_NONE
story pages             = bounded to 0 / 1 / 2
shapeData               = NULL
mediaTexels             = NULL
wall/sprite caches      = inactive
intro assets            = resident through final PARK
DoomCanvas_run          = NOT called
DoomCanvas_disposeIntro = NOT called
DoomCanvas_loadMap      = NOT called
```

The 16-byte difference from the merged intro-clock build (`50672 -> 50656`) is a
build-to-build baseline difference after adding the input state/callback code.
Every measured frame and every measured input transition retained zero
heap/largest-block delta.

## Start Game state-machine contract inherited from PR #36

The native ESP32 menu boot deliberately bypasses the original heavy
`MenuSystem_setMenu()` presentation path. Hardware testing exposed one missing
semantic side effect: the model was already `MENU_MAIN`, but `DoomCanvas.state`
was still the initial `ST_LEGALS` value 0.

That made direct Start fail until an Options -> Back round trip happened to call
the original menu transition and establish `ST_MENU`.

The native interactive-main activation explicitly restores the same state
contract:

```text
MENU_MAIN visible + touch armed
    => DoomCanvas.state == ST_MENU (2)
```

Hardware boot marker:

```text
[MENUTOUCH] STATE SYNC canvas=0->2 source=native-MENU_MAIN activation
```

This is a state-machine correction, not a relaxation of Start preconditions.
The Start helper still requires:

```text
menu      = MENU_MAIN
selected  = 0
state     = ST_MENU
frame FNV = 58a11171
shapeData = NULL
mediaTexels = NULL
wall/sprite caches inactive
```

## Hardware touch evidence / Start tolerance

A deliberate real-CYD Start tap landed at logical `y=64`, three pixels above the
visible first row beginning at `y=67`.

Only Start receives this extra top tolerance:

```text
Start Game logical x=28..119 y=64..78
Options    logical x=28..119 y=79..90
Help/About logical x=28..119 y=91..102
Exit       logical x=28..119 y=103..114
```

The other rows remain unchanged and non-overlapping.

## Real fresh Start path validated in PR #36

On a first-boot profile with no compatible save, confirmed Start executes the
original action:

```text
MENU_MAIN / item 0
    -> MenuSystem_select()
    -> Menu_select(MENU_MAIN, 0)
    -> Menu_startGame(menu, 1)
    -> Player_reset()
    -> DoomCanvas_setState(ST_INTRO)
    -> DoomCanvas_loadPrologueText()
```

The first attempted hardware run reached the real intro loader but failed on
`c.bmp` because the old ZIP path temporarily needed compressed data,
uncompressed data and a 10,992-byte miniz inflate state while only 29,064 bytes
of 8-bit heap remained.

The crash was an intentional `DoomRPG_Error()` abort/reboot, not a logical return
to the menu.

## Fresh-start lifecycle cleanup

The native boot had two large classes of memory still resident even though a
fresh Start transition no longer needs them:

1. `imgLegals` / `g.bmp`, because the ESP32 boot skips the original legal-screen
   state machine that normally frees it before handing control to the menu;
2. menu map runtime + mapping tables, because the opaque ESP32 menu no longer
   needs `menu.bsp` after New Game is irreversibly confirmed.

For the fresh-profile path only, before calling the original `MenuSystem_select()`:

```text
DoomRPG_freeImage(imgLegals)
Render_freeRuntime(render)
Game_unloadMapData(game)
```

This uses the same runtime cleanup order already used by the original map loader.
The existing-save path is detected first and deliberately keeps menu resources
resident because it must later paint the Continue / New Game menu.

Hardware measurement:

```text
before cleanup:
  heap8    = 29064
  largest8 = 17396

after cleanup:
  heap8    = 84480
  largest8 = 36852
  gained   = 55416 B

nodes       = NULL
lines       = NULL
mapSprites  = NULL
mappings    = NULL / NULL
shapeData   = NULL
mediaTexels = NULL
```

This is the required fresh-Start lifecycle boundary.

## Intro resource plan and successful load

`DoomCanvas_loadPrologueText()` loads four original indexed BMP assets:

```text
c.bmp -> imgSpaceBG
  ZIP compressed   = 3675 B
  uncompressed BMP = 12408 B
  packed pixels    = 12288 B, 192x128, 4-bpp, palette 16

d.bmp -> imgLinesLayer
  ZIP compressed   = 149 B
  uncompressed BMP = 12356 B
  packed pixels    = 12288 B, 192x128, 4-bpp, palette 3

e.bmp -> imgPlanetLayer
  ZIP compressed   = 1352 B
  uncompressed BMP = 8312 B
  packed pixels    = 8192 B, 128x128, 4-bpp, palette 16

f.bmp -> imgSpaceship
  ZIP compressed   = 114 B
  uncompressed BMP = 160 B
  packed pixels    = 45 B, 9x9, 4-bpp, palette 8
```

ZIP totals are `5290 B` compressed / `33236 B` uncompressed, but the legacy ZIP
loader peak is per-file. After the lifecycle cleanup, all four assets loaded
successfully through the existing packed indexed ESP32 BMP path.

No direct `.pak` intro-image migration was needed. The native pack remains the
preferred future path when another legacy ZIP allocation peak becomes unsuitable.

## Start Game hardware evidence

Validated normal-firmware transition before the first intro draw:

```text
[MAINSTART] Begin menu=1 selected=0 state=2 framebufferFNV=58a11171
[MAINSTART] Existing-save precheck=no -> fresh cleanup allowed
[MAINSTART] Fresh-start cleanup ... heap8=29064->84480 gained=55416
...
[ZIP] inflate c.bmp ...
[SDL] Adopt packed indexed texture 192x128 bpp=4 bytes=12288 palette=16
[ZIP] inflate d.bmp ...
[SDL] Adopt packed indexed texture 192x128 bpp=4 bytes=12288 palette=3
[ZIP] inflate e.bmp ...
[SDL] Adopt packed indexed texture 128x128 bpp=4 bytes=8192 palette=16
[ZIP] inflate f.bmp ...
[SDL] Adopt packed indexed texture 9x9 bpp=4 bytes=45 palette=8
...
[MAINSTART] After select menu=0 state=9 framebufferFNV=485915c5
             heap8=50712 largest8=13300
[MAINSTART] READY real MenuSystem_select -> Menu_startGame(new)
            -> Player_reset -> ST_INTRO
```

Fresh player reset contract remained exact:

```text
level           = 1
currentXP       = 0
nextLevelXP     = 80
credits         = 0
keys            = 0
ammo[1]         = 8
weapon          = 2
weapons         = 0x00000004
disabledWeapons = 0
totalDeaths     = 0
```

The three prologue text allocations are non-NULL and the story counters are at
the initial page:

```text
storyText1[0] != NULL
storyText1[1] != NULL
storyText2     != NULL
storyPage      = 0
storyTextPage  = 0
```

## First bounded ST_INTRO frame (PR #37)

After the fresh Start action reaches the black `ST_INTRO` boundary, the ESP32
first-frame helper establishes a deterministic local epoch:

```text
canvas.time          = 0
canvas.storyTextTime = 0
canvas.storyAnimTime = 0
canvas.showTextDone  = false
```

It executes exactly once:

```text
Esp32StoryFit_draw(canvas)
DoomRPG_flushGraphics(doomRpg)
```

There is no input dispatch, no story-page advancement and no gameplay map load.

### Pre-fit hardware baseline

The first implementation called the original 128x128 story geometry directly:

```text
frame FNV    = 6cf52a3e
heap8        = 50712 -> 50712
largest8     = 13300 -> 13300
deltaHeap    = 0
deltaLargest = 0
```

The engine/state/RAM result was correct, but the 128x128 story square maps to
`y=-4..123` around `SCR_CY=60`, so four logical pixels were cropped at both the
top and bottom of the 120-high framebuffer. `More` / `Continue` also extended
past the bottom edge.

`6cf52a3e` is retained as the pre-fit recovery hash only.

### ESP32-native 128 -> 120 story fit

The current renderer keeps Doom RPG's 128x128 coordinates as a virtual story
space and maps them directly into a centered 120x120 viewport:

```text
virtual story space : 128x128
ESP32 viewport      : 120x120
viewport origin     : x=20, y=0
logical framebuffer : 160x120
physical TFT        : exact 2x -> 320x240
```

The fit applies to the scrolling background, progressive Doom font, hand,
`More` / `Continue`, animated layers, spaceship, laser lines and clip rectangle.

There is no intermediate 128x128 framebuffer and no fit-time heap allocation.
The packed indexed textures are sampled directly into the shared RGB565
framebuffer by the existing ESP32 `SDL_RenderCopy()` scaler.

Hardware marker:

```text
[INTROFIT] virtual=128x128 -> viewport=120x120@(20,0) direct-to-framebuffer; no intermediate buffer
```

### Final first-frame hardware evidence

Validated fitted normal-firmware result:

```text
[INTRO1] Begin state=9 menu=0 page=0 textPage=0 frameFNV=485915c5 expectedEntry=485915c5 heap8=50704 largest8=13300
[INTROFIT] virtual=128x128 -> viewport=120x120@(20,0) direct-to-framebuffer; no intermediate buffer
[INTRO1] Drawn t=0 frameFNV=56438966 heap8=50704 largest8=13300 deltaHeap=0 deltaLargest=0 state=9 page=0 textPage=0
[VIDEO] Present 160x120 -> 320x240 exact 2x + sat1.15: 42761 us
[INTRO1] READY one deterministic ST_INTRO frame presented once FNV=56438966
[ALIVE] uptime=10008 ms heap=116468 heap8=50704 largest8=13300 ...
```

The 8-byte difference between the earlier `50712` Start boundary build and the
final `50704` fitted build is a build-to-build baseline difference. The critical
frame contract is unchanged before/after the draw.

Hardware visual validation confirmed the full fitted viewport and the complete
hand + `More` presentation are visible on the CYD.

## Bounded ESP32 intro clock hardware evidence (PR #38)

The clock arms immediately after the validated `t=0` fitted frame. Arduino
`loop()` services at most one due virtual tick per pass.

Clock model:

```text
virtual step = 50 ms
nominal virtual pacing ~= 20 FPS
one render maximum per loop service call
stale ticks are skipped, never catch-up rendered
```

Each due tick still executes only:

```text
Esp32StoryFit_draw(canvas)
DoomRPG_flushGraphics(doomRpg)
```

Hardware sequence:

```text
[INTROCLK] ARMED step=50 ms startFNV=56438966 heap8=50672 largest8=13300 wallStart=1313
[INTROCLK] frame=1 tick=1 t=50 FNV=da9cd50e heap8=50672 largest8=13300 skipped=0 textDone=0
[INTROCLK] frame=2 tick=2 t=100 FNV=c63cf367 heap8=50672 largest8=13300 skipped=0 textDone=0
[INTROCLK] frame=3 tick=4 t=200 FNV=2620e850 heap8=50672 largest8=13300 skipped=1 textDone=0
[INTROCLK] frame=14 tick=20 t=1000 FNV=e76fec13 heap8=50672 largest8=13300 skipped=6 textDone=0
[ALIVE] uptime=5020 ms heap=116436 heap8=50672 largest8=13300 ...
```

Validated clock RAM contract:

```text
heap8        50672 -> 50672 per rendered frame
largest8     13300 -> 13300 per rendered frame
deltaHeap    0
deltaLargest 0
```

Measured pacing at virtual `t=1000 ms`:

```text
ticks elapsed     = 20
frames rendered   = 14
ticks skipped     = 6
effective render  ~= 14 FPS
Present           ~= 42.77..42.84 ms
```

The user's visual observation that the intro feels somewhat slow is consistent
with these measurements. Before changing pacing or presentation, compare against
the original J2ME version. If optimization is required later, the measured
`PlatformVideo_present()` path and final saturation 1.15 transform are the first
candidates.

## Bounded intro input hardware evidence

The current branch uses the existing platform touch callback instead of creating
another XPT2046 poller. The platform delivers one semantic tap on the press edge,
then requires a stable 50 ms release before another semantic tap can be emitted.

Correct touch contract:

```text
press -> one semantic tap callback
hold  -> no repeat
stable release 50 ms -> rearm
```

Text-page touch geometry:

```text
story viewport = x20..139 y0..119
prompt band    = x20..139 y102..119
```

A real out-of-band touch was rejected without changing state/RAM:

```text
physical=115,59 logical=57,29 page=0 textPage=0 textDone=0 accepted=0
[INTROIN] MISS n=1 page=0 promptBandY=102..119
```

Current-build baseline:

```text
first fitted FNV = 56438966
heap8            = 50656
largest8         = 13300
```

The first clock checkpoint hashes stayed identical to PR #38:

```text
t=50 ms    da9cd50e
t=100 ms   c63cf367
t=200 ms   2620e850
t=1000 ms  e76fec13
```

Hardware-validated end-to-end sequence:

```text
[INTROCLK] TEXT DONE page=0 textPage=0 tick=78 t=3900 ...
[INTROIN] MORE textPage=0->1 t=5600 textEpoch=5600
[INTROIN] READY page=0 textPage=1 textDone=0 heap8=50656 largest8=13300

[INTROIN] REVEAL page=0 textPage=1 t=7400
[INTROIN] READY page=0 textPage=1 textDone=1 heap8=50656 largest8=13300

[INTROIN] CONTINUE storyPage=0->1 t=9150 epoch=9150
[INTROIN] READY page=1 textPage=0 textDone=0 heap8=50656 largest8=13300

[INTROCLK] AUTO-PAGE 1->2 t=19200 textPage=0 epoch=19200
[INTROCLK] TEXT DONE page=2 textPage=0 tick=453 t=22650 ...

[INTROIN] FINAL-CONTINUE page=2 textPage=0 t=23650
[INTROCLK] PARK reason=intro-exit-ready tick=473 frames=282 skipped=191 state=9 page=2 textPage=0 heap8=50656 largest8=13300
[INTROIN] READY-TO-EXIT state=9 page=2 textPage=0 heap8=50656 largest8=13300 assets=retained noDispose=yes noMapLoad=yes
[TOUCH] ...
```

Page 1 ran from virtual epoch `9150` to automatic transition `19200`, i.e. 10.05
seconds; the extra 50 ms is the expected clock quantization.

The `[TOUCH]` marker after final PARK shows that the Arduino loop continued instead
of resetting or entering the map loader. Stable 5-second heartbeats were also
observed throughout the long run.

No unexpected `[INTROIN] FAILED` or `[INTROCLK] PARK` occurred before the intended
final `intro-exit-ready` park.

Alternate-path coverage note: the pasted run used the natural page-1 timeout and
did not capture the optional `SKIP-ANIM` touch branch. Early reveal on page-0 text
0 and page-2 text was also not captured; those text pages use the same
`showTextDone` mutation hardware-validated on page-0 text 1. These gaps are
recorded rather than silently claimed.

At the final safe boundary:

```text
state                   = ST_INTRO (9)
storyPage               = 2
storyTextPage           = 0
intro clock             = inactive
intro input             = inactive
intro images/texts      = retained
DoomCanvas_run          = NOT called
DoomCanvas_disposeIntro = NOT called
DoomCanvas_loadMap      = NOT called
shapeData               = NULL
mediaTexels             = NULL
```

## Hardware-selected display profile

Current hardware-selected output remains:

```text
gamma       = 1.00
saturation  = 1.15
resampling  = nearest
```

The logical framebuffer remains the source of truth:

```text
engine / palettes / GFXRM
        -> 160x120 RGB565 framebuffer
        -> deterministic engine/menu/render FNV hashes
        -> CYD presentation transform (saturation 1.15)
        -> exact nearest-neighbour 2x
        -> ILI9341
```

Normal full-screen presentation remains about 42.7 ms. During the active intro
clock, repeated hardware measurements were about 42.77..42.84 ms. Bring-up
presentation with physical touch overlay is about 44.3 ms.

## Two PlatformIO modes remain the architecture boundary

### `esp32-cyd`

Everyday optimized firmware:

```bash
cd ESP32
pio run -e esp32-cyd -t upload
pio device monitor -e esp32-cyd
```

Normal boot skips historical resource/render proof passes and compiles no hitbox
overlay path.

### `esp32-cyd-bringup`

Diagnostic laboratory:

```bash
cd ESP32
pio run -e esp32-cyd-bringup -t upload
pio device monitor -e esp32-cyd-bringup
```

Adds:

```text
DOOMRPG_ESP32_BRINGUP_PROBES=1
DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY=1
```

Both permanent environments use the same hardware-selected CYD color profile.

## MENU_MAIN / Options recovery references

Active opaque main menu:

```text
model FNV                     = bbc2149b
layout FNV                    = 47b3656e
black + scaled logo           = 0ac1f9c6
Start Game selected           = 58a11171
Options selected              = 0cf107b1
Help/About selected           = 9db82b71
Exit selected                 = bdd775f9
MENU_MAIN_OPTIONS model       = e1ef01f7
MENU_MAIN_OPTIONS framebuffer = 6058d47d
```

Options Back remains:

```text
Back logical  x=15..119 y=65..78
Back physical x=30..239 y=130..157
```

## Normal menu-loader boundary before Start

Before Start is confirmed, normal boot still executes:

```text
Render_beginLoadMap(MAP_MENU)
Render_beginLoadMapData()
```

The seventh original loading-bar callback is the validated stop point after
runtime structures are resident but before legacy monolithic graphics loading.

Hardware runtime contract while MENU_MAIN is active:

```text
nodes          = 53
lines          = 120
mapSprites     = 44
runtimeSprites = 68
events         = 15
mapTextures    = 84
mapSpriteRefs  = 284
planeTextures  = 11
persistent used= 14092 B
```

Those map/runtime resources are intentionally released only after an irreversible
fresh Start confirmation.

## Native graphics recovery references

These contracts are normally exercised only by `esp32-cyd-bringup`:

```text
sprite 172 texel FNV          = 0c0a7acd
wall 112 texel FNV            = 92d40704
synthetic projected wall FNV  = ad191f54
real walls framebuffer        = a6d87c4a
viewSprites list FNV          = 962cd657
sprite request FNV            = 4457ac94
walls + sprites framebuffer   = ffe0995e
faithful original MENU_MAIN   = 86c38260
historical fitted MENU_MAIN   = 1afa0223
failed double-gray wall frame = b6f86faa
```

Validated cache evidence:

```text
Wall LRU3
  requests 25
  hits 14
  misses 11
  evictions 8
  peak payload 6144 B

Sprite LRU3
  requests 11
  hits 2
  misses 9
  evictions 6
  peak logical payload 6038 B
```

## Current safe boundary

Hardware validated:

- real core/layout/pre-render startup
- real Render startup
- real config + mappings
- real menu runtime structures through the pre-bitshape boundary
- bounded native wall/sprite render and cache regressions in bring-up
- opaque deterministic `MENU_MAIN`
- released double-tap menu touch semantics
- permanent bring-up hitbox overlay
- real Options action through `MenuSystem_select()`
- real Back through `MenuSystem_back()`
- fast opaque Back repaint
- no historical loading-bar flicker in normal mode
- hardware-selected color profile: gamma 1.00 / saturation 1.15 / nearest
- native MENU_MAIN state synchronized to `ST_MENU`
- fresh direct Start Game through the real `MenuSystem_select()` path
- real `Player_reset()` contract
- dead legal/menu runtime cleanup before intro allocation
- all four original intro BMP assets resident successfully
- real `ST_INTRO` state reached with prologue text loaded
- deterministic bounded first `ST_INTRO` frame presented once
- native 128x128 -> centered 120x120 story fit
- fitted first-frame FNV `56438966`
- ESP32-owned 50 ms multi-frame `ST_INTRO` clock
- representative clock hashes `da9cd50e`, `c63cf367`, `2620e850`, `e76fec13`
- bounded skipped-tick behavior under current ~42.8 ms presentation cost
- semantic intro press-edge touch with stable-release rearm
- prompt-band rejection and accepted `More` / `Continue` touches
- reveal-on-tap hardware validated
- page 0 -> page 1 transition
- natural page 1 -> page 2 transition with virtual epoch rebase
- final Continue parks at `ST_INTRO`, page 2
- current heap8 `50656` stable across measured frames/input
- current largest8 `13300` stable
- intro assets retained at final PARK
- no map/runtime resurrection
- no `DoomCanvas_run()` handoff
- no `DoomCanvas_disposeIntro()` at final Continue
- no `DoomCanvas_loadMap()` at final Continue
- `shapeData == NULL`
- `mediaTexels == NULL`

Still intentionally deferred:

- optional page-1 touch-skip hardware regression capture
- intro disposal / transition to loading
- first gameplay/map load after the intro
- existing-save Continue / New Game submenu painter and action
- Video/Input/Sound actions
- Help/About and Exit real actions
- active normal gameplay loop
- gameplay controls
- possible optimization of saturation/presentation cost after J2ME comparison
- audio

## Next milestone

After merge, the next bounded increment starts from the new `main` at the final
intro PARK and owns only disposal/loading preparation:

```text
ST_INTRO page 2 final PARK
    -> measure resident intro resources
    -> dispose intro assets/text deliberately
    -> measure reclaimed heap/largest block
    -> stop before or at the first tightly guarded gameplay-map load boundary
```

The first gameplay load must not resurrect monolithic `shapeData` or map-wide
`mediaTexels`.
