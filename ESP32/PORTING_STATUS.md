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

## Current increment

Branch: `agent/esp32-first-intro-frame`

Base `main` SHA:

```text
5275e4a1c6eca703b51221e80f3b199178015a01
```

Status: **HARDWARE PASS; DOCUMENTED; MERGE-READY**.

Objective: advance exactly one bounded step beyond the validated fresh Start
boundary: render and present one deterministic real `ST_INTRO` frame, adapt the
legacy 128x128 story presentation to the 160x120 CYD framebuffer, and park before
starting an active intro clock, processing intro input or loading gameplay.

Final fitted first-frame recovery contract:

```text
entry framebuffer FNV = 485915c5
fitted intro FNV       = 56438966
heap8                  = 50704 -> 50704
largest8               = 13300 -> 13300
deltaHeap              = 0
deltaLargest           = 0
state                   = ST_INTRO (9)
storyPage               = 0
storyTextPage           = 0
shapeData               = NULL
mediaTexels             = NULL
wall/sprite caches      = inactive
```

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
   state machine that normally frees it before entering the menu;
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

## First bounded ST_INTRO frame

The current increment intentionally does not enter `DoomCanvas_run()`. After the
fresh Start action has reached the black `ST_INTRO` boundary, the ESP32 helper
establishes a deterministic local epoch:

```text
canvas.time          = 0
canvas.storyTextTime = 0
canvas.storyAnimTime = 0
canvas.showTextDone  = false
```

It then executes exactly once:

```text
Esp32StoryFit_draw(canvas)
DoomRPG_flushGraphics(doomRpg)
```

and parks.

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
=== Doom RPG ESP32 bounded first ST_INTRO frame ===
[INTRO1] Begin state=9 menu=0 page=0 textPage=0 frameFNV=485915c5 expectedEntry=485915c5 heap8=50704 largest8=13300
[INTROFIT] virtual=128x128 -> viewport=120x120@(20,0) direct-to-framebuffer; no intermediate buffer
[INTRO1] Drawn t=0 frameFNV=56438966 heap8=50704 largest8=13300 deltaHeap=0 deltaLargest=0 state=9 page=0 textPage=0
[VIDEO] Present 160x120 -> 320x240 exact 2x + sat1.15: 42761 us
[INTRO1] READY one deterministic ST_INTRO frame presented once FNV=56438966
[INTRO1] PARK state=9 page=0 textPage=0; no DoomCanvas_run, no input dispatch, no map load
[MAINSTART] READY first ST_INTRO frame rendered/presented; engine remains parked
[ALIVE] uptime=10008 ms heap=116468 heap8=50704 largest8=13300 SD=ready ZIP=ready VIDEO=ready CORE=ready LAYOUT=ready PRERENDER=ready RENDER=ready MAPPINGS=ready MENUBSP=ready touchIRQ=idle
```

The 8-byte difference between the earlier `50712` Start boundary build and the
final `50704` fitted build is a build-to-build baseline difference. The critical
frame contract is unchanged before/after the draw:

```text
heap8        50704 -> 50704
largest8     13300 -> 13300
deltaHeap    0
deltaLargest 0
```

Hardware visual validation confirms the full fitted viewport and the complete
hand + `More` presentation are visible on the CYD.

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

Normal full-screen presentation remains about 42.7 ms; the fitted first intro
frame measured 42.761 ms. Bring-up presentation with physical touch overlay is
about 44.3 ms. This cost should be re-measured when the active intro/game loop
begins.

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
- released double-tap touch semantics
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
- final fitted intro framebuffer FNV `56438966`
- first-frame heap8 `50704 -> 50704`, largest8 `13300 -> 13300`
- no map/runtime resurrection during the frame
- `shapeData == NULL`
- `mediaTexels == NULL`

Still intentionally deferred:

- ESP32-owned timed multi-frame `ST_INTRO` progression
- intro touch/key progression (`More` / `Continue`)
- intro disposal / transition to loading
- first gameplay/map load after the intro
- existing-save Continue / New Game submenu painter and action
- Video/Input/Sound actions
- Help/About and Exit real actions
- active normal multi-frame game loop
- gameplay controls
- possible optimization of saturation cost if active frame pacing needs it
- audio
