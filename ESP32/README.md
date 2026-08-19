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

Use a clean build after linker-wrapper, generated-source or `platformio.ini`
changes:

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

## Native graphics architecture

The original map-wide graphics pools do not fit the no-PSRAM target. The ESP32
path uses bounded frames and measured LRU caches:

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

Stable native graphics references:

```text
sprite 172 texel FNV         = 0c0a7acd
wall 112 texel FNV           = 92d40704
synthetic projected wall FNV = ad191f54
real walls framebuffer       = a6d87c4a
viewSprites list FNV         = 962cd657
sprite request FNV           = 4457ac94
walls + sprites framebuffer  = ffe0995e
```

## Main menu architecture

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

layout FNV = 47b3656e
model FNV  = bbc2149b
```

### Opaque presentation

The active `MENU_MAIN` presentation is now intentionally opaque black.

At boot the existing native graphics probes may still validate the full menu scene
`ffe0995e`, but the user-facing menu then clears the shared framebuffer and paints
only the bounded UI:

```text
black framebuffer
+ scaled real j.bmp logo
+ real p.bmp cursor
+ real Doom bitmap font
```

This removes the static 3D scene as a dependency of menu navigation and is closer
to the observed J2ME presentation.

Known hardware hashes:

```text
black + logo        = 0ac1f9c6
Start Game selected = 58a11171
Options selected    = 0cf107b1
Help/About selected = 9db82b71
Exit selected       = bdd775f9
```

Historical scene-backed selected hashes are retained only as recovery references:

```text
Start Game old = cbc99461
Options old    = 961109a7
Help old       = e4eadfbb
Exit old       = 5ff2a5cd
```

## Touch input

The XPT2046 drives the real menu selection:

```text
XPT2046
  -> PlatformInput
  -> calibrated physical 320x240 point
  -> one semantic tap per press/release cycle
  -> 50 ms stable-release rearm
  -> logical 160x120 hit-test
  -> real MenuSystem.selectedIndex / action
```

Current `MENU_MAIN` UX:

```text
first tap on another row -> select / move hand
first tap on current row -> arm
second released tap      -> confirm
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
   -> selected framebuffer 0cf107b1
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
Options model FNV   = e1ef01f7
Options framebuffer = 6058d47d
```

The Options screen uses the same bounded black-background presentation style and
therefore does not need `Render_render()` or a map reload.

## Fast real Options -> Back

`Back` executes the real hierarchy operation:

```text
MenuSystem_back()
```

The model returns to real `MENU_MAIN`, then the same opaque painter used at boot is
called directly. The heavy native scene is **not** reconstructed.

Current path:

```text
MENU_MAIN_OPTIONS / 6058d47d
    -> double tap Back
    -> MenuSystem_back()
    -> direct opaque MENU_MAIN repaint
    -> 58a11171
    -> touch re-armed
```

Hardware measurement:

```text
[OPTIONBACK] FAST End framebufferFNV=58a11171 runtimeFNV=58a11171 menu=1 selected=0 touchActive=1 repaintMs=138 shapeData=0x0 mediaTexels=0x0
```

The complete Back transition + UI paint + TFT present measured about **138 ms**,
versus roughly 2.5 seconds for the former `MENUWALL + MENUSPRITE` replay.

After Back, selecting Options again reproduces `0cf107b1`, proving the navigation
cycle is re-entrant.

## Back touch tolerance

The visible Back row stays at logical y=67. Hardware jitter showed that a second
tap could land at y=65 and miss the original strict row hitbox.

The touch-only Back hitbox is therefore:

```text
logical  x=15..119 y=64..78
physical x=30..239 y=128..157
```

`Video` begins at logical y=79, so the added upper tolerance does not overlap the
next row.

## Retired grayscale re-entry workaround

The previous heavy Back implementation replayed `MENUWALL`, which exposed that
`Render_setGrayPalettes()` is destructive and not idempotent. A temporary wrapper
was needed to avoid a second grayscale conversion.

Fast opaque Back no longer reruns the wall scene, so that workaround has been
removed completely:

```text
native_menu_wall_reentry_bridge.c                removed
--wrap=DoomRPG_probeNativeMenuWallFrame           removed
--wrap=Render_setGrayPalettes                     removed
```

Historical failed wall framebuffer `b6f86faa` remains documented in
`PORTING_STATUS.md` as a recovery lesson.

## Current memory baseline

Validated hardware baseline for the fast opaque menu build:

```text
heap8    = 28688
largest8 = 17396
```

Normal menu navigation keeps:

```text
shapeData   = NULL
mediaTexels = NULL
wall cache  = inactive
sprite cache= inactive
```

## Startup probe noise

The firmware still runs many historical bring-up probes during normal startup:
asset-pack reads, sprite/wall consumers, cache regression passes and scene proof.
They were essential while establishing the current hashes and memory contracts,
but they now create a large amount of Serial output and unnecessary normal-boot
work.

Recommended next cleanup:

```text
normal mode
    -> only required startup
    -> concise logs

diagnostic bring-up mode
    -> full historical probe chain
    -> recovery hashes / memory contracts on demand
```

Keep the probe implementations; gate their execution behind an explicit build or
runtime flag rather than deleting evidence-producing code prematurely.

## Visual touch-zone calibration idea

For future calibration, a disposable diagnostic branch can draw red rectangle
outlines for every logical menu hitbox and mark the last physical/logical touch
point. A single hardware photo then shows systematic offsets across all buttons.
After calibration, remove the overlay and retain only the final hitbox constants.

This is preferable to tuning every row blindly one at a time.

## Screen diagnostics policy

Normal firmware reserves the TFT for game/menu framebuffer output:

```text
-D DOOMRPG_ESP32_SCREEN_DIAGNOSTICS=0
```

Serial diagnostics remain available.

## Legacy header include rule

Several original engine headers assume SDL types are already visible. ESP32 C
files should include SDL first:

```c
#include <SDL.h>
#include "DoomRPG.h"
#include "Render.h"
```

## Current safe boundary

Hardware validated:

- native `menu.bsp` scene and BSP traversal
- native projected walls and sprites
- bounded wall/sprite GFXRM loading and LRU caches
- deterministic scene `ffe0995e`
- real `MENU_MAIN` model/assets
- opaque bounded main-menu presentation
- deterministic cursor selection hashes
- calibrated touch hit-testing
- real selection and hand movement
- released double-tap confirmation
- real Options action through `MenuSystem_select()`
- real `MENU_MAIN_OPTIONS` model/framebuffer
- real Back through `MenuSystem_back()`
- **fast opaque Back with no MENUWALL/MENUSPRITE replay**
- **main-menu touch re-armed after Back**
- exact allocator recovery

Still deferred:

- Video/Input/Sound actions
- Help/About and Exit real actions
- Start Game / gameplay loader
- active normal multi-frame game loop
- gameplay controls
- trimming normal boot probe execution
- visual all-hitbox calibration overlay
- final color/contrast investigation
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
