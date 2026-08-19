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
- internal render target: 160x120 RGB565 = 38,400 B
- gameplay viewport: 160x80 at framebuffer y=20
- physical output: exact nearest-neighbour 2x to 320x240
- audio disabled/stubbed during bring-up

## Project direction

DoomRPG-RE is treated as an executable specification for behaviour, data formats
and useful rendering semantics, not as an architecture contract. The ESP32 target
is progressively becoming its own constrained engine:

- bounded deterministic RAM use
- SD as immutable backing store
- small measured working sets and evidence-driven caches
- no monolithic `shapeData`
- no map-wide `mediaTexels`
- one shared 160x120 RGB565 framebuffer
- storage/file-format access isolated behind GFXRM
- cache policy isolated from storage and rasterization
- original BSP/projection/game/menu semantics preserved where useful
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

## Hardware-validated milestones merged to main

1. TFT / SD / engine link bring-up.
2. Touch calibration/orientation, PR #1.
3. Native 160x120 framebuffer + exact 2x output, PR #2.
4. SDL compatibility renderer sharing the platform framebuffer, PR #3.
5. Real Doom RPG core graph, PR #4.
6. `DoomCanvas_startup()` + HUD/font/legal resources + 160x120 layout, PR #5.
7. `ParticleSystem`, `MenuSystem`, `EntityDef` startup, PR #6.
8. Real `Render_startup()` + sine table + palettes, PR #7.
9. `Game_loadConfig()` + real `Render_loadMappings()`, PR #8.
10. Real `/menu.bsp` inflate + header parse, PR #9.
11. Complete menu BSP structural allocation plan, PR #10.
12. Real map runtime structures stopped before graphics inflation, PR #11.
13. Proof original monolithic graphics cannot fit no-PSRAM ESP32, PR #12.
14. First native asset-pack random-access primitive, PR #13.
15. Full ESP32 native asset pack v2, 241 direct-access resources, PR #14.
16. Native on-demand bitshape model, zero resident `shapeData`, PR #15.
17. Native sprite-texel random access, PR #16.
18. First complete native sprite render, sprite 172, PR #17.
19. First complete native wall render, texture 112, PR #18.
20. Shared bounded GFXRM resource manager, PR #19.
21. First projected wall through bounded GFXRM bridge, PR #20.
22. Direct native projected-wall sampling, framebuffer `ad191f54`, PR #21.
23. First real `menu.bsp` walls-only frame, framebuffer `a6d87c4a`, PR #22.
24. Three-slot wall LRU: 25 requests -> 11 loads, same `a6d87c4a`, PR #23.
25. Real BSP-sorted menu sprites, final framebuffer `ffe0995e`, PR #24.
26. Three-slot sprite LRU: 11 sprite requests -> 9 physical loads, same
    `ffe0995e`, PR #25.
27. Original `MENU_MAIN` model/logo/hand/font composed over the native scene,
    final framebuffer `86c38260`, PR #26, merge
    `c327e533e59e8ebafa85a3b6a4e396c10e4a87d0`.

## Current validated increment

Branch: `agent/esp32-clean-game-display`

Base `main` SHA:

```text
c327e533e59e8ebafa85a3b6a4e396c10e4a87d0
```

Status: **HARDWARE VALIDATED, DOCUMENTED, READY TO MERGE**.

Objective: stop all bring-up/debug drawing from touching the TFT while preserving
all serial diagnostics, all probes, the touch reader and all deterministic game
framebuffer output.

The game framebuffer is now the sole normal owner of visible TFT content.

## Clean-display policy

The firmware now has an explicit compile-time switch:

```text
DOOMRPG_ESP32_SCREEN_DIAGNOSTICS=0
```

Default value is `0` in `platformio.ini` and in `main.cpp` as a fallback.

With diagnostics disabled:

- `drawDiagnosticScreen()` performs no TFT drawing;
- `drawLabel()` performs no TFT drawing;
- touch events do not invoke `Esp32Sdl_showTestPattern()`;
- touch events do not draw cyan crosshairs or coordinates on the TFT;
- all `[TOUCH]`, `[ALIVE]`, startup and probe logs remain available on Serial;
- normal game/UI composition through the shared framebuffer remains unchanged.

Setting the flag back to `1` restores the old visual bring-up tools for hardware
troubleshooting without having to recreate them.

## Deterministic graphics regression

The cleanup only removes direct diagnostic TFT writes. It does not change the
160x120 framebuffer pipeline.

The hardware-validated signatures remain:

```text
real walls framebuffer       = a6d87c4a
viewSprites list FNV         = 962cd657
sprite request FNV           = 4457ac94
walls + sprites framebuffer  = ffe0995e
MENU_MAIN model FNV          = bbc2149b
MENU_MAIN final framebuffer  = 86c38260
shapeData                    = NULL
mediaTexels                  = NULL
```

The `MENU_MAIN` progressive composition remains:

```text
scene before UI          = ffe0995e
after logo               = 0bcc168b
after Start Game + hand  = 73aa5852
after Options            = 82375e8e
after Help/About         = 249ad9b4
after Exit / final       = 86c38260
```

## Authoritative hardware validation

```text
=== Doom RPG ESP32 real MENU_MAIN overlay ===
[MAINMENU] Begin sceneFNV=ffe0995e expected=ffe0995e heap8=29872 largest8=21492 shapeData=0x0 mediaTexels=0x0
[MAINMENU] Assets logo=108x74 transparent=1 hand=13x10 transparent=1 font=144x72 largeStatus=0
[MAINMENU] Model menu=1 type=4 items=4 selected=0 scroll=0 maxItems=10 modelFNV=bbc2149b imgBG=logo
[MAINMENU] ITEM index=0 selected=yes flags=2 action=0 text="Start Game"
[MAINMENU] ITEM index=1 selected=no flags=2 action=0 text="Options   "
[MAINMENU] ITEM index=2 selected=no flags=2 action=0 text="Help/About"
[MAINMENU] ITEM index=3 selected=no flags=2 action=0 text="Exit      "
[MAINMENU] HASH stage=logo fnv=0bcc168b
[MAINMENU] HASH stage=item0 text="Start Game" fnv=73aa5852
[MAINMENU] HASH stage=item1 text="Options   " fnv=82375e8e
[MAINMENU] HASH stage=item2 text="Help/About" fnv=249ad9b4
[MAINMENU] HASH stage=item3 text="Exit      " fnv=86c38260
[MAINMENU] framebufferFNV=86c38260 sceneFNV=ffe0995e changed=yes composeMs=21 shapeData=0x0 mediaTexels=0x0
[MAINMENU] End heap8=29872 largest8=21492 deltaFromStart=0 largestDelta=0
[MAINMENU] Presented real Doom RPG MENU_MAIN overlay over native menu scene
[MAINMENU] READY original Menu model + logo + hand + font composed without legacy Render_render
[MAINMENU] READY framebuffer now contains native scene plus real main-menu UI; interaction remains intentionally out of scope
[READY] Bring-up alive; touch diagnostics are serial-only and TFT is reserved for the game framebuffer.
[ALIVE] ... heap8=29872 largest8=21492 ... MENUBSP=ready ...
```

The branch's small static-code change shifts the absolute baseline from the
previous 29,852 B to 29,872 B free heap8. The contract is still exact
before/after restoration within the same firmware.

## Touch validation

Hardware touch input remains alive while screen side effects are disabled.
Observed serial-only touch samples after the menu was visible:

```text
[TOUCH] raw=3465,1745 pressure=2239 screen=129,215
[TOUCH] raw=3261,1396 pressure=2434 screen=97,201
[TOUCH] raw=3393,1180 pressure=2433 screen=78,210
[TOUCH] raw=3617,1510 pressure=2295 screen=108,226
```

The hardware photo and these touches confirm that no diagnostic crosshair, label
or SDL test pattern replaced or corrupted the displayed Doom RPG menu.

This is an important prerequisite for using the same `PlatformInput` reader for
real menu interaction later.

## Current display ownership

Normal mode is now conceptually:

```text
Serial
  |
  +--> all startup/probe/debug diagnostics

TFT
  |
  `--> PlatformVideo shared framebuffer only
          |
          +--> native 3D scene
          `--> Doom RPG UI/menu composition
```

Direct diagnostic TFT writes remain available only when
`DOOMRPG_ESP32_SCREEN_DIAGNOSTICS=1`.

## Real `MENU_MAIN` overlay contract

The menu remains an overlay over the native 3D menu scene, matching the original
Doom RPG behaviour:

```text
native scene ffe0995e
       |
       +--> j.bmp logo 108x74
       +--> p.bmp hand 13x10
       +--> Doom RPG bitmap font
       |
       v
MENU_MAIN 86c38260
```

The original layout is still deliberately unchanged in this increment.

## Known 160x120 layout limitation

The faithful original menu layout is cramped on the 160x120 logical target:

```text
logical screen height = 120
MENU_MAIN first row y  = 80
row step               = 12
four rows extend to    ~128
logo size              = 108x74
```

The hardware photo clearly shows the menu is functional but too large/tight for
this target. This is a layout adaptation issue, not a rendering failure.

Do not change the `86c38260` faithful-layout regression retroactively. A later
ESP32 layout increment should preserve the real resources/model while choosing
coordinates that fit 160x120 cleanly.

## Current native graphics/UI architecture

```text
                  SD / DoomRPG-ESP32.pak
                           |
                           v
                         GFXRM
                       /       \
            Sprite LRU(3)       Wall LRU(3)
                 |                 |
                 v                 v
          native sprite       native wall
             spans               spans
                 \                 /
                  \               /
                shared 160x120 RGB565
                         |
                         v
                     ffe0995e
                         |
            original MENU_MAIN UI
                         |
                         v
                     86c38260
                         |
                         v
                    exact 2x TFT
```

## Current safe stop boundary

Validated and executed:

- complete engine startup through mappings
- real menu BSP structural data
- native asset-pack access
- zero resident `shapeData`
- zero map-wide `mediaTexels`
- bounded GFXRM sprite/wall frames
- hardware-validated wall LRU and sprite LRU
- native projected walls and BSP-sorted sprites
- deterministic native scene `ffe0995e`
- original `MENU_MAIN` model and real UI assets
- deterministic menu overlay `86c38260`
- exact allocator restoration
- TFT diagnostic drawing disabled by default
- serial diagnostics preserved
- touch remains readable without modifying visible game output

Still intentionally out of scope:

- original monolithic bitshape/texel loaders
- textured floor/ceiling planes
- persistent caches in a normal multi-frame runtime
- active normal `ST_MENU` loop
- touch navigation of real menu items
- 160x120-specific menu layout adaptation
- gameplay entities/player in the normal gameplay loop
- audio

## Recommended next increment after merge

Do the **160x120 `MENU_MAIN` layout adaptation next**, before touch hit-testing.

Reason: touch zones should be built around the final ESP32 menu coordinates, not
around the current faithful layout where the fourth row extends beyond the logical
screen.

Recommended sequence:

1. preserve `ffe0995e` as the native-scene regression boundary;
2. keep the real `MENU_MAIN` model, `j.bmp`, `p.bmp` and Doom RPG font;
3. move/scale only what is necessary so all four items fit cleanly in 160x120;
4. establish a new deterministic framebuffer hash for the ESP32-adapted layout;
5. then add touch behaviour: first tap selects/moves the hand, second tap on the
   same item validates through the real menu model;
6. gameplay input remains intentionally deferred until the first actual game map
   is loaded and visible.
