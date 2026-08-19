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

The XPT2046 reader is alive, calibrated and now drives the real `MENU_MAIN`
selection on hardware.

Input path:

```text
XPT2046
  -> PlatformInput
  -> calibrated physical 320x240 point
  -> semantic press/release tap
  -> logical 160x120 hit-test
  -> real MenuSystem.selectedIndex
  -> real p.bmp hand cursor
```

Semantic taps are delivered immediately on the press edge and a second tap is
blocked until the panel has been released continuously for 50 ms. This prevents
a held finger from becoming an accidental double tap and avoids inheriting the
old ~80 ms Serial diagnostic throttle.

Current `MENU_MAIN` behaviour:

```text
first tap on another row
    -> move selection + hand

first tap on the already selected row
    -> arm confirmation

second released tap on the same row
    -> CONFIRM-PASS / CONFIRM
    -> action intentionally deferred
```

Important: a working double tap currently produces **no visible menu transition**.
That is deliberate. `MenuSystem_select()` is not called yet on this branch because
`Start Game` can enter the not-yet-migrated gameplay loader.

Gameplay controls remain deferred until the first actual gameplay map is running.

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

Touch hit-testing reuses these exact constants.

### Historical fitted-layout signatures

The PR #28 layout used the original selected-item convention where the selected
text is shifted 2 px after the hand:

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

`1afa0223` remains a valid historical regression reference.

## Touch-ready hand-only menu

For lightweight cursor movement, menu text now stays at a fixed centered
position and only the real hand indicates selection. This avoids repainting text
or rerendering the 3D scene when the selected row changes.

Touch-ready progressive hashes measured on the CYD:

```text
after scaled logo        = 1e8bcfbb
after Start Game + hand  = c03215ab
after Options            = b2f6a68d
after Help/About         = 994a049d
after Exit / final       = cbc99461
```

Initial touch-ready frame:

```text
[MAINTOUCHLAYOUT] framebufferFNV=cbc99461 sceneFNV=ffe0995e priorFittedFNV=1afa0223 composeMs=54 shapeData=0x0 mediaTexels=0x0
[MAINTOUCHLAYOUT] End heap8=28704 largest8=17396 deltaFromStart=0 largestDelta=0
```

`composeMs` is diagnostic only.

## Hardware-validated touch zones

```text
                 logical 160x120        physical 320x240
x all rows       28..119                 56..239
Start Game       y=67..78                y=134..157
Options          y=79..90                y=158..181
Help/About       y=91..102               y=182..205
Exit             y=103..114              y=206..229
```

## Lightweight cursor patches

No second 38,400-byte framebuffer is used. Four tiny 13x10 RGB565 background
patches are retained under the possible hand positions:

```text
4 * 13 * 10 * 2 B = 1,040 B
```

A selection change is only:

```text
restore old hand background
-> update selectedIndex
-> draw p.bmp at new row
-> Present
```

Hardware confirms `noSceneRerender=yes` and `noSDRead=yes` for selection moves.

## Hardware-validated selection hashes

```text
selected Start Game = cbc99461
selected Options    = 961109a7
selected Help/About = e4eadfbb
selected Exit       = 5ff2a5cd
```

Returning from Help/About to Options reproduced `961109a7` exactly, proving the
13x10 background restoration is bit-identical. Returning to Start Game similarly
reproduced `cbc99461`.

Example hardware log:

```text
[MENUTOUCH] SELECT 2->1 text="Options   " framebufferFNV=961109a7 previousKnown=961109a7 heap8=28704->28704 largest8=17396->17396
```

## Double-tap confirmation status

The second released tap on the same row is hardware validated.

Examples:

```text
[MENUTOUCH] GATE tap=7 CONFIRM-PASS item=1
[MENUTOUCH] CONFIRM item=1 text="Options   " count=2 framebufferFNV=961109a7 action=deferred

[MENUTOUCH] GATE tap=11 CONFIRM-PASS item=0
[MENUTOUCH] CONFIRM item=0 text="Start Game" count=3 framebufferFNV=cbc99461 action=deferred

[MENUTOUCH] GATE tap=15 CONFIRM-PASS item=3
[MENUTOUCH] CONFIRM item=3 text="Exit      " count=5 framebufferFNV=5ff2a5cd action=deferred
```

`action=deferred` means exactly that: the double tap worked, but no real menu
action was executed. This branch validates input semantics and cursor rendering,
not action dispatch.

## Memory boundary for touch

Observed during hardware selection changes:

```text
heap8    = 28704 -> 28704
largest8 = 17396 -> 17396
```

No per-tap heap allocation is introduced. The 1,040-byte cursor background store
is static bounded state.

## Color / contrast observation

The geometry and touch selection are visually successful, but comparison against
a J2ME reference screenshot still shows the CYD result looking flatter and less
saturated / lower contrast.

This remains a separate open visual issue. Do not tweak the palette blindly.
Investigate it separately with a controlled test that distinguishes among:

- background/composition contrast;
- palette/RGB565 conversion;
- SDL texture modulation/blitting;
- physical TFT appearance.

## Current safe boundary

Validated:

- real `menu.bsp` scene and BSP traversal
- native projected walls/sprites
- bounded wall/sprite resource loading and LRU caches
- deterministic native scene `ffe0995e`
- original `MENU_MAIN` model and real assets
- clean TFT ownership with Serial-only diagnostics
- faithful original menu reference `86c38260`
- fitted 160x120 menu geometry
- calibrated physical touch hit-testing
- real `MenuSystem.selectedIndex` changes
- independently movable real `p.bmp` hand
- bit-identical small cursor-background restoration
- second released tap recognized as semantic `CONFIRM`
- exact allocator restoration during touch movement

Still intentionally out of scope:

- execution of `MenuSystem_select()` from touch confirmation
- real Options / Help / Exit transitions
- Start Game / gameplay loader activation
- original monolithic bitshape/texel loaders
- textured floor/ceiling planes
- persistent caches in the normal multi-frame runtime
- active normal multi-frame `ST_MENU` state machine
- final color/contrast correction
- normal gameplay loop / gameplay control scheme
- audio

## Recommended next direction

The touch frontend itself is validated. The next menu increment should connect
**one safe confirmed action** to the real original menu path rather than enabling
all four entries at once.

A good first candidate is `Options`:

```text
confirmed Options
    -> real MenuSystem/Menu transition
    -> render resulting options menu on ESP32
    -> hardware validate
```

Keep `Start Game` deferred until the gameplay loader is ready, and keep the
color/contrast investigation separate from input/action work.

## Porting workflow

1. create one branch from the latest hardware-validated `main`
2. implement one small measurable objective
3. build/flash/test on the real CYD
4. fix failures on the same branch
5. after hardware success, update every relevant `.md` on that branch
6. only when code + documentation agree is the branch merge-ready
7. merge
8. only then start the next increment
