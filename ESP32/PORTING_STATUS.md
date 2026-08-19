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

## Current increment

Branch: `agent/esp32-bringup-touch-hitboxes`

Base `main` SHA:

```text
1035d4413686624feb07aaf208821946cead5869
```

Status: **IMPLEMENTED; INITIAL BRING-UP PHOTOS VALIDATE THE OVERLAY; FINAL DUAL-PROFILE RETEST PENDING**.

Objective: keep a permanent visual touch-calibration tool in the diagnostic
firmware without contaminating the normal optimized firmware.

The tool shows:

```text
red rectangles = actual logical touch zones scaled exactly 2x on the TFT
cyan cross      = most recent calibrated semantic touch
small yellow ring = exact touch centre
Serial          = raw / physical / logical coordinates
```

The overlay is drawn directly on the physical ILI9341 after framebuffer present.
It never writes into the 160x120 game framebuffer, so deterministic framebuffer
FNV contracts remain unchanged.

## Two PlatformIO modes remain the architecture boundary

### `esp32-cyd` — normal firmware

Build / flash:

```bash
cd ESP32
pio run -e esp32-cyd -t upload
pio device monitor -e esp32-cyd
```

This remains the everyday optimized firmware.

Normal boot executes only the real state required to reach the interactive menu:

```text
Platform video / SD / ZIP
    -> real engine core and layout
    -> real ParticleSystem / MenuSystem / EntityDef startup
    -> real Render_startup
    -> real config + mappings
    -> Render_beginLoadMap(MAP_MENU)
    -> real Render_beginLoadMapData structural phase
    -> stop before legacy bitshapes/texels
    -> direct opaque MENU_MAIN
    -> touch gate armed
    -> READY
```

Historical proof/demo passes remain skipped in normal mode.

Most importantly for this increment, `DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY` is not
defined in `esp32-cyd`, and overlay code is isolated at preprocessing time:

```text
normal esp32-cyd
    -> no debug-overlay state arrays
    -> no debug-overlay draw function
    -> no debug-overlay C bridge functions
    -> no PlatformInput overlay call
    -> no menu overlay registration calls
    -> no red rectangles / touch marker
```

This is deliberate. The normal path does not rely on no-op functions or on the
optimizer removing diagnostic calls.

### `esp32-cyd-bringup` — diagnostic laboratory

Build / flash:

```bash
cd ESP32
pio run -e esp32-cyd-bringup -t upload
pio device monitor -e esp32-cyd-bringup
```

This environment extends `esp32-cyd` with:

```text
-D DOOMRPG_ESP32_BRINGUP_PROBES=1
-D DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY=1
```

It therefore contains both:

1. the complete historical validation/probe suite;
2. the physical touch-hitbox overlay.

Use it for resource regressions, native renderer/cache validation and visual
input calibration. It is intentionally verbose and is not the product boot.

## Bring-up hitbox implementation

The overlay lives outside the framebuffer.

`PlatformVideo_present()` still presents the exact 160x120 RGB565 framebuffer to
the 320x240 TFT. Only in the bring-up build does it then draw the diagnostic
rectangles/cross directly with TFT_eSPI.

Therefore:

```text
framebuffer hashes = game pixels only
physical TFT       = game pixels + optional bring-up overlay
```

The latest semantic calibrated touch is captured in `PlatformInput` before the
main-menu double-tap gate can consume an ARM/CONFIRM event. This makes the marker
useful even when a first tap is deliberately swallowed by the interaction gate.

## MENU_MAIN hitboxes

The main-menu overlay consumes the same compile-time geometry constants as the
final `MENUTOUCH` gate.

Current zones:

```text
Start Game
logical  x=28..119 y=67..78
physical x=56..239 y=134..157

Options
logical  x=28..119 y=79..90
physical x=56..239 y=158..181

Help/About
logical  x=28..119 y=91..102
physical x=56..239 y=182..205

Exit
logical  x=28..119 y=103..114
physical x=56..239 y=206..229
```

Initial real-CYD photograph validation showed all four red rectangles aligned
cleanly with the visible four menu rows. No geometry adjustment was needed.

## MENU_MAIN_OPTIONS hitboxes

The visible Options rows are:

```text
Back  visual y=67..78
Video        y=79..90
Input        y=91..102
Sound        y=103..114
```

The first bring-up photograph showed the four zones correctly aligned overall.
The Back zone looked slightly taller because it still contained the earlier
three-logical-pixel upper tolerance (`64..78`).

That tolerance was introduced after a real tap landed at logical `y=65`, just
above the visible row, and missed.

The final proposed Back calibration is now tightened by one logical pixel while
preserving that exact observed `y=65` case:

```text
Back logical  x=15..119 y=65..78
Back physical x=30..239 y=130..157
visible row   y=67..78
top tolerance = 2 logical pixels
```

Other Options diagnostic rows remain:

```text
Video logical x=15..119 y=79..90
Input logical x=15..119 y=91..102
Sound logical x=15..119 y=103..114
```

Only Back is currently actionable. Video/Input/Sound are still deliberately
deferred, but their rectangles are useful for visual calibration before those
actions are implemented.

## Original engine debug facilities checked

The reverse-engineered original engine does contain hidden developer/debug menus:

```text
MENU_DEBUG
MENU_DEBUG_CHEATS
MENU_DEBUG_MAPS
MENU_DEBUG_STATS
MENU_DEVELOPER
```

It also contains benchmark/developer facilities.

No original touch-hitbox/bounds visualization was found in the menu/input code.
This is expected because the original menu interaction is directional and driven
through `selectedIndex` / `MenuSystem_moveDir()`, not through CYD touch zones.

The ESP32 hitbox overlay therefore remains an adaptation-specific diagnostic,
while the existing original debug/developer menus remain interesting candidates
for future ESP32 diagnostics rather than inventing parallel tools unnecessarily.

## Active opaque MENU_MAIN references

The real model remains:

```text
Start Game
Options
Help/About
Exit
```

Geometry:

```text
logical screen = 160x120
logo target    = 90x62 at 35,2
Start Game y=67
Options    y=79
Help/About y=91
Exit       y=103
layout FNV = 47b3656e
model FNV  = bbc2149b
```

Current hardware hashes:

```text
black + scaled logo          = 0ac1f9c6
Start Game selected           = 58a11171
Options selected              = 0cf107b1
Help/About selected           = 9db82b71
Exit selected                 = bdd775f9
MENU_MAIN_OPTIONS model       = e1ef01f7
MENU_MAIN_OPTIONS framebuffer = 6058d47d
```

Historical scene-backed main-menu hashes remain recovery references only:

```text
Start Game old = cbc99461
Options old    = 961109a7
Help old       = e4eadfbb
Exit old       = 5ff2a5cd
```

## Fast Options -> Back remains the normal navigation path

```text
MENU_MAIN
    -> MenuSystem_select()
    -> MENU_MAIN_OPTIONS
    -> MenuSystem_back()
    -> direct opaque MENU_MAIN repaint
    -> touch re-armed
```

No MENUWALL/MENUSPRITE replay occurs during Back.

Previously measured complete Back model transition + paint + TFT present:

```text
~138 ms
```

The overlay is diagnostic presentation only and must not alter this normal
navigation contract.

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

## Normal-mode memory baseline

Last hardware-validated clean normal firmware baseline before this diagnostic
increment:

```text
heap8    = 29064
largest8 = 17396
```

Strong invariant:

```text
shapeData   = NULL
mediaTexels = NULL
```

The final dual-profile test for this branch must confirm the normal baseline and
normal hashes remain unchanged after compile-time isolation of the overlay.

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

## Final validation required before merge

Test both environments from this branch.

### Normal

```bash
cd ESP32
pio run -e esp32-cyd -t clean
pio run -e esp32-cyd -t upload
pio device monitor -e esp32-cyd
```

Expected:

```text
no [HITBOX] logs
no red rectangles
MENU_MAIN finalFNV=58a11171
Options selected=0cf107b1
Options framebuffer=6058d47d
Back returns to 58a11171
shapeData=0x0
mediaTexels=0x0
```

### Bring-up

```bash
pio run -e esp32-cyd-bringup -t clean
pio run -e esp32-cyd-bringup -t upload
pio device monitor -e esp32-cyd-bringup
```

Expected:

```text
[HITBOX] Physical overlay enabled
4 red MAIN rectangles
4 red Options rectangles
cyan/yellow last-touch marker
framebuffer hashes unchanged
Back zone logical y=65..78
```

If both profiles pass, no additional documentation step is required for this
increment; it is merge-ready.

## Current safe boundary

Hardware validated before the final retest:

- real core/layout/pre-render startup
- real Render startup
- real config + mappings
- real menu runtime structures through the pre-bitshape boundary
- opaque deterministic `MENU_MAIN`
- real Options action through `MenuSystem_select()`
- real Back through `MenuSystem_back()`
- fast opaque Back repaint
- no historical loading-bar flicker in normal mode
- initial bring-up hitbox photographs align with MAIN and Options controls
- `shapeData == NULL`
- `mediaTexels == NULL`

Implemented on this branch, pending final dual-profile confirmation:

- permanent bring-up-only physical hitbox overlay
- last calibrated touch marker
- compile-time removal of overlay path from normal firmware
- Back upper tolerance tightened from logical y=64 to y=65

Still intentionally deferred:

- Video/Input/Sound actions
- Help/About and Exit real actions
- Start Game / gameplay loader activation
- active normal multi-frame game loop
- gameplay controls
- final color/contrast investigation
- audio
