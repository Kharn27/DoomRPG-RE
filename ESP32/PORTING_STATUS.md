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

## Current increment

Branch: `agent/esp32-color-tuning-probe`

Base `main` SHA:

```text
2b29ca7f3479c9add022ccc803bdbff7dd5ade34
```

Status: **HARDWARE PASS IN NORMAL + BRING-UP; DOCUMENTED; MERGE-READY**.

Objective: resolve the visually faded CYD output without changing engine assets,
palettes, framebuffer hashes or renderer/resource architecture.

## Hardware comparison and selected profile

A temporary four-way physical TFT comparison displayed four exact 160x120 1:1
copies of the same logical framebuffer:

```text
A  gamma 1.00 / saturation 1.00
B  gamma 1.30 / saturation 1.00
C  gamma 1.00 / saturation 1.15
D  gamma 1.30 / saturation 1.15
```

The real-CYD visual choice was **C**.

The temporary comparison environment was removed before final validation.
There are still only the two permanent PlatformIO environments:

```text
esp32-cyd
esp32-cyd-bringup
```

Final hardware-selected display profile:

```text
gamma       = 1.00
saturation  = 1.15
resampling  = nearest
```

Nearest-neighbour sampling was already the ESP32 behavior, so the only new
runtime transform is saturation.

## Color-profile architecture

The logical framebuffer remains the source of truth:

```text
engine / palettes / GFXRM
        -> 160x120 RGB565 framebuffer
        -> deterministic engine/menu/render FNV hashes
        -> CYD presentation transform (saturation 1.15)
        -> exact nearest-neighbour 2x
        -> ILI9341
```

The saturation transform is deliberately outside engine state.

Implementation:

- expand RGB565 to 8-bit channels by bit replication;
- compute BT.601-style integer luma;
- scale only chroma to 115% around that luma;
- preserve neutral grays and black/white;
- repack to RGB565;
- transform once per logical pixel, then duplicate the result for exact 2x output.

Cost model:

```text
logical pixels transformed / frame = 160 * 120 = 19,200
second framebuffer                  = no
large LUT                           = no
persistent allocation              = no
```

Strong invariant:

```text
framebuffer contents = unchanged by panel tuning
```

Therefore all pre-existing framebuffer hashes remain valid.

## Normal-mode hardware evidence

Validated with `esp32-cyd`:

```text
heap8    = 29064
largest8 = 17396
shapeData   = 0x0
mediaTexels = 0x0
```

Deterministic hashes remained exact:

```text
MENU_MAIN Start Game = 58a11171
Options selected      = 0cf107b1
Options framebuffer   = 6058d47d
Back -> MENU_MAIN     = 58a11171
```

Representative presentation timing:

```text
[VIDEO] Present ... + sat1.15: 42883 us
[VIDEO] Present ... + sat1.15: 42717 us
[VIDEO] Present ... + sat1.15: 42704 us
[VIDEO] Present ... + sat1.15: 42734 us
[VIDEO] Present ... + sat1.15: 42739 us
```

Normal display present is therefore about **42.7 ms**, versus about 34.4 ms before
saturation tuning.

Fast Options -> Back remained correct:

```text
[OPTIONBACK] FAST End framebufferFNV=58a11171 expected=58a11171
runtimeFNV=58a11171 menu=1 selected=0 touchActive=1 repaintMs=147
shapeData=0x0 mediaTexels=0x0
```

## Bring-up hardware evidence

Validated with `esp32-cyd-bringup`.

Bring-up memory baseline remained:

```text
heap8    = 28592
largest8 = 17396
```

Native renderer/cache regression contracts remained exact while the physical TFT
used the tuned output profile:

```text
real walls framebuffer       = a6d87c4a
walls + sprites framebuffer  = ffe0995e
viewSprites list FNV         = 962cd657
sprite request FNV           = 4457ac94
shapeData                    = 0x0
mediaTexels                  = 0x0
```

Bring-up hitbox diagnostics also remained operational:

```text
[HITBOX] MAIN overlay registered from final tap gate zones=4 framebuffer=untouched
[HITBOX] OPTIONS overlay registered from Back/row hit constants zones=4 ...
```

Representative presentation timing with physical overlay:

```text
[VIDEO] Present ... + sat1.15: 44394 us
[VIDEO] Present ... + sat1.15: 44323 us
[VIDEO] Present ... + sat1.15: 44317 us
[VIDEO] Present ... + sat1.15: 44310 us
```

Bring-up menu presentation with overlay is therefore about **44.3 ms**.

Fast Back with bring-up diagnostics remained correct:

```text
[OPTIONBACK] FAST End framebufferFNV=58a11171 expected=58a11171
runtimeFNV=58a11171 menu=1 selected=0 touchActive=1 repaintMs=157
shapeData=0x0 mediaTexels=0x0
```

## Performance consequence

The selected physical color transform adds roughly 8 ms to a normal full-screen
present compared with the previous neutral path.

Current measured values:

```text
old neutral Present          ~= 34.4 ms
sat1.15 normal Present       ~= 42.7 ms
sat1.15 bring-up + overlay   ~= 44.3 ms
```

This is acceptable for the current menu milestone. It is not forgotten: when the
active gameplay loop is enabled, frame pacing should be measured before deciding
whether the same transform needs a cheaper lookup/table implementation or another
optimization.

Do not move the correction into game palettes merely to recover this timing; the
current architecture intentionally keeps rendering truth separate from panel
calibration.

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

Current touch zones:

```text
Start Game logical x=28..119 y=67..78
Options    logical x=28..119 y=79..90
Help/About logical x=28..119 y=91..102
Exit       logical x=28..119 y=103..114

Back logical  x=15..119 y=65..78
Back physical x=30..239 y=130..157
```

## Normal loader boundary remains real

Normal boot still executes:

```text
Render_beginLoadMap(MAP_MENU)
Render_beginLoadMapData()
```

The seventh original loading-bar callback remains the validated stop point after
runtime structures are resident but before legacy monolithic graphics loading.

Hardware runtime contract:

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

Normal mode suppresses the old five-box loading-bar TFT flicker while bring-up
retains the historical behaviour.

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
- logical framebuffer hashes unchanged by color profile
- `shapeData == NULL`
- `mediaTexels == NULL`

Still intentionally deferred:

- Video/Input/Sound actions
- Help/About and Exit real actions
- Start Game / gameplay loader activation
- active normal multi-frame game loop
- gameplay controls
- possible optimization of saturation cost if gameplay frame pacing needs it
- audio
