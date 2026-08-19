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
- gameplay viewport: 160x80 at framebuffer y=20
- physical output: exact nearest-neighbour 2x to 320x240
- audio still disabled during bring-up

## Project direction

DoomRPG-RE is treated as an executable specification for behaviour, data formats
and useful rendering semantics, not as an architecture contract. The ESP32 port
is progressively becoming its own constrained engine:

- bounded deterministic RAM use
- SD as immutable backing store
- measured working sets and evidence-driven caches
- no monolithic `shapeData`
- no map-wide `mediaTexels`
- one shared 160x120 RGB565 framebuffer
- storage isolated behind GFXRM
- cache policy isolated from storage/rasterization
- original BSP, projection, game and menu behaviour preserved where useful
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
- faithful original `MENU_MAIN` overlay reference `86c38260`
- clean TFT game ownership, Serial diagnostics retained
- fitted 160x120 `MENU_MAIN` geometry, historical framebuffer `1afa0223`
- touch-ready hand-only `MENU_MAIN` with real XPT2046 selection
- hardware-validated selection hashes:
  - Start Game `cbc99461`
  - Options `961109a7`
  - Help/About `e4eadfbb`
  - Exit `5ff2a5cd`
- double-tap semantic confirmation validated on hardware
- touch-select merge on `main` / PR #29:
  `03b0341c8c5df50b1b82d0c668dabfa41d53697e`

## Current validated increment

Branch: `agent/esp32-main-menu-options-action`

Base `main` SHA:

```text
03b0341c8c5df50b1b82d0c668dabfa41d53697e
```

Status: **HARDWARE VALIDATED, DOCUMENTED, READY TO MERGE**.

Objective: promote exactly one already validated `MENU_MAIN` confirmation into a
real original menu action:

```text
double tap Options
    -> real MenuSystem_select()
    -> real MENU_MAIN_OPTIONS model
    -> bounded ESP32 presentation
```

`Start Game`, `Help/About` and `Exit` remain unchanged/deferred.

## Why Options is the first executed action

The original `MenuSystem_select()` delegates to `Menu_select()`, then calls
`MenuSystem_setMenu()` when the target menu differs from the current menu.

For `MENU_MAIN` selected item 1 (`Options`), the original path reaches
`MENU_MAIN_OPTIONS` without entering gameplay loading. The resulting model from
`Menu_initMenu()` is:

```text
menu          = MENU_MAIN_OPTIONS / 7
type          = MENUTYPE_MAIN2 / 7
oldMenu       = MENU_MAIN / 1
selectedIndex = 0
scrollIndex   = 0
numItems      = 4

0 Back
1 Video
2 Input
3 Sound
```

This makes Options a safe first proof that the ESP32 touch frontend can drive a
real Doom RPG menu transition.

## Bounded Options presentation

The real `MenuSystem_select()` transition is executed, but the original
`MenuSystem_paint()` is intentionally not used yet for pre-game menus because it
still calls legacy `Render_render()`.

Instead, the ESP32 transition paints the resulting real Options model through a
bounded presentation path:

```text
real MenuSystem_select()
        |
        v
real MENU_MAIN_OPTIONS model
        |
        v
black controlled framebuffer
+ real scaled j.bmp logo
+ real p.bmp hand
+ real Doom bitmap font
        |
        v
shared 160x120 RGB565 framebuffer
```

No BSP rerender, map reload, gameplay loader, `shapeData` or map-wide
`mediaTexels` is introduced by this action.

The Options rows currently reuse the hardware-validated 160x120 row positions:

```text
Back  y=67
Video y=79
Input y=91
Sound y=103
```

The screen is deliberately **display-only after the transition** in this
increment. Touch is disarmed when Options opens. Therefore tapping `Back`,
`Video`, `Input` or `Sound` does nothing yet; that is expected success, not a
regression.

## Authoritative hardware validation

The real hardware produced:

```text
=== Doom RPG ESP32 real MENU_MAIN -> Options action ===
[MAINOPTIONS] Begin menu=1 selected=1 framebufferFNV=961109a7 expectedSelectedOptionsFNV=961109a7 heap8=28704 largest8=17396 shapeData=0x0 mediaTexels=0x0
[MAINOPTIONS] Model menu=7 type=7 old=1 selected=0 scroll=0 items=4 state=2 modelFNV=e1ef01f7
[MAINOPTIONS] ITEM index=0 y=67 text="Back" flags=0 action=0 selected=yes
[MAINOPTIONS] ITEM index=1 y=79 text="Video" flags=0 action=0 selected=no
[MAINOPTIONS] ITEM index=2 y=91 text="Input" flags=0 action=0 selected=no
[MAINOPTIONS] ITEM index=3 y=103 text="Sound" flags=0 action=0 selected=no
[MAINOPTIONS] HASH stage=logo fnv=0ac1f9c6
[MAINOPTIONS] HASH stage=item0 text="Back" fnv=c7258261
[MAINOPTIONS] HASH stage=item1 text="Video" fnv=4e764e2f
[MAINOPTIONS] HASH stage=item2 text="Input" fnv=175fa691
[MAINOPTIONS] HASH stage=item3 text="Sound" fnv=6058d47d
[MAINOPTIONS] framebufferFNV=6058d47d inputFNV=961109a7 changed=yes shapeData=0x0 mediaTexels=0x0
[MAINOPTIONS] End heap8=28704 largest8=17396 deltaFromStart=0 largestDelta=0
[VIDEO] Present 160x120 -> 320x240 exact 2x: 34426 us
[MAINOPTIONS] Presented real MENU_MAIN_OPTIONS model with bounded ESP32 paint
[MAINOPTIONS] READY MenuSystem_select executed for Options; no legacy Render_render, no map reload, no gameplay loader
[MAINOPTIONS] READY Options interaction intentionally remains disabled for this increment
```

`Present` timing is diagnostic only, not a regression requirement.

## New deterministic Options signatures

```text
MENU_MAIN selected Options input = 961109a7
MENU_MAIN_OPTIONS model FNV      = e1ef01f7
Options after logo               = 0ac1f9c6
Options after Back               = c7258261
Options after Video              = 4e764e2f
Options after Input              = 175fa691
Options after Sound / final      = 6058d47d
```

The authoritative final Options framebuffer for this exact presentation is:

```text
6058d47d
```

## Memory and legacy-resource boundary

The action retained exact allocator values:

```text
before: heap8=28704 largest8=17396
after:  heap8=28704 largest8=17396

deltaFromStart = 0
largestDelta   = 0
```

And throughout the transition:

```text
shapeData   = NULL
mediaTexels = NULL
```

No wall/sprite cache is active during the menu transition.

## Touch architecture status

The validated `MENU_MAIN` touch frontend remains:

```text
XPT2046
   -> PlatformInput
   -> one tap per press/release cycle
   -> 50 ms stable-release rearm
   -> physical 320x240 -> logical 160x120
   -> MENU_MAIN hit-test
   -> real MenuSystem.selectedIndex
   -> real p.bmp cursor
   -> second released tap = CONFIRM
```

Cursor movement still uses four static 13x10 RGB565 background patches:

```text
4 * 13 * 10 * 2 B = 1,040 B
```

No second 38,400-byte framebuffer is used.

## Linker-wrapper lesson from this increment

The first implementation attempt added:

```text
--wrap=DoomRPG_esp32MainMenuTouchOnTap
```

That was incorrect for this callback architecture. `native_main_menu_touch.c`
takes the address of `DoomRPG_esp32MainMenuTouchOnTap()` in the same translation
unit, while the existing tap gate compares callback function pointers in another
translation unit. The additional wrapper changed symbol identity for the gate but
did not intercept the local function-address reference.

Observed consequence:

```text
GATE READY missing
CONFIRM fell back to action=deferred
Options action never ran
```

The failed wrapper was removed on the same branch. The final implementation keeps
only the already validated `PlatformInput_setTapCallback` gate and performs the
Options dispatch explicitly at the confirmed-item boundary.

Rule for future work:

> Do not assume GNU `--wrap` will intercept a function pointer taken locally in
> the same translation unit. Be especially careful when pointer identity is part
> of the control flow.

## Deterministic regression boundaries

```text
sprite 172 texel FNV              = 0c0a7acd
wall 112 texel FNV                = 92d40704
synthetic projected wall FNV      = ad191f54
real walls framebuffer            = a6d87c4a
viewSprites list FNV              = 962cd657
sprite request FNV                = 4457ac94
walls + sprites framebuffer       = ffe0995e
faithful MENU_MAIN layout         = 86c38260
ESP32 160x120 layout config       = 47b3656e
historical fitted MENU_MAIN       = 1afa0223
touch Start Game selected         = cbc99461
touch Options selected            = 961109a7
touch Help/About selected         = e4eadfbb
touch Exit selected               = 5ff2a5cd
MENU_MAIN_OPTIONS model           = e1ef01f7
MENU_MAIN_OPTIONS framebuffer     = 6058d47d
```

## Display ownership

Normal mode remains:

```text
Serial
  `--> startup/probe/debug/touch diagnostics

TFT
  `--> shared game framebuffer only
```

`DOOMRPG_ESP32_SCREEN_DIAGNOSTICS=0` remains the default.

## Open visual issue: color / contrast

Comparison against the J2ME reference still shows the main-menu presentation on
CYD looking flatter / less saturated / lower contrast. Do not change palette
values blindly.

The Options screen in this increment uses a controlled black background, which
will be useful later when separating background/composition contrast from palette
conversion or physical TFT behaviour. No color correction is part of this
increment.

## Current safe stop boundary

Validated and executed:

- complete engine startup through mappings
- real menu BSP structural data
- native asset-pack access
- zero resident `shapeData`
- zero map-wide `mediaTexels`
- bounded GFXRM wall/sprite frames and LRU caches
- native projected walls and BSP-sorted sprites
- deterministic native menu scene `ffe0995e`
- real `MENU_MAIN` model/assets and fitted 160x120 presentation
- calibrated physical touch hit-testing
- real `MenuSystem.selectedIndex` changes from touch
- independently movable real `p.bmp` hand cursor
- bit-identical cursor background restoration
- second released tap on same row detected as `CONFIRM`
- **real `MenuSystem_select()` execution for confirmed Options**
- real `MENU_MAIN_OPTIONS` model (`Back / Video / Input / Sound`)
- bounded Options framebuffer `6058d47d`
- exact allocator restoration through Options transition

Still intentionally out of scope:

- touch interaction inside `MENU_MAIN_OPTIONS`
- `Back` transition to `MENU_MAIN`
- execution of `Video`, `Input` or `Sound`
- `Help/About` and `Exit` real actions
- Start Game / gameplay loader activation
- original monolithic bitshape/texel loaders
- textured floor/ceiling planes
- active normal multi-frame engine/menu loop
- final color/contrast correction
- normal gameplay controls
- audio

## Recommended next increment after merge

The smallest useful continuation is to make the Options screen itself touch-aware,
starting with the safest action: **Back**.

Recommended scope:

```text
Options screen appears
    -> touch Back
    -> first tap selects/arms using the same UX
    -> second released tap executes real back transition
    -> return to bounded MENU_MAIN presentation
    -> re-arm MENU_MAIN touch
```

Keep `Video`, `Input`, `Sound` and `Start Game` deferred until their respective
subsystems are understood and bounded.
