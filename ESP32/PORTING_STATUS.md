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
- real XPT2046 menu touch + released double-tap confirmation
- Options / Back real menu actions
- fast opaque menu return without replaying menu scene
- fast-menu branch merged as PR #32 at
  `cc2cb40cf026b5a5e232dba67f884905aca42488`
- normal/bring-up boot split merged as PR #33 at
  `1035d4413686624feb07aaf208821946cead5869`
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

Objective: add bounded touch progression through the complete intro while keeping
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
intro assets            = resident until final PARK
DoomCanvas_run          = NOT called
DoomCanvas_disposeIntro = NOT called
DoomCanvas_loadMap      = NOT called
```

The 16-byte baseline change from the merged clock build (`50672 -> 50656`) is a
build-to-build difference from the input state/callback code. Every measured
frame and every measured input transition retained zero heap/largest-block delta.

## Current intro touch semantics

`PlatformInput` emits one semantic callback on the physical press edge, then
requires a stable 50 ms release before another callback can be emitted:

```text
press -> one semantic tap
hold  -> no repeat
release stable 50 ms -> rearm
```

Earlier wording called this a `released-tap`; the implementation was correct but
the wording was not. The current branch corrects the runtime marker and comments.

Text-page hit geometry:

```text
story viewport logical = x20..139 y0..119
prompt band logical    = x20..139 y102..119
```

Page 1 has no prompt, so its optional animation-skip touch accepts the whole story
viewport.

The real CYD run proved rejection outside the prompt band:

```text
physical=115,59 -> logical=57,29
page=0 textPage=0 textDone=0 accepted=0
[INTROIN] MISS ... promptBandY=102..119
```

## Hardware-validated intro input sequence

Initial deterministic hashes remain unchanged from PR #38:

```text
t=0 ms     56438966
t=50 ms    da9cd50e
t=100 ms   c63cf367
t=200 ms   2620e850
t=1000 ms  e76fec13
```

Hardware path:

```text
page 0 / text 0 naturally completes at t=3900

[INTROIN] MORE textPage=0->1 t=5600 textEpoch=5600
[INTROIN] READY page=0 textPage=1 ... heap8=50656 largest8=13300

[INTROIN] REVEAL page=0 textPage=1 t=7400
[INTROIN] READY page=0 textPage=1 textDone=1 heap8=50656 largest8=13300

[INTROIN] CONTINUE storyPage=0->1 t=9150 epoch=9150
[INTROIN] READY page=1 textPage=0 ... heap8=50656 largest8=13300

[INTROCLK] AUTO-PAGE 1->2 t=19200 textPage=0 epoch=19200

page 2 / text 0 naturally completes at t=22650

[INTROIN] FINAL-CONTINUE page=2 textPage=0 t=23650
[INTROCLK] PARK reason=intro-exit-ready tick=473 frames=282 skipped=191
           state=9 page=2 textPage=0 heap8=50656 largest8=13300
[INTROIN] READY-TO-EXIT state=9 page=2 textPage=0 heap8=50656 largest8=13300
          assets=retained noDispose=yes noMapLoad=yes
[TOUCH] ...
```

The page-1 natural animation therefore lasted 10.05 virtual seconds from epoch
`t=9150` to transition `t=19200`; the extra 50 ms is the expected clock
quantization.

The `[TOUCH]` line after final `READY-TO-EXIT` shows the Arduino loop continued
after PARK. Stable `[ALIVE]` markers were also observed throughout the long run.

No unexpected `[INTROIN] FAILED` or `[INTROCLK] PARK` occurred before the intended
final `intro-exit-ready` park.

### Alternate-path coverage note

The pasted hardware capture did not exercise:

- reveal page-0 text 0 before natural completion
- page-1 touch skip (`SKIP-ANIM`)
- reveal page-2 text before natural completion

The reveal mutation is hardware-validated on page-0 text 1 and shared by the
other text pages. The optional page-1 skip remains implemented and is recorded as
an alternate-path regression check; the complete natural end-to-end path and the
final safe boundary are hardware validated.

## Fresh Start lifecycle contract inherited from PR #36

Fresh no-save path:

```text
MENU_MAIN / Start Game
    -> MenuSystem_select()
    -> Menu_startGame(new)
    -> Player_reset()
    -> ST_INTRO
    -> DoomCanvas_loadPrologueText()
```

Before the irreversible fresh transition, dead menu/legal resources are released:

```text
DoomRPG_freeImage(imgLegals)
Render_freeRuntime(render)
Game_unloadMapData(game)
```

Measured recovery:

```text
heap8    29064 -> 84480
largest8 17396 -> 36852
gained   55416 B
```

Required post-cleanup state:

```text
nodes       = NULL
lines       = NULL
mapSprites  = NULL
mappings    = NULL / NULL
shapeData   = NULL
mediaTexels = NULL
```

The original prologue loader then successfully loads:

```text
c.bmp 192x128 4-bpp packed=12288 B palette=16
d.bmp 192x128 4-bpp packed=12288 B palette=3
e.bmp 128x128 4-bpp packed=8192 B  palette=16
f.bmp 9x9     4-bpp packed=45 B    palette=8
```

Fresh `Player_reset()` remains:

```text
level=1 xp=0 nextXP=80 credits=0 keys=0 ammo[1]=8
weapon=2 weapons=0x00000004 disabledWeapons=0 deaths=0
```

## Fitted first-frame contract inherited from PR #37

Virtual story geometry:

```text
original virtual story = 128x128
ESP32 viewport         = 120x120 at x20,y0
logical framebuffer    = 160x120
physical TFT           = exact 2x -> 320x240
```

Final first-frame regression:

```text
entry FNV        = 485915c5
fitted frame FNV = 56438966
heap8            = 50704 -> 50704
largest8         = 13300 -> 13300
```

Pre-fit historical frame `6cf52a3e` is retained only as the cropped baseline.

## Intro clock contract inherited from PR #38

Clock model:

```text
virtual step      = 50 ms
nominal target    ~= 20 FPS
one render max per loop service
stale ticks       = skipped, never catch-up rendered
```

At virtual `t=1000 ms` on hardware:

```text
ticks elapsed    = 20
frames rendered  = 14
ticks skipped    = 6
effective render ~= 14 FPS
Present          ~= 42.77..42.84 ms
```

The clock is functionally correct; current full-screen TFT presentation cost is
the measured pacing bottleneck. Compare against the original J2ME version before
optimizing. `PlatformVideo_present()` and the saturation 1.15 output transform are
the first measured candidates if optimization is later required.

## Hardware-selected display profile

```text
gamma       = 1.00
saturation  = 1.15
resampling  = nearest
```

The transform is applied only after the logical framebuffer/FNV boundary.

## Two PlatformIO modes

Normal firmware:

```bash
cd ESP32
pio run -e esp32-cyd -t upload
pio device monitor -e esp32-cyd
```

Bring-up laboratory:

```bash
cd ESP32
pio run -e esp32-cyd-bringup -t upload
pio device monitor -e esp32-cyd-bringup
```

Bring-up adds historical probes and the physical touch-hitbox overlay while normal
mode keeps those paths compiled out.

## Stable menu / renderer recovery references

```text
native scene framebuffer       = ffe0995e
MENU_MAIN model FNV            = bbc2149b
MENU_MAIN layout FNV           = 47b3656e
black + scaled logo            = 0ac1f9c6
Start Game selected            = 58a11171
Options selected               = 0cf107b1
Help/About selected            = 9db82b71
Exit selected                  = bdd775f9
MENU_MAIN_OPTIONS model        = e1ef01f7
MENU_MAIN_OPTIONS framebuffer  = 6058d47d
sprite 172 texel               = 0c0a7acd
wall 112 texel                 = 92d40704
synthetic projected wall       = ad191f54
real walls framebuffer         = a6d87c4a
viewSprites list               = 962cd657
sprite request                 = 4457ac94
```

## Current safe boundary

Hardware validated:

- core/layout/pre-render/Render/config/mappings startup
- bounded wall/sprite render and LRU contracts
- opaque deterministic `MENU_MAIN`
- calibrated XPT2046 menu touch and stable-release rearm
- Options / Back actions
- fresh direct Start through real `MenuSystem_select()`
- fresh-start lifecycle cleanup recovering 55,416 B
- original prologue text + four intro BMP assets resident
- fitted first `ST_INTRO` FNV `56438966`
- native 128x128 -> centered 120x120 story fit
- ESP32-owned 50 ms multi-frame clock
- bounded skipped-tick behavior under ~42.8 ms TFT presentation cost
- semantic intro touch input
- prompt-band rejection / acceptance
- `More` page-0 text progression
- reveal-on-tap
- `Continue` page 0 -> page 1
- natural 10-second page 1 -> page 2 transition with virtual epoch rebase
- page-2 rendering/completion
- final Continue parks at `ST_INTRO`, page 2
- intro assets retained at final PARK
- heap8 `50656` stable throughout current run
- largest8 `13300` stable throughout current run
- no map/runtime resurrection
- no `DoomCanvas_run()` handoff
- no `DoomCanvas_disposeIntro()` at final Continue
- no `DoomCanvas_loadMap()` at final Continue
- `shapeData == NULL`
- `mediaTexels == NULL`

Still intentionally deferred:

- optional page-1 touch-skip hardware regression capture
- bounded intro disposal / transition to loading
- first gameplay/map load after the intro
- existing-save Continue / New Game submenu painter/action
- Video/Input/Sound actions
- Help/About and Exit real actions
- active normal gameplay loop
- gameplay controls
- possible presentation optimization after J2ME comparison
- audio

## Next milestone

After merge, start from the new `main` and implement one bounded intro-disposal /
loading handoff increment:

```text
final PARK at ST_INTRO page 2
    -> measure resident intro resources
    -> dispose intro resources deliberately
    -> measure reclaimed heap/largest block
    -> approach first gameplay map load behind a new explicit boundary
```

Do not permit the first gameplay load to reintroduce monolithic `shapeData` or
map-wide `mediaTexels`.
