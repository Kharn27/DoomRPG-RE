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
is progressively becoming its own small engine:

- bounded deterministic RAM use
- SD as immutable backing store
- small measured working sets and evidence-driven caches
- no monolithic `shapeData`
- no map-wide `mediaTexels`
- one shared 160x120 RGB565 framebuffer
- canonical RGB565 palette convention for native consumers
- storage/file-format access isolated behind GFXRM
- cache/reuse policy isolated from storage and rasterization
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

## Hardware-validated milestones already merged to main

1. TFT / SD / engine link bring-up.
2. Touch calibration/orientation, PR #1.
3. Native 160x120 framebuffer + exact 2x output, PR #2.
4. SDL compatibility renderer sharing the platform framebuffer, PR #3.
5. Real Doom RPG core graph, PR #4.
6. `DoomCanvas_startup()` + HUD/font/legal resources + 160x120 layout, PR #5.
7. `ParticleSystem`, `MenuSystem`, `EntityDef` startup with packed indexed BMPs, PR #6.
8. Real `Render_startup()` + `sintable.bin` + `palettes.bin`, PR #7.
9. `Game_loadConfig()` + real `Render_loadMappings()`, PR #8.
10. Real `/menu.bsp` inflate + 33-byte header parse, PR #9.
11. Complete menu BSP structural allocation plan, PR #10.
12. Real map runtime structures stopped before graphics inflation, PR #11.
13. Proof the original monolithic graphics path cannot fit no-PSRAM ESP32, PR #12.
14. First native asset-pack random-access primitive, PR #13.
15. Full ESP32 native asset pack v2, 241 direct-access resources, PR #14.
16. Native on-demand bitshape model, zero resident `shapeData`, PR #15.
17. Native sprite-texel random access, PR #16.
18. First complete native sprite render, sprite 172, PR #17.
19. First complete native wall render, texture 112, PR #18.
20. Shared bounded GFXRM resource manager, PR #19.
21. First projected wall through bounded GFXRM compatibility bridge, PR #20.
22. Direct native projected-wall sampling, framebuffer `ad191f54`, PR #21.
23. First real `menu.bsp` walls-only frame, framebuffer `a6d87c4a`, PR #22.
24. Three-slot wall LRU: 25 requests -> 11 physical loads, same `a6d87c4a`,
    PR #23, merge `e39079253ef72c17a908c1c2633762d20fe5f29e`.
25. Real BSP-sorted menu sprites over real walls, final framebuffer `ffe0995e`,
    PR #24, merge `0ba4ca8efa15974846bee638333e52e12bbda0e3`.
26. Three-slot sprite LRU: 11 logical sprite requests -> 9 physical loads,
    final framebuffer still `ffe0995e`, PR #25, merge
    `f551e2f840e66abfa699e114b77398e6c687752a`.

## Current validated increment

Branch: `agent/esp32-real-main-menu-overlay`

Base `main` SHA:

```text
f551e2f840e66abfa699e114b77398e6c687752a
```

Status: **HARDWARE VALIDATED, DOCUMENTED, READY TO MERGE**.

Objective: keep the hardware-validated native real menu scene unchanged, then
compose the original Doom RPG `MENU_MAIN` UI over it using the already resident
logo, hand cursor and bitmap font, without calling the legacy monolithic
`Render_render()` path again.

This is the first hardware-validated composition of the original main-menu model
over the ESP32-native scene.

## Pre-overlay deterministic scene contract

The scene underneath the menu remains exactly the PR #25 regression:

```text
real menu walls framebuffer = a6d87c4a
viewSprites list FNV        = 962cd657
sprite request FNV          = 4457ac94
walls + sprites framebuffer = ffe0995e
wall LRU                    = 14 hits / 11 misses / 8 evictions
sprite LRU                  = 2 hits / 9 misses / 6 evictions
shapeData                    = NULL
mediaTexels                  = NULL
```

The wall and sprite caches are completely torn down before UI composition.

## Original `MENU_MAIN` behaviour used as specification

`Menu_initMenu(MENU_MAIN)` builds the original model:

```text
menu          = MENU_MAIN (1)
type          = MENUTYPE_MAIN (4)
selectedIndex = 0
scrollIndex   = 0
items         = 4
imgBG         = imgLogo

0  Start Game
1  Options
2  Help/About
3  Exit
```

`MenuSystem_startup()` already owns the real menu resources:

```text
p.bmp -> imgHand
q.bmp -> imgArrowUpDown
j.bmp -> imgLogo
```

The current hardware values are:

```text
logo = 108x74, transparent
hand = 13x10, transparent
font = 144x72
largeStatus = 0
```

The menu is intentionally an **overlay over the rendered 3D menu scene**. The
original `MenuSystem_paint()` clears/re-renders the scene, then draws `imgBG`, the
hand cursor and the text items. The ESP32 increment preserves that visual model
but reuses the already validated native scene instead of invoking legacy
`Render_render()` again.

## Shared framebuffer composition

The ESP32 SDL shim already implements `SDL_RenderCopy()` as a software blit into
`PlatformVideo_framebuffer()`. Therefore menu BMPs and fonts compose directly into
the same 160x120 RGB565 framebuffer used by native walls/sprites:

```text
native menu walls + sprites
          |
          v
      ffe0995e
          |
          +--> j.bmp logo
          +--> p.bmp selected hand
          +--> bitmap font / four labels
          |
          v
      86c38260
          |
          v
    exact 2x TFT present
```

No second framebuffer and no new 2D renderer were introduced.

## Integration boundary

The validated `DoomRPG_probeNativeMenuSpriteFrame()` implementation is untouched.
A GNU ld wrapper chains the overlay only after the real scene probe succeeds and
all caches are released:

```text
__wrap_DoomRPG_probeNativeMenuSpriteFrame
    |
    +--> __real_DoomRPG_probeNativeMenuSpriteFrame
    |       `--> requires ffe0995e and cache teardown
    |
    `--> DoomRPG_probeNativeMainMenuOverlay
```

PlatformIO adds:

```text
-Wl,--wrap=DoomRPG_probeNativeMenuSpriteFrame
```

A build failure during this increment exposed the legacy header dependency that
`DoomRPG.h` / `Render.h` expect SDL types to be visible first. The bridge was
fixed on the same branch by including `<SDL.h>` before those engine headers.

## Authoritative hardware validation

```text
=== Doom RPG ESP32 real MENU_MAIN overlay ===
[MAINMENU] Begin sceneFNV=ffe0995e expected=ffe0995e heap8=29852 largest8=21492 shapeData=0x0 mediaTexels=0x0
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
[MAINMENU] End heap8=29852 largest8=21492 deltaFromStart=0 largestDelta=0
[MAINMENU] Presented real Doom RPG MENU_MAIN overlay over native menu scene
[MAINMENU] READY original Menu model + logo + hand + font composed without legacy Render_render
[MAINMENU] READY framebuffer now contains native scene plus real main-menu UI; interaction remains intentionally out of scope
[ALIVE] ... heap8=29852 largest8=21492 ... MENUBSP=ready ...
```

## Deterministic menu signatures

These progressive hashes identify where UI composition diverges if a future
change breaks transparency, glyph rendering, positioning or color modulation:

```text
scene before UI          = ffe0995e
MENU_MAIN model FNV      = bbc2149b
after logo               = 0bcc168b
after Start Game + hand  = 73aa5852
after Options            = 82375e8e
after Help/About         = 249ad9b4
after Exit               = 86c38260
final MENU_MAIN FNV      = 86c38260
```

`composeMs=21` is diagnostic only, not a regression requirement.

## Memory boundary

Before UI composition:

```text
heap8    = 29852
largest8 = 21492
```

After UI composition:

```text
heap8    = 29852
largest8 = 21492
deltaFromStart = 0
largestDelta   = 0
```

The menu assets were already resident from startup, so this overlay requires no
new persistent heap allocation.

## Hardware visual observation

The hardware photo confirms the expected original-style menu overlay over the
native 3D scene. It also exposes two intentionally unresolved presentation issues:

1. **Original menu layout is cramped on 160x120.** `MENUTYPE_MAIN` starts at
   `y=80` and advances by 12 pixels per item. Four rows therefore extend toward
   y=128, beyond a 120-pixel logical height. The 108x74 logo also occupies a large
   fraction of the screen. This is a layout adaptation problem, not a failed blit.
2. **Bring-up TFT diagnostics are still visible around/over the game area.** Text
   such as `Sprite: MENU BSP OK` is diagnostic output from the current firmware,
   not part of the `86c38260` menu framebuffer contract.

Do not "fix" either issue by changing the validated overlay on this branch. Keep
this branch as the faithful composition baseline and address presentation cleanup
in a later dedicated increment.

## Current native graphics/UI architecture

```text
                  SD / DoomRPG-ESP32.pak
                           |
                           v
                         GFXRM
                       /       \
                      /         \
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
            original menu UI composition
              /          |          \
          j.bmp        p.bmp       font
              \          |          /
                         v
                     86c38260
                         |
                         v
                  exact 2x CYD TFT
```

## Current safe stop boundary

Validated and executed:

- complete engine startup through mappings
- real `Render_beginLoadMap(MAP_MENU)` structural load
- 53 nodes, 120 lines, 44 map sprites, 68 runtime sprite slots, 15 events
- native asset-pack access
- zero resident `shapeData`
- zero map-wide `mediaTexels`
- canonical RGB565 palette normalization
- bounded GFXRM sprite + wall frames
- native projected walls and BSP-sorted sprites
- hardware-validated three-slot wall LRU
- hardware-validated three-slot sprite LRU
- deterministic native scene `ffe0995e`
- real `MENU_MAIN` model built by original `Menu_initMenu()`
- real `j.bmp` logo, `p.bmp` hand and Doom RPG bitmap font
- deterministic real-menu overlay `86c38260`
- zero heap/largest-block change during menu composition

Still intentionally NOT integrated:

- original `Render_loadBitShapes()`
- original `Render_loadTexels()`
- monolithic `shapeData`
- monolithic/map-wide `mediaTexels`
- textured floor/ceiling planes
- persistent caches across a normal multi-frame runtime
- persistent open native pack
- complete graphics-loader replacement
- active `ST_MENU` state machine / normal main loop
- touch navigation through real menu items
- 160x120-specific menu layout adaptation
- removal of bring-up TFT debug overlay
- gameplay entities/player in the normal gameplay loop
- audio

## Recommended next increment after merge

The next increment should stay focused on **menu runtime integration**, not add
another renderer subsystem.

Recommended order:

1. make the current real `MENU_MAIN` composition the active `ST_MENU` screen in
   the normal loop without reintroducing legacy `Render_render()`;
2. route existing touch/input actions to `MenuSystem_moveDir()` /
   `MenuSystem_select()` and prove the selected hand moves between the four real
   items;
3. only after interaction works, adapt the `MENUTYPE_MAIN` layout for 160x120 and
   remove the bring-up TFT diagnostic text in separate small increments.

Keep `ffe0995e` as the pre-overlay scene boundary and `86c38260` as the faithful
original-layout overlay reference while making these changes.
