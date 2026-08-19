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

### Legacy header include rule

Several original engine headers are not self-contained and assume SDL types are
already visible. In ESP32-specific C files, include SDL before headers such as
`DoomRPG.h` or `Render.h`:

```c
#include <SDL.h>
#include "DoomRPG.h"
#include "Render.h"
```

This rule was re-confirmed by the real-main-menu overlay increment after a build
failure on `SDL_Texture`, `SDL_RWops`, `SDL_Rect` and `Uint8`.

## SD card contents

During migration the SD card contains:

```text
/DoomRPG.zip
/DoomRPG-ESP32.pak
```

`DoomRPG.zip` remains only for legacy engine paths not yet migrated.
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
and seekable; the entire index is not kept resident in RAM.

## Why the native graphics path exists

The original graphics architecture cannot fit the no-PSRAM target. Measured menu
legacy sizes include:

```text
wall-only legacy mediaTexels = 172,032 B
selected sprite texels       = 143,990 B
expanded shapeData           = 55,676 B
```

The ESP32 path instead uses bounded frames and small measured caches:

```text
largest validated sprite frame = 2,112 B
one packed wall texture         = 2,048 B
3-slot wall cache payload       = 6,144 B
3-slot sprite cache peak        = 6,038 B on the reference frame
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
                         /         \
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

The deterministic menu camera is:

```text
spawnIndex    = 460
spawn tile    = 12,14
world X/Y     = 800,928
direction     = 0
camera Z      = 36
animFrameTime = 0
```

## Hardware-validated wall LRU

On the exact real wall request stream:

```text
logical requests         = 25
hits                     = 14
misses                   = 11
evictions                 = 8
logical cache payload    = 6,144 B
allocator cache cost     = 6,192 B
physical wall loads      = 11
wall bytes read          = 22,528 B
framebuffer              = a6d87c4a
```

## Hardware-validated sprite LRU

Exact logical request stream:

```text
562, 406, 410, 598, 172, 578, 578, 426, 410, 578, 102
```

Three-slot result:

```text
logical requests        = 11
hits                    = 2
misses                  = 9
evictions                = 6
peak logical payload    = 6,038 B
final resident payload  = 3,709 B
final allocator cost    = 3,760 B
physical sprite loads   = 9
sprite bytes read       = 10,734 B
wall-backed loads       = 2
GFXRM total             = 14,830 B
framebuffer             = ffe0995e
```

Caches are opt-in and completely torn down at the deterministic probe boundary.

## Real `MENU_MAIN` overlay

The project now composes the **original Doom RPG main menu model** over the
hardware-validated native menu scene.

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

The original menu assets were already loaded by `MenuSystem_startup()`:

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

### Yes: the menu is an overlay

The original `MenuSystem_paint()` renders the menu scene, then overlays the logo,
selection hand and text. The ESP32 path preserves that model but uses the already
validated native scene rather than calling the heavy legacy `Render_render()`
again:

```text
native real menu scene
       ffe0995e
           |
           +--> j.bmp logo
           +--> p.bmp hand on selected item
           +--> Doom RPG bitmap font
           |
           v
       86c38260
```

The ESP32 SDL shim's `SDL_RenderCopy()` blits directly into
`PlatformVideo_framebuffer()`, so UI composition uses the **same** 160x120 RGB565
buffer. There is no second UI framebuffer and no separate 2D rendering engine.

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

Hardware log summary:

```text
[MAINMENU] Begin sceneFNV=ffe0995e expected=ffe0995e heap8=29852 largest8=21492 shapeData=0x0 mediaTexels=0x0
[MAINMENU] Assets logo=108x74 transparent=1 hand=13x10 transparent=1 font=144x72 largeStatus=0
[MAINMENU] Model menu=1 type=4 items=4 selected=0 scroll=0 maxItems=10 modelFNV=bbc2149b imgBG=logo
[MAINMENU] framebufferFNV=86c38260 sceneFNV=ffe0995e changed=yes composeMs=21 shapeData=0x0 mediaTexels=0x0
[MAINMENU] End heap8=29852 largest8=21492 deltaFromStart=0 largestDelta=0
[MAINMENU] READY original Menu model + logo + hand + font composed without legacy Render_render
```

`composeMs=21` is diagnostic only.

### Memory behaviour

The menu assets are already resident from startup. UI composition changes no heap
state:

```text
before: heap8=29852 largest8=21492
after:  heap8=29852 largest8=21492
```

### Current 160x120 presentation limitation

The original layout is **faithful but cramped** on the current logical height.
`MENUTYPE_MAIN` starts item drawing at `y=80` and advances 12 pixels per row. Four
items therefore extend toward y=128, while the logo itself is 74 pixels tall.

This is not a failed renderer or scale bug. It is a layout adaptation problem:

```text
logical screen height = 120
menu start y          = 80
row height            = 12
four rows extend to   ~128
```

Keep `86c38260` as the faithful original-layout reference. Adaptation for 160x120
should happen in a later dedicated increment so rendering integration and UI
redesign are not mixed.

### Bring-up diagnostic text

The current firmware still writes bring-up status text such as
`Sprite: MENU BSP OK` on the TFT. That text is **not part of the MENU_MAIN
framebuffer hash** and should eventually be removed/disabled once the normal menu
loop becomes the primary presentation path.

## Linker integration for the menu probe

The real scene probe remains unchanged. The overlay is chained after its success
with:

```text
-Wl,--wrap=DoomRPG_probeNativeMenuSpriteFrame
```

Conceptually:

```text
__real_DoomRPG_probeNativeMenuSpriteFrame()
    -> proves ffe0995e
    -> releases wall/sprite caches

DoomRPG_probeNativeMainMenuOverlay()
    -> builds original MENU_MAIN model
    -> composes logo/hand/font
    -> proves 86c38260
```

## Current safe boundary

Validated:

- real menu BSP structural data
- native wall and sprite resource access
- native projected walls and sprites
- measured three-slot wall and sprite LRU caches
- deterministic scene `ffe0995e`
- original `MENU_MAIN` model
- original logo, hand and bitmap font composition
- deterministic menu overlay `86c38260`
- exact allocator restoration

Still intentionally out of scope:

- original monolithic bitshape/texel loaders
- textured floor/ceiling planes
- persistent caches in the normal multi-frame runtime
- active normal `ST_MENU` loop
- touch navigation of real menu items
- 160x120 menu layout adaptation
- removal of bring-up TFT diagnostics
- normal gameplay loop
- audio

## Recommended next direction

After this overlay increment merges, continue with **menu runtime integration**:

1. make the real `MENU_MAIN` composition the active `ST_MENU` presentation in the
   normal loop without reintroducing legacy `Render_render()`;
2. route existing touch/input actions through the real `MenuSystem_moveDir()` /
   `MenuSystem_select()` path and prove the hand moves between menu items;
3. adapt the main-menu layout for 160x120 only after interaction is stable;
4. remove the bring-up TFT diagnostics in a separate cleanup increment.

Keep these two regression boundaries while doing so:

```text
pre-overlay native scene = ffe0995e
faithful MENU_MAIN        = 86c38260
```

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
