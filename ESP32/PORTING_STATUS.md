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
- audio disabled during bring-up

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
5. Only after hardware PASS, update all relevant `.md` files on that branch.
6. Only then is the branch merge-ready.
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
- PR #31 heavy but deterministic Options -> Main round trip merged at
  `457e38fc6231d392a0c7d960d5b177011d923995`

## Current validated increment

Branch: `agent/esp32-fast-menu-back`

Base `main` SHA:

```text
457e38fc6231d392a0c7d960d5b177011d923995
```

Status: **HARDWARE VALIDATED, DOCUMENTED, READY TO MERGE**.

Objective: remove the ~2.5 second "mini reboot" feeling from Options -> Back
without allocating a second 38,400-byte framebuffer.

The new architecture deliberately separates graphics bring-up proof from menu
presentation:

```text
boot
  -> native MENUWALL / MENUSPRITE probes still validate scene ffe0995e
  -> MENU_MAIN presentation clears the shared framebuffer to black
  -> real j.bmp logo + p.bmp hand + Doom bitmap font

Options -> Back
  -> real MenuSystem_back()
  -> direct bounded opaque MENU_MAIN repaint
  -> touch re-armed
  -> NO MENUWALL replay
  -> NO MENUSPRITE replay
```

No persistent framebuffer copy, compressed snapshot or new large allocation is
needed.

## Opaque MENU_MAIN presentation

The real Doom RPG main model remains unchanged:

```text
Start Game
Options
Help/About
Exit
```

The ESP32 presentation is now opaque black rather than composited over the static
3D menu scene. This is closer to the observed J2ME presentation and, more
importantly, makes normal menu navigation independent from expensive BSP scene
reconstruction.

Geometry remains unchanged:

```text
logical screen = 160x120
logo source    = 108x74
logo target    = 90x62 at 35,2

Start Game y=67
Options    y=79
Help/About y=91
Exit       y=103

layout FNV = 47b3656e
model FNV  = bbc2149b
```

The hardware proves the existing native scene before it is hidden:

```text
[MAINTOUCHLAYOUT] Begin sceneFNV=ffe0995e expected=ffe0995e ... background=opaque-black
```

The black framebuffer plus scaled real logo is also deterministic and matches the
already-known Options composition:

```text
black + logo FNV = 0ac1f9c6
```

Hardware-validated initial opaque main frame:

```text
Start Game selected = 58a11171
```

The progressive paint hashes are diagnostic composition signatures, not selection
hashes:

```text
after item0 = 8fc6e681
after item1 = e4dc2287
after item2 = 711423ad
after item3 = 58a11171
```

Measured opaque composition on the tested CYD:

```text
composeMs = 58
heap8     = 28688
largest8  = 17396
```

## Active MENU_MAIN selected-frame hashes

Moving the real hand cursor on the opaque menu produced these deterministic
hardware signatures:

```text
Start Game = 58a11171
Options    = 0cf107b1
Help/About = 9db82b71
Exit       = bdd775f9
```

These supersede the old scene-backed selected-frame hashes for current operation.
Historical scene-backed references remain useful for recovery:

```text
Start Game old = cbc99461
Options old    = 961109a7
Help old       = e4eadfbb
Exit old       = 5ff2a5cd
```

Cursor movement still uses only four 13x10 RGB565 background patches:

```text
4 * 13 * 10 * 2 B = 1,040 B
```

No second full framebuffer is used.

## Real MENU_MAIN -> Options action

Confirmed Options still executes the real original menu transition:

```text
MENU_MAIN selected Options = 0cf107b1
    -> MenuSystem_select()
    -> MENU_MAIN_OPTIONS
```

The resulting real model remains:

```text
menu          = MENU_MAIN_OPTIONS / 7
type          = 7
oldMenu       = MENU_MAIN / 1
selectedIndex = 0
scrollIndex   = 0
numItems      = 4

Back
Video
Input
Sound
```

Hardware signatures remain unchanged because Options was already painted on a
controlled black background:

```text
modelFNV                     = e1ef01f7
black + logo                 = 0ac1f9c6
after Back                   = c7258261
after Video                  = 4e764e2f
after Input                  = 175fa691
after Sound / final Options  = 6058d47d
```

`0cf107b1` is now a strict precondition for the Options action rather than a
learn-on-hardware value.

## Fast real Options -> Back transition

Back still uses the real hierarchy operation:

```text
MenuSystem_back()
```

The original model returns correctly to:

```text
menu          = MENU_MAIN / 1
type          = 4
oldMenu       = -1
selectedIndex = 0
numItems      = 4
state         = ST_MENU / 2
```

The presentation then calls the same opaque MENU_MAIN painter used at boot.
Hardware result:

```text
=== Doom RPG ESP32 fast Options -> MENU_MAIN Back ===
...
[MAINOPAQUE] ... finalFNV=58a11171 composeMs=58 heap8=28688 largest8=17396
...
[OPTIONBACK] FAST End framebufferFNV=58a11171 runtimeFNV=58a11171 menu=1 selected=0 touchActive=1 repaintMs=138 shapeData=0x0 mediaTexels=0x0
[OPTIONBACK] READY real MenuSystem_back + opaque bounded repaint; no MENUWALL/MENUSPRITE replay
[OPTIONBACK] READY MENU_MAIN touch re-armed for another complete cycle
```

Measured return time for the complete model transition + bounded paint + TFT
present was about **138 ms** on this hardware, down from roughly 2.5 seconds.

The user also validated that a second cycle can immediately select Options again,
producing the same `0cf107b1` selected framebuffer. This proves normal navigation
is re-entrant without replaying the 3D scene.

## Back touch hitbox tolerance

The initial Back hitbox was visually aligned to the row only:

```text
logical y=67..78
```

Hardware touch jitter exposed a real miss:

```text
first tap  logical y=67 -> ARM
second tap logical y=65 -> MISS
```

The drawn row did not move. Only the touch tolerance was expanded upward:

```text
Back logical hitbox  x=15..119 y=64..78
Back physical hitbox x=30..239 y=128..157
```

This captures the observed XPT2046 variation while staying below the `Video` row,
which begins at logical y=79.

The current UX remains:

```text
first Back tap  -> ARM
release
second Back tap -> confirm Back
```

## Grayscale re-entry bridge retired

PR #31 needed a one-shot wrapper around `Render_setGrayPalettes()` because the
heavy Back path rerendered the scene and a second grayscale conversion changed
`a6d87c4a` into the failed `b6f86faa` wall frame.

The fast opaque Back path no longer reruns MENUWALL at all. Therefore:

- `native_menu_wall_reentry_bridge.c` has been removed
- `--wrap=DoomRPG_probeNativeMenuWallFrame` has been removed
- `--wrap=Render_setGrayPalettes` has been removed

The old failure remains documented as an architectural lesson, but the runtime
workaround is no longer part of normal code.

## Current memory boundary

Hardware baseline for this increment:

```text
heap8    = 28688
largest8 = 17396
```

Across main-menu selection, Options transition and fast Back:

```text
shapeData   = NULL
mediaTexels = NULL
wall cache  = inactive during menu navigation
sprite cache= inactive during menu navigation
```

No per-navigation memory leak was observed.

## Deterministic regression boundaries

Current active UI:

```text
MENU_MAIN model                    = bbc2149b
MENU_MAIN layout                   = 47b3656e
black + scaled logo                = 0ac1f9c6
opaque Start Game selected         = 58a11171
opaque Options selected            = 0cf107b1
opaque Help/About selected         = 9db82b71
opaque Exit selected               = bdd775f9
MENU_MAIN_OPTIONS model            = e1ef01f7
MENU_MAIN_OPTIONS framebuffer      = 6058d47d
```

Native graphics recovery references:

```text
sprite 172 texel FNV               = 0c0a7acd
wall 112 texel FNV                 = 92d40704
synthetic projected wall FNV       = ad191f54
real walls framebuffer             = a6d87c4a
viewSprites list FNV               = 962cd657
sprite request FNV                 = 4457ac94
walls + sprites framebuffer        = ffe0995e
faithful original MENU_MAIN        = 86c38260
historical fitted MENU_MAIN        = 1afa0223
failed double-gray wall frame      = b6f86faa
```

## Display and logging policy

TFT ownership remains:

```text
Serial -> startup/probe/debug/touch diagnostics
TFT    -> shared game framebuffer only
```

`DOOMRPG_ESP32_SCREEN_DIAGNOSTICS=0` remains the default.

The historical graphics/resource probes currently still execute during normal
startup. They were essential to establish the recovery hashes above, but they now
produce a large amount of serial output and add bring-up work that is no longer
useful on every normal boot.

Recommended next increment: introduce an explicit bring-up/probe flag so that:

```text
normal firmware
    -> minimal required startup
    -> concise logs
    -> no historical demonstration/proof probes

diagnostic bring-up mode
    -> current full probe chain
    -> all recovery hashes and memory contracts available on demand
```

Do not delete the proven probe code until normal startup dependencies are clearly
separated from validation-only probes.

## Touch-zone calibration follow-up

A future disposable diagnostic branch may draw the logical hitboxes as visible red
outlines over the actual menu and mark the last touch point. Hardware photos can
then calibrate all menu hitboxes together rather than adjusting them one at a time.
The overlay must be removed after measurements are captured; only final constants
belong in normal firmware.

## Current safe boundary

Hardware validated and executed:

- complete engine startup through mappings
- real menu BSP structural data
- native asset-pack access
- zero resident `shapeData`
- zero map-wide `mediaTexels`
- bounded GFXRM wall/sprite frames and LRU caches
- deterministic native menu scene `ffe0995e`
- real `MENU_MAIN` model/assets
- opaque black bounded main-menu presentation
- deterministic four-state main-menu cursor hashes
- calibrated physical touch hit-testing and 50 ms release rearm
- real `MenuSystem.selectedIndex` changes from touch
- real `MenuSystem_select()` for Options
- real `MENU_MAIN_OPTIONS` model/framebuffer
- real `MenuSystem_back()` from Options
- **fast direct Back repaint without MENUWALL/MENUSPRITE replay**
- **MENU_MAIN touch re-armed after Back**
- no second framebuffer and exact allocator recovery

Still intentionally deferred:

- Video/Input/Sound actions
- Help/About and Exit real actions
- Start Game / gameplay loader activation
- active normal multi-frame engine/game loop
- gameplay controls
- removing validation-only probes from normal boot
- full touch-zone visual calibration overlay
- final color/contrast investigation
- audio

## Recommended next increment after merge

Create a normal-vs-bring-up startup boundary so historical validation probes stop
running on every normal boot while remaining available under an explicit debug
flag. This should reduce startup latency and serial noise without discarding the
hardware evidence that got the port to this point.
