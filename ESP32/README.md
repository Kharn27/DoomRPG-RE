# Doom RPG ESP32 port

This directory contains the ESP32-specific engine/port for the classic
ESP32-2432S028R Cheap Yellow Display with **no PSRAM**.

DoomRPG-RE is used as a behavioural and data-format reference while rendering,
resource management and UI are progressively rebuilt around the real target
constraints.

For the exact hardware recovery point and regression hashes, see
[`PORTING_STATUS.md`](PORTING_STATUS.md).

## Current target

- ESP32-2432S028R / classic CYD
- ESP32-D0WD-V3, 240 MHz
- 4 MB flash
- no PSRAM
- ILI9341 320x240 landscape
- XPT2046 touch
- internal RGB565 framebuffer: 160x120 = 38,400 B
- exact nearest-neighbour 2x output to 320x240
- microSD-backed resources
- audio disabled during bring-up

## Build / flash

Normal build:

```bash
cd ESP32
pio run -t upload
pio device monitor
```

Use a clean build after linker-wrapper or generated-source changes:

```bash
cd ESP32
pio run -t clean
pio run -t upload
pio device monitor
```

## SD card

Current migration setup expects:

```text
/DoomRPG.zip
/DoomRPG-ESP32.pak
```

`DoomRPG.zip` remains for legacy engine paths not yet migrated.
`DoomRPG-ESP32.pak` is the native random-access resource pack.

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

## Screen diagnostics policy

Normal firmware reserves the TFT for the game framebuffer:

```text
-D DOOMRPG_ESP32_SCREEN_DIAGNOSTICS=0
```

With diagnostics disabled:

- no bring-up text on TFT
- no touch crosshair on TFT
- no touch-triggered SDL test pattern
- Serial startup/probe/`[ALIVE]`/`[TOUCH]` diagnostics remain available

Set the flag to `1` temporarily to restore the old visual diagnostics.

## Native graphics architecture

The original map-wide graphics pools do not fit the no-PSRAM target.
The ESP32 path uses bounded resource frames and measured LRU caches:

```text
                 SD / DoomRPG-ESP32.pak
                          |
                          v
                        GFXRM
                      /       \
            sprite frames     wall frames
                 |                 |
       Sprite LRU cache(3)    Wall LRU cache(3)
                 |                 |
                 v                 v
          projected sprites   projected walls
                  \             /
                   \           /
                    160x120 RGB565
                          |
                          v
                      TFT exact x2
```

Strong runtime invariant:

```text
shapeData   = NULL
mediaTexels = NULL
```

Do not reintroduce those original map-wide pools as a shortcut.

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

Hardware-validated caches:

```text
Wall LRU3
  requests = 25
  hits     = 14
  misses   = 11
  framebuffer = a6d87c4a

Sprite LRU3
  requests = 11
  hits     = 2
  misses   = 9
  peak logical payload = 6038 B
  framebuffer = ffe0995e
```

## Main menu

The real Doom RPG `MENU_MAIN` model is preserved:

```text
Start Game
Options
Help/About
Exit
```

Real resources:

```text
j.bmp -> logo
p.bmp -> hand cursor
DoomCanvas imgFont -> text
```

The original J2ME/BREW-style geometry does not fit four 12-pixel rows inside a
160x120 framebuffer. The ESP32 presentation therefore keeps the font and hand at
native size and scales only the logo.

Hardware-validated geometry:

```text
logical screen = 160x120
logo source    = 108x74
logo target    = 90x62 at 35,2

Start Game y=67
Options    y=79
Help/About y=91
Exit       y=103
```

Historical deterministic references:

```text
faithful original MENU_MAIN = 86c38260
fitted PR #28 MENU_MAIN     = 1afa0223
layout geometry FNV         = 47b3656e
```

## Touch input

The XPT2046 drives the real menu selection on hardware:

```text
XPT2046
  -> PlatformInput
  -> calibrated physical 320x240 point
  -> one semantic tap per press/release cycle
  -> logical 160x120 hit-test
  -> real MenuSystem.selectedIndex
  -> real p.bmp hand cursor
```

A new semantic tap is blocked until the panel has been released continuously for
50 ms. This prevents one held press from becoming an accidental double tap.

Current `MENU_MAIN` UX:

```text
first tap on another row
    -> select row
    -> move real hand

first tap on current row
    -> arm confirmation

second released tap on same row
    -> CONFIRM
```

Touch zones:

```text
                 logical 160x120        physical 320x240
x all rows       28..119                 56..239
Start Game       y=67..78                y=134..157
Options          y=79..90                y=158..181
Help/About       y=91..102               y=182..205
Exit             y=103..114              y=206..229
```

No second framebuffer is used for cursor movement. Four static 13x10 RGB565
background patches are retained under the possible hand locations:

```text
4 * 13 * 10 * 2 B = 1,040 B
```

Hardware-validated selected-frame hashes:

```text
Start Game = cbc99461
Options    = 961109a7
Help/About = e4eadfbb
Exit       = 5ff2a5cd
```

Returning to a previously selected row reproduces the same framebuffer hash,
proving the cursor-background restoration is bit-identical.

## Real Options action

Confirmed `Options` is now the **first real menu action executed from touch**.

Path:

```text
MENU_MAIN
   |
   | double tap Options
   v
real MenuSystem_select()
   |
   v
real MENU_MAIN_OPTIONS model
   |
   v
bounded ESP32 Options paint
```

The real original model after the transition is:

```text
menu          = MENU_MAIN_OPTIONS / 7
type          = 7
oldMenu       = MENU_MAIN / 1
selectedIndex = 0
scrollIndex   = 0

Back
Video
Input
Sound
```

Hardware model signature:

```text
modelFNV = e1ef01f7
```

The original pre-game `MenuSystem_paint()` is intentionally not used yet because
it still calls legacy `Render_render()`. Instead the ESP32 path paints the real
Options model onto a controlled black framebuffer using the real logo, hand and
font.

Hardware hashes:

```text
after logo          = 0ac1f9c6
after Back          = c7258261
after Video         = 4e764e2f
after Input         = 175fa691
after Sound / final = 6058d47d
```

Authoritative final marker:

```text
[MAINOPTIONS] framebufferFNV=6058d47d inputFNV=961109a7 changed=yes shapeData=0x0 mediaTexels=0x0
[MAINOPTIONS] End heap8=28704 largest8=17396 deltaFromStart=0 largestDelta=0
```

The transition performs no legacy BSP rerender, no map reload and no gameplay
load.

### Current Options interaction boundary

The Options screen is intentionally **display-only** after opening in this
increment. The menu touch callback is disabled during the transition.

Therefore these do nothing yet:

```text
Back
Video
Input
Sound
```

That is expected. `Back` is the recommended next action to activate because it is
the smallest safe proof of a full menu round trip.

## Linker-wrapper caution

Do not blindly use GNU `--wrap` around callbacks whose address is taken in the
same translation unit.

A failed implementation of the Options action tried:

```text
--wrap=DoomRPG_esp32MainMenuTouchOnTap
```

The existing tap gate compares callback function pointers. Wrapping the symbol
changed the identity visible to the gate while the callback implementation's own
translation unit still took its local function address directly.

Symptom on hardware:

```text
[MENUTOUCH] GATE READY ...   <- missing
[MENUTOUCH] CONFIRM ... action=deferred
```

The wrapper was removed. The validated implementation dispatches the Options
action explicitly from the existing confirmed-item gate.

## Legacy header include rule

Several original engine headers assume SDL types are already visible. ESP32 C
files that include `DoomRPG.h`, `Render.h`, etc. should include SDL first:

```c
#include <SDL.h>
#include "DoomRPG.h"
#include "Render.h"
```

## Color / contrast observation

The main-menu geometry and touch interaction are correct, but comparison against
a J2ME reference capture still makes the CYD result look flatter / less saturated
and lower contrast.

Do not tweak the palette blindly. Keep this as a separate measured investigation
covering:

- background/composition contrast
- palette / RGB565 conversion
- SDL blitting/modulation
- physical TFT appearance

The new Options presentation uses a controlled black background and may later be
useful as a comparison case.

## Current safe boundary

Hardware validated:

- native `menu.bsp` scene and BSP traversal
- native projected walls and sprites
- bounded wall/sprite GFXRM loading and LRU caches
- deterministic scene `ffe0995e`
- real `MENU_MAIN` model/assets
- clean TFT ownership
- fitted 160x120 menu presentation
- calibrated touch hit-testing
- real `selectedIndex` changes
- real hand cursor movement
- released double-tap confirmation
- **real Options action through `MenuSystem_select()`**
- real `MENU_MAIN_OPTIONS` model
- bounded Options framebuffer `6058d47d`
- exact allocator restoration

Still deferred:

- touch interaction inside Options
- Back round trip to MENU_MAIN
- Video/Input/Sound actions
- Help/About and Exit real actions
- Start Game / gameplay loader
- active normal multi-frame game loop
- gameplay controls
- final color correction
- audio

## Recommended next direction

Make `MENU_MAIN_OPTIONS` touch-aware, starting with **Back only**:

```text
Options visible
    -> tap Back / arm
    -> second released tap
    -> real back transition
    -> bounded MENU_MAIN presentation
    -> MENU_MAIN touch re-armed
```

Do not enable Video/Input/Sound in the same increment.

## Porting workflow

1. create one branch from the latest hardware-validated `main`
2. implement one small measurable objective
3. build/flash/test on the real CYD
4. fix failures on the same branch
5. after hardware PASS update every relevant `.md` on that branch
6. only when code + documentation agree is the branch merge-ready
7. merge
8. only then start the next increment
