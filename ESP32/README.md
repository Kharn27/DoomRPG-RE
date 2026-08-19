# Doom RPG ESP32 port

This directory contains the ESP32-specific port and bring-up work for the classic
ESP32-2432S028R Cheap Yellow Display with **no PSRAM**.

DoomRPG-RE is used as a behavioural/data-format reference while resource
management, rendering and UI composition are progressively rebuilt around the
actual target constraints.

For the authoritative recovery point and exact hardware figures, see
[`PORTING_STATUS.md`](PORTING_STATUS.md).

## Current target

- ESP32-2432S028R / classic CYD, no PSRAM
- ESP32-D0WD-V3, 240 MHz, 4 MB flash
- ILI9341 320x240 landscape
- XPT2046 touch
- internal render target: 160x120 RGB565
- gameplay viewport: 160x80 at framebuffer y=20
- exact 2x nearest-neighbour TFT output
- microSD-backed resources
- audio disabled during bring-up

## Build and flash

```bash
cd ESP32
pio run -t upload
pio device monitor
```

Clean only when generated-source/linker changes make it useful:

```bash
cd ESP32
pio run -t clean
pio run -t upload
```

## Screen diagnostics policy

Normal firmware reserves the TFT for the game framebuffer.

```text
-D DOOMRPG_ESP32_SCREEN_DIAGNOSTICS=0
```

With `0`, the old bring-up screen, direct TFT labels, touch crosshair and touch
triggered SDL test pattern are disabled. Serial startup/probe/`[ALIVE]`/`[TOUCH]`
logs remain available.

Set the flag to `1` temporarily to restore the old visual hardware diagnostics.

## Touch status

The XPT2046 reader is alive and calibrated in physical 320x240 landscape
coordinates. It currently reports touches on Serial but does not yet activate
menu actions.

Future menu input will convert physical coordinates to logical 160x120 and use
the final layout geometry documented below.

Gameplay controls are intentionally deferred until the first actual gameplay map
is loaded and visible.

## Legacy header include rule

Several original engine headers assume SDL types are already visible. ESP32 C
files that include `DoomRPG.h`, `Render.h`, etc. should include SDL first:

```c
#include <SDL.h>
#include "DoomRPG.h"
#include "Render.h"
```

## SD card contents

During migration:

```text
/DoomRPG.zip
/DoomRPG-ESP32.pak
```

`DoomRPG.zip` remains only for legacy engine paths not yet migrated.
`DoomRPG-ESP32.pak` is the native direct-access resource backend.

Build the native pack with:

```bash
python3 ESP32/tools/build_asset_pack.py \
    /path/to/DoomRPG.zip \
    /path/to/DoomRPG-ESP32.pak
```

The builder must finish with:

```text
[PACK] self-check: OK
```

## Native graphics architecture

The original map-wide graphics architecture cannot fit the no-PSRAM target.
The ESP32 path uses bounded frames and small measured caches:

```text
                     SD / DoomRPG-ESP32.pak
                              |
                              v
                            GFXRM
                          /       \
               sprite frames      wall frames
                    |                  |
        NativeSpriteLruCache(3)   NativeWallLruCache(3)
                    |                  |
                    v                  v
          native projected       native projected
             sprite spans           wall spans
                    \                  /
                     \                /
                      shared 160x120 RGB565
```

Strong invariant:

```text
shapeData   = NULL
mediaTexels = NULL
```

Do not reintroduce those map-wide pools as a shortcut.

## Stable graphics signatures

```text
sprite 172 texel FNV         = 0c0a7acd
wall 112 texel FNV           = 92d40704
synthetic projected wall FNV = ad191f54
real walls framebuffer       = a6d87c4a
viewSprites list FNV         = 962cd657
sprite request FNV           = 4457ac94
walls + sprites framebuffer  = ffe0995e
```

The deterministic menu camera is:

```text
spawnIndex    = 460
spawn tile    = 12,14
world X/Y     = 800,928
direction     = 0
camera Z      = 36
animFrameTime = 0
```

## Hardware-validated caches

Wall LRU3:

```text
25 logical requests
14 hits
11 misses
8 evictions
11 physical wall loads
6,144 B logical cache payload
framebuffer = a6d87c4a
```

Sprite LRU3:

```text
11 logical sprite requests
2 hits
9 misses
6 evictions
9 physical sprite loads
6,038 B peak logical payload
framebuffer = ffe0995e
```

## Main-menu composition

The real Doom RPG `MENU_MAIN` model is preserved:

```text
Start Game
Options
Help/About
Exit
```

Real assets:

```text
j.bmp -> logo
p.bmp -> selected hand cursor
DoomCanvas imgFont -> menu text
```

Original hardware asset sizes:

```text
logo = 108x74
hand = 13x10
font sheet = 144x72
normal glyph cell = 9x12
normal glyph advance = 7 px
```

The original menu remains conceptually an overlay over the native menu scene:

```text
native scene ffe0995e
       |
       +--> logo
       +--> hand
       +--> font
```

The faithful original layout remains available as a regression reference:

```text
faithful MENU_MAIN framebuffer = 86c38260
modelFNV                       = bbc2149b
```

## ESP32 160x120 main-menu layout

The original row placement (`80,92,104,116`) cannot fit four 12-pixel-high rows
inside a 120-pixel framebuffer. The ESP32-specific presentation therefore scales
**only the logo** and keeps the font/hand untouched.

Hardware-validated geometry:

```text
logical screen = 160x120

logo source = 108x74
logo target = 90x62
logo x/y    = 35,2
logo bottom = 64

Start Game = y 67
Options    = y 79
Help/About = y 91
Exit       = y 103

font height    = 12
content bottom = 115
bottom margin  = 5 px
```

Shared constants live in:

```text
ESP32/include/native_main_menu_160x120_layout.h
```

The next touch-input increment must use these constants instead of duplicating
menu coordinates.

### Hardware-validated fitted layout signatures

```text
scene before UI             = ffe0995e
MENU_MAIN model             = bbc2149b
layout geometry FNV         = 47b3656e
after scaled logo           = 1e8bcfbb
after Start Game + hand     = 64516fd1
after Options               = 0fb73263
after Help/About            = c7a0b65f
after Exit / final          = 1afa0223
```

Authoritative final marker:

```text
[MAINLAYOUT] framebufferFNV=1afa0223 sceneFNV=ffe0995e changed=yes composeMs=19 shapeData=0x0 mediaTexels=0x0
[MAINLAYOUT] End heap8=29872 largest8=21492 deltaFromStart=0 largestDelta=0
```

The layout composition adds no persistent/per-pass heap allocation.

## Color / contrast observation

The fitted layout is geometrically successful, but comparison against a J2ME
reference screenshot shows the CYD result currently looks **flatter and less
saturated / lower contrast**.

This is an open visual issue, not a reason to invalidate the current layout
increment. Do not tweak the palette blindly. Investigate it separately with a
controlled test that can distinguish among:

- background/composition contrast;
- palette/RGB565 conversion;
- SDL texture modulation/blitting;
- physical TFT appearance.

A useful future diagnostic is to render the same menu assets on a controlled
black background and compare framebuffer colors before TFT presentation.

`1afa0223` is the deterministic signature of the current fitted composition; it
is not a claim that the final color grading is already correct.

## Current safe boundary

Validated:

- real `menu.bsp` scene and BSP traversal
- native projected walls/sprites
- bounded wall/sprite resource loading and LRU caches
- deterministic native scene `ffe0995e`
- original `MENU_MAIN` model and real assets
- clean TFT ownership with Serial-only diagnostics
- faithful original menu reference `86c38260`
- fitted 160x120 menu `1afa0223`
- exact allocator restoration
- stable row geometry ready for touch hit-testing

Still intentionally out of scope:

- original monolithic bitshape/texel loaders
- textured floor/ceiling planes
- persistent caches in the normal multi-frame runtime
- active normal `ST_MENU` state machine
- touch menu selection/validation
- final color/contrast correction
- normal gameplay loop / gameplay control scheme
- audio

## Recommended next direction

With the final row geometry now validated, menu touch can be added without
inventing a second UI model:

```text
first tap on a different item
    -> update selectedIndex
    -> move the real hand
    -> do not validate

second tap on the already selected item
    -> validate through the real menu path
```

Use the constants in `native_main_menu_160x120_layout.h` for hit-testing and map
physical 320x240 touch coordinates to logical 160x120 first.

Keep color/contrast investigation as a separate measured increment so input
semantics and rendering diagnostics do not get mixed.

## Porting workflow

1. create one branch from the latest hardware-validated `main`
2. implement one small measurable objective
3. build/flash/test on the real CYD
4. fix failures on the same branch
5. after hardware success, update every relevant `.md` on that branch
6. only when code + documentation agree is the branch merge-ready
7. merge
8. only then start the next increment
