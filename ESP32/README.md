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

This is the default environment and the one to use for everyday development and
hardware testing.

```bash
cd ESP32
pio run -t upload
pio device monitor
```

Equivalent explicit form:

```bash
pio run -e esp32-cyd -t upload
pio device monitor -e esp32-cyd
```

Normal boot runs only the real engine initialization required to reach the current
interactive menu. Historical graphics/resource proof probes are skipped.

### `esp32-cyd-bringup` — full diagnostic laboratory

This preserves the former historical bring-up behaviour and enables the complete
validation suite:

```bash
cd ESP32
pio run -e esp32-cyd-bringup -t upload
pio device monitor -e esp32-cyd-bringup
```

It extends `esp32-cyd` with:

```text
DOOMRPG_ESP32_BRINGUP_PROBES=1
```

Use it when re-validating asset-pack access, memory budgets, bitshapes, sprite/wall
consumers, projected walls, native menu scene rendering, LRU cache contracts or
other low-level graphics invariants.

The bring-up profile is **not** obsolete code. It is the permanent diagnostic mode
for evidence-heavy tests that should not slow down or spam normal boot.

## Normal boot sequence

Current normal startup is:

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

The following historical proof/demo passes are **not** executed by normal boot:

```text
BSP structure planning
RESOURCEPLAN
ASSETPAK full proof
BITSHAPE walk
SPRITETEX proof
SPRITERENDER demo
WALLRENDER demo
PROJWALL synthetic projection
MENUWALL benchmark
MENUSPRITE benchmark
```

They remain compiled and available in `esp32-cyd-bringup`.

Normal-mode hardware marker:

```text
[BOOT] bringupProbes=off; skipping memory-plan/asset/sprite/wall/projected/menu-scene validation suite
```

Expected final menu marker:

```text
[MAINOPAQUE] ... finalFNV=58a11171 ...
[BOOT] NORMAL READY mainMenuFNV=58a11171 ... shapeData=0x0 mediaTexels=0x0
```

## Real menu runtime boundary

Normal boot still uses the real menu map loader. It does **not** fabricate a
minimal menu state.

The current ESP32 wrapper stops the original structural load on the seventh
`DoomCanvas_updateLoadingBar()` callback, immediately before the legacy
`Render_loadBitShapes()` / `Render_loadTexels()` phase.

At that boundary the hardware-validated real runtime state is:

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

Do not reintroduce the original map-wide pools as a shortcut.

## Suppressed historical loading-bar flicker

Original `DoomCanvas_updateLoadingBar()` clears the framebuffer, draws five small
progress rectangles and flushes them to the TFT.

That was useful in the original game but produced a brief black/white five-box
flash during the cleaned ESP32 normal startup.

Current behaviour:

```text
esp32-cyd
    -> count loader callbacks
    -> keep exact structural stop boundary
    -> suppress intermediate loading-bar drawing / TFT presents

esp32-cyd-bringup
    -> preserve historical loading-bar behaviour
```

The real CYD has validated that normal boot now goes directly to the main menu
without that transient loading screen.

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

Stable native graphics recovery references, normally exercised only by
`esp32-cyd-bringup`:

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

The active presentation is intentionally opaque black:

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

Historical scene-backed selected hashes remain recovery references only:

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

## Back touch tolerance

The visible Back row stays at logical y=67. The touch-only hitbox is slightly
larger to absorb measured XPT2046 jitter:

```text
logical  x=15..119 y=64..78
physical x=30..239 y=128..157
```

`Video` begins at logical y=79, so the tolerance does not overlap the next row.

## Bring-up visual hitbox diagnostics

Touch-zone visualization should live in the permanent `esp32-cyd-bringup` profile,
not in disposable production code.

Planned diagnostic overlay:

```text
red outline = actual hitbox accepted by firmware
marker/cross = most recent touch point
Serial       = raw + physical + logical coordinates + detected item
```

The overlay should consume the **same hitbox data** as the input code. Do not copy
coordinates into a second independent diagnostic table.

Expected calibration workflow:

```text
1. flash esp32-cyd-bringup with hitbox overlay
2. photograph the real CYD
3. compare red rectangles with visible menu controls
4. adjust all zones together
5. repeat until aligned
6. keep final constants for normal firmware
```

This diagnostic is intended to remain useful for future submenus and gameplay
controls, so it should be reusable rather than deleted after the first calibration.

Normal `esp32-cyd` must never show these diagnostic rectangles.

## Current memory baseline

Clean normal boot hardware baseline:

```text
heap8    = 29064
largest8 = 17396
```

Normal menu navigation keeps:

```text
shapeData   = NULL
mediaTexels = NULL
wall cache  = inactive
sprite cache= inactive
```

## Screen / logging policy

Normal environment:

```text
TFT    -> game/menu framebuffer only
Serial -> concise startup and runtime/touch diagnostics
```

Bring-up environment:

```text
TFT    -> framebuffer plus explicitly enabled bring-up visuals
Serial -> full validation/benchmark output
```

Normal firmware still uses:

```text
-D DOOMRPG_ESP32_SCREEN_DIAGNOSTICS=0
```

## Build note

Use a clean build after linker-wrapper, generated-source or significant
`platformio.ini` changes:

```bash
cd ESP32
pio run -t clean
pio run -t upload
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

Hardware validated in normal mode:

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
- `shapeData == NULL`
- `mediaTexels == NULL`

Available on demand through `esp32-cyd-bringup`:

- structure/memory planning diagnostics
- native asset-pack validation
- bitshape and sprite-texel proof passes
- sprite/wall render consumers
- projected wall regression
- full native menu walls/sprites scene
- wall/sprite LRU regression metrics

Still deferred:

- Video/Input/Sound actions
- Help/About and Exit real actions
- Start Game / gameplay loader
- active normal multi-frame game loop
- gameplay controls
- visual hitbox overlay implementation
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
