# Doom RPG ESP32 CYD porting status

This file is the **authoritative recovery point** for the classic
ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for the stable build/architecture guide and
[`DOCUMENTATION.md`](DOCUMENTATION.md) for documentation ownership rules.
Detailed milestone logs are preserved in the linked archive files instead of
being recopied here.

## Hardware recovery baseline

Latest hardware-affecting merged baseline:

```text
PR   = #39 — bounded intro input
main = 7ba68955a9b0979924c5e759736fb483589be744
```

The subsequent `agent/esp32-docs-architecture` branch is documentation-only and
does not change the firmware/hardware contract below.

### Current safe boundary

The real CYD reaches and remains safely parked after the final intro Continue:

```text
menu                    = MENU_NONE
state                   = ST_INTRO (9)
storyPage               = 2
storyTextPage           = 0
intro clock             = inactive
intro input             = inactive
intro images/texts      = resident
heap8                   = 50656
largest8                = 13300
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
DoomCanvas_disposeIntro = NOT called
DoomCanvas_loadMap      = NOT called
```

Post-PARK touch diagnostics still print, proving the Arduino loop remains alive
instead of resetting or silently entering map loading.

## Current execution path

Validated normal-firmware path:

```text
video / SD / ZIP
    -> Doom RPG core/layout/pre-render startup
    -> Render_startup
    -> config + mappings
    -> menu.bsp structural runtime load
    -> stop before legacy bitshape/texel inflation
    -> direct opaque MENU_MAIN
    -> DoomCanvas.state synchronized to ST_MENU
    -> semantic XPT2046 menu input
    -> real MenuSystem_select(Start Game)
    -> fresh-start dead-resource cleanup
    -> original Player_reset()
    -> ST_INTRO + real prologue strings/images
    -> fitted deterministic t=0 story frame
    -> ESP32-owned 50 ms intro clock
    -> bounded More / Continue touch progression
    -> final PARK at ST_INTRO page 2
```

The next gameplay transition is still intentionally behind a hard boundary.

## Permanent architecture invariants

Target:

- ESP32-2432S028R / classic CYD
- ESP32-D0WD-V3, dual core, 240 MHz
- 4 MB flash
- no PSRAM
- ILI9341 320x240 landscape
- XPT2046 touch
- microSD backing store
- logical framebuffer 160x120 RGB565 = 38,400 B
- exact nearest-neighbor 2x panel presentation
- audio still deferred

Permanent graphics/resource direction:

```text
shapeData   == NULL
mediaTexels == NULL
```

The port must not resurrect the original monolithic `shapeData` or map-wide
`mediaTexels` architecture. SD/native pack resources are loaded as bounded frames
through measured working sets/caches.

## Documentation / development discipline

1. Branch from the exact latest merged `main`.
2. One branch = one bounded measurable objective.
3. Build/flash/test on the real classic CYD.
4. Fix failures on that same branch.
5. Preserve detailed evidence in a milestone archive when it has long-term value.
6. Update this recovery point before merge.
7. Merge only when implementation, real hardware and documentation agree.
8. After merge, mark the milestone document as a historical merged archive.

## Merged milestone timeline

| PR | Milestone | Merge/reference SHA |
| ---: | --- | --- |
| #32 | fast opaque Options -> Main return without replaying MENUWALL/MENUSPRITE | `cc2cb40cf026b5a5e232dba67f884905aca42488` |
| #33 | normal/bring-up boot split + loading-bar flicker suppression | `1035d4413686624feb07aaf208821946cead5869` |
| #34 | permanent bring-up touch-hitbox overlay | `2b29ca7f3479c9add022ccc803bdbff7dd5ade34` |
| #35 | hardware-selected CYD color profile | `a87b50747fa69bba6624870f944cbb1111014276` |
| #36 | fresh Start Game entry + lifecycle cleanup | `5275e4a1c6eca703b51221e80f3b199178015a01` |
| #37 | first fitted deterministic `ST_INTRO` frame | `b934e21c7f2dbf6463a4d2dfa13d1e06614e2b96` |
| #38 | bounded ESP32-owned 50 ms intro clock | `58edfe5d7080a7e9e64ff5b516697ddf3cca31da` |
| #39 | full bounded intro touch progression | `7ba68955a9b0979924c5e759736fb483589be744` |

Earlier validated work leading to PR #32 includes native asset pack v2, zero
resident monolithic graphics pools, bounded wall/sprite frames and caches, real
menu BSP traversal, native projected wall/sprite rasterization, the fitted
160x120 main menu and semantic touch selection.

Detailed intro milestone archives:

- [`FIRST_INTRO_FRAME.md`](FIRST_INTRO_FRAME.md) — PR #37
- [`INTRO_CLOCK.md`](INTRO_CLOCK.md) — PR #38
- [`INTRO_INPUT.md`](INTRO_INPUT.md) — PR #39

## Native graphics recovery references

These values are primarily regression/bring-up references and should remain
available even though the current normal path no longer replays every proof pass.

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

Validated LRU evidence:

```text
Wall LRU3
  logical requests = 25
  hits             = 14
  physical misses  = 11
  evictions        = 8
  peak payload     = 6144 B

Sprite LRU3
  logical requests = 11
  hits             = 2
  physical misses  = 9
  evictions        = 6
  peak payload     = 6038 B
```

## Menu runtime and presentation recovery

Normal menu-map structural load stops before the legacy monolithic graphics
phase. Hardware runtime while `MENU_MAIN` is active:

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
shapeData      = NULL
mediaTexels    = NULL
```

Main menu model:

```text
Start Game
Options
Help/About
Exit
```

Fitted logical geometry:

```text
screen      = 160x120
logo target = 90x62 at 35,2
Start Game  y=67
Options     y=79
Help/About  y=91
Exit        y=103
```

Menu regression references:

```text
MENU_MAIN model FNV            = bbc2149b
layout FNV                     = 47b3656e
black + scaled logo            = 0ac1f9c6
Start Game selected            = 58a11171
Options selected               = 0cf107b1
Help/About selected            = 9db82b71
Exit selected                  = bdd775f9
MENU_MAIN_OPTIONS model        = e1ef01f7
MENU_MAIN_OPTIONS framebuffer  = 6058d47d
```

Start's real-CYD touch needed a narrow top tolerance after a measured tap landed at
logical `y=64` while the visible label starts at `y=67`:

```text
Start Game logical x=28..119 y=64..78
Options    logical x=28..119 y=79..90
Help/About logical x=28..119 y=91..102
Exit       logical x=28..119 y=103..114
```

Options Back remains:

```text
logical  x=15..119 y=65..78
physical x=30..239 y=130..157
```

Native menu activation must explicitly establish:

```text
MENU_MAIN visible + touch armed
    => DoomCanvas.state == ST_MENU (2)
```

because the optimized menu painter bypasses the original expensive
`MenuSystem_setMenu()` side effects.

## Touch platform contract

Current platform semantics:

```text
press edge -> one semantic callback
hold       -> no repeat
release    -> 50 ms stable-release debounce
next press -> next callback
```

Menu code applies its own select/confirm policy above this platform behavior.
Intro input reuses the same callback; no second XPT2046 poller exists.

Bring-up overlay meaning:

```text
red rectangle     = accepted logical hitbox scaled 2x
cyan cross        = last calibrated semantic touch
small yellow ring = exact touch center
Serial            = raw / physical / logical coordinates
```

The overlay is drawn after framebuffer presentation and never changes logical
framebuffer hashes.

## Fresh Start Game contract

Strict preconditions at confirmed Start:

```text
menu        = MENU_MAIN
selected    = 0
state       = ST_MENU
frame FNV   = 58a11171
shapeData   = NULL
mediaTexels = NULL
wall/sprite caches inactive
```

Fresh no-save action remains the real original route:

```text
MENU_MAIN / item 0
    -> MenuSystem_select()
    -> Menu_select(MENU_MAIN, 0)
    -> Menu_startGame(menu, 1)
    -> Player_reset()
    -> DoomCanvas_setState(ST_INTRO)
    -> DoomCanvas_loadPrologueText()
```

The existing-save precheck occurs before destructive cleanup. Existing-save mode
keeps menu runtime because `MENU_MAIN_CONTINUE` still needs it.

### Fresh-start lifecycle cleanup

The optimized menu lifecycle otherwise leaves two classes of dead resources alive:

1. `imgLegals` / `g.bmp`, normally freed by the skipped legal-screen state path;
2. `menu.bsp` runtime/mappings, no longer needed after fresh New Game is confirmed.

Fresh Start therefore executes:

```text
DoomRPG_freeImage(imgLegals)
Render_freeRuntime(render)
Game_unloadMapData(game)
```

Original measured build:

```text
heap8     29064 -> 84480
largest8  17396 -> 36852
recovered         55416 B
```

Later PR #39 build:

```text
heap8     29008 -> 84424
largest8  17396 -> 36852
recovered         55416 B
```

The exact 55,416-byte recovery is stable despite small build-to-build baseline
movement.

### Fresh Player reset contract

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

## Intro resource plan

`DoomCanvas_loadPrologueText()` uses the original three text buffers plus four
packed indexed BMP textures:

```text
c.bmp -> imgSpaceBG
  ZIP c/u       = 3675 / 12408 B
  packed pixels = 12288 B
  192x128, 4-bpp, palette 16

d.bmp -> imgLinesLayer
  ZIP c/u       = 149 / 12356 B
  packed pixels = 12288 B
  192x128, 4-bpp, palette 3

e.bmp -> imgPlanetLayer
  ZIP c/u       = 1352 / 8312 B
  packed pixels = 8192 B
  128x128, 4-bpp, palette 16

f.bmp -> imgSpaceship
  ZIP c/u       = 114 / 160 B
  packed pixels = 45 B
  9x9, 4-bpp, palette 8
```

ZIP totals:

```text
compressed   = 5290 B
uncompressed = 33236 B
```

The legacy ZIP peak is per file. After the fresh-start cleanup all four assets
load successfully through the existing packed indexed BMP path, so a native `.pak`
migration was not required for these intro images.

## Intro rendering recovery

### Black entry boundary

After real fresh Start/prologue loading:

```text
menu            = MENU_NONE
state           = ST_INTRO (9)
framebuffer FNV = 485915c5
storyPage       = 0
storyTextPage   = 0
```

`485915c5` is the black 160x120 RGB565 entry framebuffer.

### Story fit

Original 128x128 story coordinates do not fit 120 logical pixels vertically. The
first direct renderer produced valid pre-fit hardware FNV `6cf52a3e` but cropped
four logical pixels at top and bottom.

Final fitted mapping:

```text
virtual story space = 128x128
ESP32 viewport      = 120x120
viewport origin     = x20,y0
logical framebuffer = 160x120
physical story      = centered 240x240 at exact 2x
```

No intermediate framebuffer is allocated.

Final deterministic first fitted frame:

```text
t=0 FNV       = 56438966
first-frame build heap8    = 50704 -> 50704
largest8                 = 13300 -> 13300
```

See [`FIRST_INTRO_FRAME.md`](FIRST_INTRO_FRAME.md) for the full pre-fit/fitted
hardware evidence.

## Intro clock recovery

Clock model:

```text
virtual step = 50 ms
nominal time = 0, 50, 100, 150, ...
max render   = one due frame per loop service
late ticks   = skipped, never catch-up rendered
```

Stable framebuffer checkpoints:

```text
t=0 ms     FNV=56438966
t=50 ms    FNV=da9cd50e
t=100 ms   FNV=c63cf367
t=200 ms   FNV=2620e850
t=1000 ms  FNV=e76fec13
```

PR #38 clock build RAM:

```text
heap8       = 50672 -> 50672 per rendered frame
largest8    = 13300 -> 13300 per rendered frame
deltaHeap   = 0
deltaLargest= 0
```

At virtual `t=1000 ms`:

```text
ticks elapsed    = 20
frames rendered  = 14
ticks skipped    = 6
effective render ~= 14 FPS
```

See [`INTRO_CLOCK.md`](INTRO_CLOCK.md) for detailed timing evidence.

## Intro input recovery

Fitted input geometry:

```text
story viewport = x20..139 y0..119
prompt band    = x20..139 y102..119
```

Across two real-CYD captures, **every bounded intro input branch is validated**:

```text
out-of-band prompt touch          -> MISS PASS
page 0 / text 0 early reveal      -> PASS
page 0 / text 0 -> More           -> PASS
page 0 / text 1 early reveal      -> PASS
page 0 -> page 1 Continue         -> PASS
page 1 natural timeout -> page 2  -> PASS
page 1 touch skip -> page 2       -> PASS
page 2 early reveal               -> PASS
final Continue -> safe PARK       -> PASS
```

Natural page-1 transition:

```text
page-1 epoch = t=9150
AUTO-PAGE    = t=19200
elapsed      = 10.05 s
```

Touch-skip validation path:

```text
REVEAL page0/text0  t=2050
MORE                t=3300
REVEAL page0/text1  t=4600
CONTINUE page0->1   t=5400
SKIP-ANIM page1->2  t=7300
REVEAL page2        t=8400
FINAL-CONTINUE      t=9150
PARK tick=183 frames=115 skipped=68
```

The current PR #39 build remains:

```text
heap8    = 50656
largest8 = 13300
```

through all measured frame and input transitions.

See [`INTRO_INPUT.md`](INTRO_INPUT.md) for both complete hardware paths.

## Current memory baselines

Build-to-build baseline movement is expected as small state/code objects are
added. The critical contract is that bounded render/input operations do not
allocate unexpectedly.

```text
interactive normal-menu, earlier build
  heap8    = 29064
  largest8 = 17396

interactive normal-menu, PR #39 build
  heap8    = 29008
  largest8 = 17396

bring-up menu with hitbox diagnostics
  heap8    = 28592
  largest8 = 17396

fresh cleanup, earlier build
  heap8    = 84480
  largest8 = 36852

fresh cleanup, PR #39 build
  heap8    = 84424
  largest8 = 36852

fresh Start/prologue PR #36 boundary
  heap8    = 50712
  largest8 = 13300

fitted first-frame build
  heap8    = 50704
  largest8 = 13300

intro-clock build
  heap8    = 50672
  largest8 = 13300

current intro-input build/final PARK
  heap8    = 50656
  largest8 = 13300
```

Do not attribute small baseline differences to a particular allocation unless a
measurement isolates the cause.

## Display / pacing recovery

Hardware-selected profile:

```text
gamma       = 1.00
saturation  = 1.15
resampling  = nearest
```

Representative panel timing:

```text
normal full-screen Present          ~= 42.7 ms
first fitted ST_INTRO Present       = 42.761 ms
active intro Present                ~= 42.67..42.84 ms
bring-up Present + physical overlay ~= 44.3 ms
old neutral Present                 ~= 34.4 ms
```

The current 50 ms virtual intro clock is a functional/RAM PASS but does not
physically sustain 20 rendered FPS. It skips stale virtual ticks by design and
measures roughly 14 rendered FPS. Compare against the original J2ME version before
changing pacing; the presentation/saturation path is the measured first
optimization candidate.

## Current deferred work

Still intentionally deferred:

- intro disposal and transition into loading
- first gameplay/map load
- bounded gameplay resource working set for map 1
- existing-save Continue / New Game submenu painter/action
- Video/Input/Sound menu actions
- Help/About and Exit real actions
- active normal gameplay loop
- gameplay controls
- presentation optimization if J2ME comparison warrants it
- audio

## Next bounded milestone

Start from the final PR #39 PARK:

```text
ST_INTRO page 2
heap8=50656 largest8=13300
intro assets/texts resident
clock/input inactive
shapeData/mediaTexels NULL
```

Next objective:

```text
measure resident intro resources
    -> dispose intro assets/text deliberately
    -> measure reclaimed heap/largest block
    -> verify all intro pointers are cleared safely
    -> stop before, or at a fresh explicit guard around, the first gameplay-map load
```

The first gameplay map must **not** be allowed to resurrect monolithic
`shapeData` or map-wide `mediaTexels`.
