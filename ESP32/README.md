# Doom RPG ESP32 port

This directory contains the ESP32-specific port and bring-up work for the classic
ESP32-2432S028R Cheap Yellow Display with **no PSRAM**.

DoomRPG-RE is used as a behavioural/data-format reference while resource
management, rendering and UI composition are progressively rebuilt around the
actual target constraints.

For the authoritative recovery point, exact hardware figures and current branch
state, see [`PORTING_STATUS.md`](PORTING_STATUS.md).

## Current target

- ESP32-2432S028R / classic CYD, no PSRAM
- ESP32-D0WD-V3, 240 MHz, 4 MB flash
- ILI9341 320x240 landscape
- XPT2046 touch
- internal render target: 160x120 RGB565
- gameplay viewport: 160x80 at framebuffer y=20
- exact 2x nearest-neighbour output to 320x240
- microSD-backed resources
- audio disabled during bring-up

## Build and flash

From the repository root:

```bash
cd ESP32
pio run -t upload
pio device monitor
```

A clean build is normally unnecessary. Use it when generated-source or linker
configuration changes make it useful:

```bash
cd ESP32
pio run -t clean
pio run -t upload
```

## Screen diagnostics policy

Normal firmware now reserves the TFT for the game framebuffer.

Default PlatformIO flag:

```text
-D DOOMRPG_ESP32_SCREEN_DIAGNOSTICS=0
```

With `0`:

- the old bring-up screen is not drawn;
- `drawLabel()` does not touch the TFT;
- a touch does not trigger `Esp32Sdl_showTestPattern()`;
- touch crosshairs/coordinates are not drawn on the TFT;
- all startup, probe, `[ALIVE]` and `[TOUCH]` logs remain available on Serial.

To temporarily restore the old hacker/bring-up display for hardware debugging,
change the flag to:

```text
-D DOOMRPG_ESP32_SCREEN_DIAGNOSTICS=1
```

This switch intentionally changes only diagnostic presentation. It does not alter
the Doom framebuffer pipeline.

Hardware-validated normal-mode marker:

```text
[TFT] Screen diagnostics=disabled; game framebuffer owns visible output
```

and after startup:

```text
[READY] Bring-up alive; touch diagnostics are serial-only and TFT is reserved for the game framebuffer.
```

## Touch remains available

Disabling screen diagnostics does **not** disable the XPT2046 reader. Touches are
still reported on Serial and can later drive the real menu input layer.

Hardware examples while the Doom menu remained visually unchanged:

```text
[TOUCH] raw=3465,1745 pressure=2239 screen=129,215
[TOUCH] raw=3261,1396 pressure=2434 screen=97,201
[TOUCH] raw=3393,1180 pressure=2433 screen=78,210
[TOUCH] raw=3617,1510 pressure=2295 screen=108,226
```

This is the intended boundary for future menu touch handling: input acquisition
exists, but current touches have no game action and no visual diagnostic side
effect.

## Legacy header include rule

Several original engine headers are not self-contained and assume SDL types are
already visible. In ESP32-specific C files, include SDL before headers such as
`DoomRPG.h` or `Render.h`:

```c
#include <SDL.h>
#include "DoomRPG.h"
#include "Render.h"
```

This was re-confirmed by the real-main-menu overlay increment after a build
failure on `SDL_Texture`, `SDL_RWops`, `SDL_Rect` and `Uint8`.

## SD card contents

During migration the SD card contains:

```text
/DoomRPG.zip
/DoomRPG-ESP32.pak
```

`DoomRPG.zip` remains only for legacy paths not yet migrated.
`DoomRPG-ESP32.pak` is the native direct-access resource backend.

## Building the native asset pack

```bash
python3 ESP32/tools/build_asset_pack.py \
    /path/to/DoomRPG.zip \
    /path/to/DoomRPG-ESP32.pak
```

Example directly to a mounted SD card:

```bash
python3 ESP32/tools/build_asset_pack.py \
    /media/$USER/DOOMRPG/DoomRPG.zip \
    /media/$USER/DOOMRPG/DoomRPG-ESP32.pak
```

To list records while building:

```bash
python3 ESP32/tools/build_asset_pack.py --list \
    /path/to/DoomRPG.zip \
    /path/to/DoomRPG-ESP32.pak
```

The builder must finish with:

```text
[PACK] self-check: OK
```

Validated pack v2 contains 241 direct-access resources. Payloads are uncompressed
and seekable; the whole index is not kept resident in RAM.

## Why the native graphics path exists

The original graphics architecture cannot fit the no-PSRAM target. Measured menu
legacy sizes include:

```text
wall-only legacy mediaTexels = 172,032 B
selected sprite texels       = 143,990 B
expanded shapeData           = 55,676 B
```

The ESP32 path instead uses bounded frames and measured caches:

```text
largest validated sprite frame = 2,112 B
one packed wall texture         = 2,048 B
3-slot wall cache payload       = 6,144 B
3-slot sprite cache peak        = 6,038 B
```

Strong invariant:

```text
shapeData   = NULL
mediaTexels = NULL
```

Do not reintroduce those map-wide pools as a shortcut.

## Graphics resource architecture

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
              borrowed frame      borrowed frame
                    |                  |
                    v                  v
          native projected       native projected
             sprite spans           wall spans
                    \                  /
                     \                /
                      shared 160x120 RGB565
```

GFXRM owns storage/file-format knowledge. Cache policy remains separate from
loading and rasterization.

## Stable native scene signatures

Useful deterministic regression markers:

```text
sprite 172 texel FNV         = 0c0a7acd
wall 112 texel FNV           = 92d40704
synthetic projected wall FNV = ad191f54
real wall request FNV        = 4db9da28
real walls framebuffer       = a6d87c4a
viewSprites list FNV         = 962cd657
sprite request FNV           = 4457ac94
walls + sprites framebuffer  = ffe0995e
wall LRU                     = 14 hits / 11 misses / 8 evictions
sprite LRU                   = 2 hits / 9 misses / 6 evictions
```

Deterministic menu camera:

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
logical requests      = 25
hits                  = 14
misses                = 11
evictions              = 8
logical cache payload = 6,144 B
allocator cache cost  = 6,192 B
physical wall loads   = 11
wall bytes read       = 22,528 B
framebuffer           = a6d87c4a
```

Sprite LRU3 exact logical request stream:

```text
562, 406, 410, 598, 172, 578, 578, 426, 410, 578, 102
```

Result:

```text
logical requests        = 11
hits                    = 2
misses                  = 9
evictions                = 6
peak logical payload    = 6,038 B
final resident payload  = 3,709 B
physical sprite loads   = 9
sprite bytes read       = 10,734 B
wall-backed loads       = 2
GFXRM total             = 14,830 B
framebuffer             = ffe0995e
```

## Real `MENU_MAIN` overlay

The project composes the **original Doom RPG main menu model** over the native
menu scene.

Original model built by `Menu_initMenu(MENU_MAIN)`:

```text
menu          = 1 / MENU_MAIN
type          = 4 / MENUTYPE_MAIN
selectedIndex = 0
scrollIndex   = 0
items         = 4

Start Game
Options
Help/About
Exit
```

Real menu assets already loaded by `MenuSystem_startup()`:

```text
j.bmp -> logo
p.bmp -> selected hand cursor
q.bmp -> arrows
DoomCanvas imgFont -> menu text
```

Hardware sizes:

```text
logo = 108x74, transparent
hand = 13x10, transparent
font = 144x72
largeStatus = 0
```

### The menu is an overlay

The original behaviour renders the 3D menu scene, then overlays the logo, hand
and text. The ESP32 path preserves that model but uses the already validated
native scene instead of invoking legacy `Render_render()` again:

```text
native real menu scene
       ffe0995e
           |
           +--> j.bmp logo
           +--> p.bmp selected hand
           +--> Doom RPG bitmap font
           |
           v
       86c38260
```

The SDL shim's `SDL_RenderCopy()` blits directly into
`PlatformVideo_framebuffer()`, so UI and 3D share the same 160x120 RGB565 buffer.

### Hardware-validated overlay signatures

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

Current clean-display firmware reproduces the same final hash:

```text
[MAINMENU] framebufferFNV=86c38260 sceneFNV=ffe0995e changed=yes composeMs=21 shapeData=0x0 mediaTexels=0x0
[MAINMENU] End heap8=29872 largest8=21492 deltaFromStart=0 largestDelta=0
```

The 20-byte baseline difference from the earlier overlay firmware is due to this
increment's static diagnostic-control code. No per-frame leak is present.

## Current 160x120 presentation limitation

The original layout is faithful but cramped on the current logical height:

```text
logical screen height = 120
MENU_MAIN first row y  = 80
row step               = 12
four rows extend to    ~128
logo size              = 108x74
```

This is a layout adaptation issue, not a renderer/scale bug.

Keep `86c38260` as the faithful original-layout reference. The next visual
increment should adapt the menu coordinates for 160x120 while preserving the real
model, logo, hand and font.

## Current display/input boundary

Normal mode is now:

```text
Serial
  `--> diagnostics / probes / touch coordinates

TFT
  `--> shared game framebuffer only
          |
          +--> native menu scene
          `--> Doom RPG UI overlay
```

This separation is intentional preparation for real menu interaction.

## Current safe boundary

Validated:

- real menu BSP structural data
- native wall/sprite resource access and projected rendering
- measured wall/sprite LRU caches
- deterministic native scene `ffe0995e`
- original `MENU_MAIN` model
- real logo, hand and bitmap font composition
- deterministic menu overlay `86c38260`
- exact allocator restoration
- clean TFT with diagnostic drawing disabled by default
- serial diagnostics preserved
- touch acquisition remains active without corrupting the display

Still intentionally out of scope:

- original monolithic bitshape/texel loaders
- textured floor/ceiling planes
- persistent caches in the normal multi-frame runtime
- active normal `ST_MENU` loop
- touch selection/validation of real menu items
- 160x120 menu layout adaptation
- normal gameplay loop
- gameplay control scheme
- audio

## Recommended next direction

Do the **160x120 `MENU_MAIN` layout adaptation before touch hit-testing**.

Recommended order:

1. keep `ffe0995e` as the native-scene regression boundary;
2. preserve the real `MENU_MAIN` model and assets;
3. choose an ESP32-specific logo/item placement so all four options fit cleanly;
4. establish a new deterministic hash for the adapted layout;
5. then map physical touch to logical 160x120 item hit zones;
6. first tap on a different item selects it and moves the real hand;
7. second tap on the already selected item validates through the real menu path;
8. defer gameplay controls until the first actual game map is running on hardware.

## Porting workflow

1. create one branch from the latest hardware-validated `main`
2. implement one small measurable objective
3. build/flash/test on the real CYD
4. fix failures on the same branch
5. after hardware success, update every relevant `.md` on that same branch
6. only when code + documentation agree is the branch merge-ready
7. merge
8. only then start the next increment

Documentation is part of the increment, not a separate follow-up task.
