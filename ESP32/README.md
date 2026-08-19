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

Serial keeps startup/probe/touch diagnostics. The TFT remains game output only.

## Native graphics architecture

The original map-wide graphics pools do not fit the no-PSRAM target.
The ESP32 path uses bounded frames and measured LRU caches:

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

ESP32 fitted geometry:

```text
logical screen = 160x120
logo source    = 108x74
logo target    = 90x62 at 35,2

Start Game y=67
Options    y=79
Help/About y=91
Exit       y=103
```

Deterministic references:

```text
faithful original MENU_MAIN = 86c38260
fitted PR #28 MENU_MAIN     = 1afa0223
layout geometry FNV         = 47b3656e
```

## Touch input

The XPT2046 drives the real menu selection:

```text
XPT2046
  -> PlatformInput
  -> calibrated physical 320x240 point
  -> one semantic tap per press/release cycle
  -> logical 160x120 hit-test
  -> real MenuSystem.selectedIndex / action
```

A new semantic tap is blocked until release has remained stable for 50 ms.

Current `MENU_MAIN` UX:

```text
first tap on another row -> select / move hand
first tap on current row -> arm
second released tap      -> confirm
```

Selected-frame hashes:

```text
Start Game = cbc99461
Options    = 961109a7
Help/About = e4eadfbb
Exit       = 5ff2a5cd
```

Cursor movement uses four static 13x10 RGB565 background patches:

```text
4 * 13 * 10 * 2 B = 1,040 B
```

No second 38,400-byte framebuffer is allocated.

## Real Options action

Confirmed `Options` executes the original menu path:

```text
MENU_MAIN
   -> double tap Options
   -> MenuSystem_select()
   -> MENU_MAIN_OPTIONS
```

The real resulting model is:

```text
Back
Video
Input
Sound
```

Hardware signatures:

```text
MENU_MAIN selected Options = 961109a7
Options model FNV          = e1ef01f7
Options framebuffer        = 6058d47d
```

The Options screen is painted through a bounded ESP32 path rather than the
legacy pre-game `MenuSystem_paint()` path.

## Real Options -> Back round trip

`Back` is now the first active action inside `MENU_MAIN_OPTIONS`.

UX:

```text
first Back tap  -> ARM
release
second Back tap -> real MenuSystem_back()
```

The original menu model returns correctly to:

```text
menu          = MENU_MAIN
selectedIndex = 0
numItems      = 4
state         = ST_MENU
```

The shared framebuffer then reconstructs the native menu scene and touch-ready
main menu.

Hardware-validated deterministic chain:

```text
Options framebuffer      = 6058d47d
Back walls               = a6d87c4a
Back walls + sprites     = ffe0995e
Back MENU_MAIN           = cbc99461
MENU_MAIN touch re-armed = yes
```

Final marker:

```text
[OPTIONBACK] End framebufferFNV=cbc99461 expected=cbc99461 menu=1 selected=0 touchActive=1 shapeData=0x0 mediaTexels=0x0
```

Final measured baseline for this build:

```text
heap8    = 28680
largest8 = 17396
```

## Grayscale re-entry rule

A hardware failure during the first Back implementation exposed that
`Render_setGrayPalettes()` is destructive and not idempotent.

Calling it twice changed the otherwise identical wall frame from:

```text
a6d87c4a  expected
```

to:

```text
b6f86faa  double-gray failure
```

The ESP32 re-entry bridge now detects the precise `Options -> MENU_MAIN`
reconstruction and skips only the second grayscale conversion:

```text
[MENUWALL] REENTRY detected from Options framebuffer=6058d47d; preserving already-gray palette
[MENUWALL] REENTRY grayscale already applied; skipping destructive second Render_setGrayPalettes pass
```

Normal boot still calls the real grayscale conversion.

## Why Back currently looks like a reboot

It is **not an actual reboot** and it does not rerun the full startup chain.

The initial Serial blocks such as:

```text
PRERENDER
RENDERSTART
MAPPINGS
MENUBSP
MAPSTRUCT
ASSETPAK
```

belong to the real boot only.

But the current Back implementation deliberately reuses the already validated
scene probes to reconstruct the shared framebuffer:

```text
MENUWALL
    -> ~1355 ms measured
MENUSPRITE
    -> ~1039 ms measured
MENU_MAIN overlay
```

That makes Back take roughly 2.5 seconds and produces a huge amount of diagnostic
logging. It therefore **feels like a mini reboot**, even though startup/resources
are not being rebuilt.

This is accepted only as a proof/recovery implementation for the current
round-trip increment. It is **not** the intended final menu-navigation path.

## Recommended next optimization

Before enabling `Video`, `Input` or `Sound`, make menu navigation fast.

The deterministic `ffe0995e` menu scene is static, so the next useful experiment
is to measure an exact compact representation that can restore it without
rerunning walls + sprites.

Constraints:

```text
full second RGB565 framebuffer = 38,400 B  -> impossible / undesirable
largest free 8-bit block       ~= 17 KB
```

Measure lossless bounded candidates first (for example indexed or RLE storage)
before choosing an implementation. Keep the full native rerender as a regression
or recovery path.

## Linker-wrapper caution

Do not blindly use GNU `--wrap` around callbacks whose address is taken in the
same translation unit. The existing menu callback gate relies on pointer identity.

A narrow wrapper is acceptable at a clear external boundary when it remains
transparent during normal operation; the grayscale re-entry guard is one such
case.

## Legacy header include rule

Several original engine headers assume SDL types are already visible. ESP32 C
files should include SDL first:

```c
#include <SDL.h>
#include "DoomRPG.h"
#include "Render.h"
```

## Color / contrast observation

The main-menu geometry and touch interaction are correct, but comparison against
a J2ME reference still makes the CYD result look flatter / less saturated and
lower contrast.

Do not tweak the palette blindly. Keep this as a separate measured investigation.
The double-grayscale re-entry bug was a deterministic rerender issue and does not
explain the original J2ME-vs-CYD visual difference.

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
- real selection and hand movement
- released double-tap confirmation
- real Options action through `MenuSystem_select()`
- real `MENU_MAIN_OPTIONS` model
- real Back action through `MenuSystem_back()`
- bit-identical return to `MENU_MAIN`
- main-menu touch re-armed after Back
- exact allocator recovery

Still deferred:

- fast menu-scene restoration
- Video/Input/Sound actions
- Help/About and Exit real actions
- Start Game / gameplay loader
- active normal multi-frame game loop
- gameplay controls
- final color correction
- audio

## Porting workflow

1. create one branch from the latest hardware-validated `main`
2. implement one small measurable objective
3. build/flash/test on the real CYD
4. fix failures on the same branch
5. after hardware PASS update every relevant `.md` on that branch
6. only when code + documentation agree is the branch merge-ready
7. merge
8. only then start the next increment
