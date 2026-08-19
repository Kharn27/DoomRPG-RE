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

## PlatformIO environments

The project intentionally exposes **two** PlatformIO environments.

### `esp32-cyd` — normal firmware

This is the default everyday environment:

```bash
cd ESP32
pio run -e esp32-cyd -t upload
pio device monitor -e esp32-cyd
```

Normal boot runs only the real engine initialization required to reach the
interactive menu. Historical graphics/resource proof probes are skipped.

Touch-hitbox diagnostics are also excluded at preprocessing time. The normal
firmware does not contain overlay state arrays, overlay draw calls, overlay C
bridge functions or menu overlay-registration calls.

### `esp32-cyd-bringup` — full diagnostic laboratory

```bash
cd ESP32
pio run -e esp32-cyd-bringup -t upload
pio device monitor -e esp32-cyd-bringup
```

This environment extends `esp32-cyd` with:

```text
DOOMRPG_ESP32_BRINGUP_PROBES=1
DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY=1
```

It preserves the full historical validation suite and adds physical touch-zone
visualization. Use it when re-validating asset-pack access, memory budgets,
bitshapes, sprite/wall consumers, projected walls, native menu scene rendering,
LRU cache contracts or touch calibration.

The bring-up profile is intentionally verbose and is not the product boot.

## Normal boot sequence

```text
Platform video / SD / ZIP
    -> engine core + layout
    -> ParticleSystem / MenuSystem / EntityDef startup
    -> Render_startup
    -> config + mappings
    -> Render_beginLoadMap(MAP_MENU)
    -> real Render_beginLoadMapData structural phase
    -> stop immediately before legacy bitshape/texel loaders
    -> direct opaque MENU_MAIN
    -> touch armed
    -> READY
```

Historical proof/demo passes are not executed by normal boot. They remain
available in `esp32-cyd-bringup`.

Expected normal final menu marker:

```text
[MAINOPAQUE] ... finalFNV=58a11171 ...
[BOOT] NORMAL READY mainMenuFNV=58a11171 ... shapeData=0x0 mediaTexels=0x0
```

## Real menu runtime boundary

Normal boot still uses the real menu map loader. The wrapper stops the original
structural load on the seventh `DoomCanvas_updateLoadingBar()` callback,
immediately before the legacy `Render_loadBitShapes()` / `Render_loadTexels()`
phase.

Hardware-validated state at that boundary:

```text
nodes          = 53
lines          = 120
mapSprites     = 44
runtimeSprites = 68
events         = 15
mapTextures    = 84
mapSpriteRefs  = 284
planeTextures  = 11
persistent     = 14092 B
```

Strong graphics-memory invariant:

```text
shapeData   = NULL
mediaTexels = NULL
```

Normal mode suppresses the historical five-box loading-bar TFT flicker. Bring-up
keeps the old behaviour for diagnostic fidelity.

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

Stable bring-up recovery references:

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
logo target    = 90x62 at 35,2

Start Game y=67
Options    y=79
Help/About y=91
Exit       y=103

layout FNV = 47b3656e
model FNV  = bbc2149b
```

The active presentation is opaque black:

```text
black framebuffer
+ scaled real j.bmp logo
+ real p.bmp cursor
+ real Doom bitmap font
```

Hardware hashes:

```text
black + logo        = 0ac1f9c6
Start Game selected = 58a11171
Options selected    = 0cf107b1
Help/About selected = 9db82b71
Exit selected       = bdd775f9
```

## Touch input

The XPT2046 drives real menu selection:

```text
XPT2046
  -> PlatformInput
  -> calibrated physical 320x240 point
  -> one semantic tap per press/release cycle
  -> 50 ms stable-release rearm
  -> logical 160x120 hit-test
  -> real MenuSystem.selectedIndex / action
```

Current UX:

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

## Bring-up touch-hitbox overlay

`esp32-cyd-bringup` can visualize the exact touch geometry on the physical TFT:

```text
red rectangle = accepted logical hitbox scaled exactly 2x
cyan cross    = most recent calibrated semantic touch
small yellow ring = exact touch centre
Serial        = raw / physical / logical touch coordinates
```

The overlay is drawn **after** framebuffer presentation with TFT_eSPI. It never
modifies the 160x120 framebuffer, so framebuffer FNVs remain game-only regression
contracts.

The latest touch marker is captured before the main-menu double-tap gate. A first
ARM tap therefore remains visible even if the gate deliberately consumes it.

Normal `esp32-cyd` does not compile this path.

### Current MENU_MAIN zones

Real-CYD photographs showed the four rectangles aligned well with the visible
controls, so no adjustment was required:

```text
Start Game logical x=28..119 y=67..78
Options    logical x=28..119 y=79..90
Help/About logical x=28..119 y=91..102
Exit       logical x=28..119 y=103..114
```

Physical coordinates are exact 2x equivalents.

### Current Options zones

Visible rows:

```text
Back  y=67..78
Video y=79..90
Input y=91..102
Sound y=103..114
```

The first overlay photograph showed the Options geometry aligned, but Back looked
slightly taller because it still had the earlier `y=64..78` touch tolerance.

A previously missed real tap had landed at logical `y=65`, so the final proposed
Back zone keeps that proven point but removes one unnecessary upper pixel:

```text
Back logical  x=15..119 y=65..78
Back physical x=30..239 y=130..157
```

`Video`, `Input` and `Sound` remain deferred actions, but their diagnostic zones
are displayed in bring-up for calibration.

## Real Options action and fast Back

Confirmed Options executes the original menu path:

```text
MENU_MAIN / Options selected 0cf107b1
    -> MenuSystem_select()
    -> MENU_MAIN_OPTIONS
```

Real Options model:

```text
Back
Video
Input
Sound
```

Hardware references:

```text
Options model FNV   = e1ef01f7
Options framebuffer = 6058d47d
```

Back executes the real hierarchy transition:

```text
MENU_MAIN_OPTIONS
    -> MenuSystem_back()
    -> real MENU_MAIN model
    -> direct opaque repaint
    -> 58a11171
    -> touch re-armed
```

No native scene reconstruction is needed for normal Back navigation.
Previously measured complete return including TFT present:

```text
~138 ms
```

## Original debug/developer menu

The reverse-engineered original engine already contains hidden development menus:

```text
MENU_DEBUG
MENU_DEBUG_CHEATS
MENU_DEBUG_MAPS
MENU_DEBUG_STATS
MENU_DEVELOPER
```

They include cheats, maps, statistics, benchmark and developer facilities.

No original touch-hitbox visualization was found in the menu/input path. Original
menu selection is directional (`selectedIndex` / `MenuSystem_moveDir()`), so the
CYD hitbox overlay remains an ESP32 touch adaptation rather than a duplicate of an
existing original feature.

The original debug/developer menus remain useful candidates for future ESP32
diagnostics instead of growing unrelated parallel debug systems.

## Current memory baseline

Last validated clean normal boot baseline:

```text
heap8    = 29064
largest8 = 17396
```

Normal menu navigation keeps:

```text
shapeData    = NULL
mediaTexels  = NULL
wall cache   = inactive
sprite cache = inactive
```

## Final branch validation before merge

The current hitbox increment is documented before its final dual-profile test so
that a successful hardware pass can be merged immediately.

Test normal:

```bash
cd ESP32
pio run -e esp32-cyd -t clean
pio run -e esp32-cyd -t upload
pio device monitor -e esp32-cyd
```

Expected:

```text
no [HITBOX] logs
no red rectangles
MENU_MAIN = 58a11171
Options selected = 0cf107b1
Options screen = 6058d47d
Back -> 58a11171
shapeData = 0x0
mediaTexels = 0x0
```

Then test bring-up:

```bash
pio run -e esp32-cyd-bringup -t clean
pio run -e esp32-cyd-bringup -t upload
pio device monitor -e esp32-cyd-bringup
```

Expected:

```text
[HITBOX] Physical overlay enabled
4 MAIN rectangles
4 Options rectangles
last-touch marker
Back y=65..78
unchanged framebuffer hashes
```

If both profiles pass, this branch is merge-ready without another documentation
round.

## Build note

Use a clean build after PlatformIO profile or compile-time diagnostic changes:

```bash
cd ESP32
pio run -e esp32-cyd -t clean
pio run -e esp32-cyd -t upload
```

## Legacy header include rule

Several original engine headers assume SDL types are already visible. ESP32 C
files should include SDL first:

```c
#include <SDL.h>
#include "DoomRPG.h"
#include "Render.h"
```

## Current safe boundary

Hardware validated before the final branch retest:

- real core/layout/pre-render startup
- real Render startup
- real config + mappings
- real menu runtime structures through the pre-bitshape boundary
- opaque deterministic `MENU_MAIN`
- calibrated touch / released double-tap semantics
- real Options action through `MenuSystem_select()`
- real `MENU_MAIN_OPTIONS`
- real Back through `MenuSystem_back()`
- fast opaque Back repaint
- no historical loading-bar flicker
- initial bring-up photos show correctly aligned MAIN/Options hitboxes
- `shapeData == NULL`
- `mediaTexels == NULL`

Implemented on the current branch and awaiting final normal + bring-up confirmation:

- permanent bring-up-only physical hitbox overlay
- last calibrated touch marker
- strict compile-time exclusion from normal firmware
- Back tolerance tightened to logical `y=65..78`

Still deferred:

- Video/Input/Sound actions
- Help/About and Exit real actions
- Start Game / gameplay loader
- active normal multi-frame game loop
- gameplay controls
- final color/contrast investigation
- audio
