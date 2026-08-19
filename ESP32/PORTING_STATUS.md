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
- bounded sprite/wall GFXRM frames
- native projected wall + sprite rasterization
- real `menu.bsp` BSP traversal, camera and visible scene
- wall LRU3: 25 logical requests -> 11 physical loads
- sprite LRU3: 11 logical requests -> 9 physical loads
- native scene framebuffer `ffe0995e`
- original `MENU_MAIN` model/logo/hand/font overlay, faithful layout `86c38260`
- TFT bring-up diagnostics disabled by default; Serial diagnostics retained
- clean game-owned display, PR #27 merge
  `da2de773765f9675c4fe9eea1cbc82cf24b7523c`

## Current validated increment

Branch: `agent/esp32-main-menu-160x120-layout`

Base `main` SHA:

```text
da2de773765f9675c4fe9eea1cbc82cf24b7523c
```

Status: **HARDWARE VALIDATED, DOCUMENTED, READY TO MERGE**.

Objective: adapt only the real main-menu presentation to the 160x120 logical
framebuffer while preserving the real Doom RPG menu model, bitmap font, hand
cursor and four labels.

The faithful original-layout implementation remains in the repository as the
`86c38260` reference. The ESP32-specific layout is a separate presentation layer.

## Why adaptation is required

The normal Doom RPG font uses 12-pixel-high glyph cells. Four menu rows therefore
need 48 vertical pixels. The original main-menu layout starts at y=80:

```text
original rows = 80, 92, 104, 116
font height   = 12
logical H     = 120
```

The last row cannot fit. The original logo is also 108x74, so simply moving the
text upward would collide with the logo.

The chosen ESP32 adaptation keeps every real asset but scales only the logo:

```text
logo source = 108x74
logo target = 90x62
logo x/y    = 35,2

Start Game  y=67
Options     y=79
Help/About  y=91
Exit        y=103
font height = 12
```

Resulting geometry:

```text
logo bottom    = 64
first item y   = 67
logo/text gap  = 3 px
content bottom = 115
screen bottom  = 120
bottom margin  = 5 px
```

The real font remains unscaled/pixel-perfect. The real 13x10 hand cursor also
remains unscaled.

These coordinates live in:

```text
ESP32/include/native_main_menu_160x120_layout.h
```

They are intentionally shared so the next touch increment can derive hit zones
from the exact hardware-validated geometry instead of duplicating magic numbers.

## Model contract preserved

The original `Menu_initMenu(MENU_MAIN)` model remains unchanged:

```text
menu          = MENU_MAIN / 1
type          = MENUTYPE_MAIN / 4
selectedIndex = 0
scrollIndex   = 0
items         = 4

0 Start Game
1 Options
2 Help/About
3 Exit
```

Hardware model signature remains:

```text
modelFNV = bbc2149b
```

The underlying native scene is still the same deterministic scene:

```text
sceneFNV = ffe0995e
shapeData = NULL
mediaTexels = NULL
```

## Authoritative hardware validation

```text
=== Doom RPG ESP32 MENU_MAIN 160x120 layout ===
[MAINLAYOUT] Begin sceneFNV=ffe0995e expected=ffe0995e faithfulOriginalFNV=86c38260 heap8=29872 largest8=21492
[MAINLAYOUT] Model FNV=bbc2149b items=4 selected=0
[MAINLAYOUT] Geometry screen=160x120 logoSrc=108x74 logoDst=35,2 90x62 logoBottom=64 itemStart=67 line=12 rows=4 contentBottom=115 layoutFNV=47b3656e
[MAINLAYOUT] ITEM index=0 y=67 selected=yes text="Start Game"
[MAINLAYOUT] ITEM index=1 y=79 selected=no text="Options   "
[MAINLAYOUT] ITEM index=2 y=91 selected=no text="Help/About"
[MAINLAYOUT] ITEM index=3 y=103 selected=no text="Exit      "
[MAINLAYOUT] HASH stage=logo fnv=1e8bcfbb
[MAINLAYOUT] HASH stage=item0 text="Start Game" fnv=64516fd1
[MAINLAYOUT] HASH stage=item1 text="Options   " fnv=0fb73263
[MAINLAYOUT] HASH stage=item2 text="Help/About" fnv=c7a0b65f
[MAINLAYOUT] HASH stage=item3 text="Exit      " fnv=1afa0223
[MAINLAYOUT] framebufferFNV=1afa0223 sceneFNV=ffe0995e changed=yes composeMs=19 shapeData=0x0 mediaTexels=0x0
[MAINLAYOUT] End heap8=29872 largest8=21492 deltaFromStart=0 largestDelta=0
[MAINLAYOUT] Presented fitted Doom RPG MENU_MAIN on clean CYD display
[MAINLAYOUT] READY original menu model/font/hand preserved; only logo scale + target geometry changed
[MAINLAYOUT] READY item rows are stable for the next touch hit-test increment
```

`composeMs=19` is diagnostic only, not a regression requirement.

## Deterministic signatures

Useful current regression boundaries:

```text
sprite 172 texel FNV         = 0c0a7acd
wall 112 texel FNV           = 92d40704
synthetic projected wall FNV = ad191f54
real walls framebuffer       = a6d87c4a
viewSprites list FNV         = 962cd657
sprite request FNV           = 4457ac94
walls + sprites framebuffer  = ffe0995e
faithful MENU_MAIN layout    = 86c38260
ESP32 160x120 layout config  = 47b3656e
ESP32 fitted MENU_MAIN       = 1afa0223
```

Progressive fitted-layout hashes:

```text
after scaled logo          = 1e8bcfbb
after Start Game + hand    = 64516fd1
after Options              = 0fb73263
after Help/About           = c7a0b65f
after Exit / final         = 1afa0223
```

## Memory boundary

The layout introduces no per-composition allocation:

```text
before: heap8=29872 largest8=21492
after:  heap8=29872 largest8=21492

deltaFromStart = 0
largestDelta   = 0
```

`shapeData` and `mediaTexels` remain `NULL` for the entire path.

## Display ownership remains clean

Normal mode remains:

```text
Serial
  `--> startup/probe/debug/touch diagnostics

TFT
  `--> shared game framebuffer only
```

`DOOMRPG_ESP32_SCREEN_DIAGNOSTICS=0` remains the default. The old bring-up visual
tools can still be temporarily restored with value `1`.

## Hardware visual observation: color/contrast

The fitted geometry is visually successful on hardware: all four rows fit cleanly
and the selected hand is visible in the intended location.

However, comparison against a J2ME reference capture shows the CYD presentation
looks noticeably **flatter / less saturated / lower contrast**. This is now a
recorded open visual issue, but it is **not** treated as a failure of this layout
increment because:

- the menu model and geometry are correct;
- the framebuffer hashes are deterministic;
- all memory/raster invariants pass;
- the issue may involve composition/background, palette conversion, display
  characteristics or another presentation stage.

Do **not** change palette values blindly. Investigate this in a separate measured
increment, ideally comparing the same menu assets against a black/controlled
background and checking framebuffer colors separately from physical TFT output.

`1afa0223` is therefore the deterministic reference for this exact fitted
composition, not a claim that its color appearance is already final/perfect.

## Current safe stop boundary

Validated and executed:

- complete engine startup through mappings
- real menu BSP structural data
- native asset-pack access
- zero resident `shapeData`
- zero map-wide `mediaTexels`
- bounded GFXRM wall/sprite frames
- hardware-validated wall and sprite LRU caches
- native projected walls and BSP-sorted sprites
- deterministic native scene `ffe0995e`
- original `MENU_MAIN` model and real UI assets
- faithful original menu composition `86c38260`
- clean TFT diagnostics policy
- ESP32-specific fitted `MENU_MAIN` composition `1afa0223`
- exact allocator restoration
- final item geometry suitable for touch hit-testing

Still intentionally out of scope:

- original monolithic bitshape/texel loaders
- textured floor/ceiling planes
- persistent caches in the normal multi-frame runtime
- active normal `ST_MENU` loop
- touch selection/validation of real menu items
- final color/contrast correction
- normal gameplay loop and gameplay control scheme
- audio

## Recommended next increments after merge

The layout geometry is now stable enough for the planned menu touch behaviour:

```text
first tap on a different item -> move selectedIndex / hand only
second tap on same selected item -> validate selection
```

The hit-test must reuse the constants in
`native_main_menu_160x120_layout.h` and convert physical 320x240 touch coordinates
to logical 160x120 coordinates.

Keep gameplay input deferred until the first actual gameplay map is running.

The color/contrast discrepancy versus J2ME is also recorded and should be handled
as a separate diagnostic increment rather than mixed into touch semantics.
