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

Touch-hitbox diagnostics are excluded at preprocessing time. The normal firmware
does not contain overlay state arrays, overlay draw calls, overlay C bridge
functions or menu overlay-registration calls.

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

It preserves the historical validation suite and adds physical touch-zone
visualization. Use it for asset-pack/resource regressions, memory budgets,
bitshapes, sprite/wall consumers, projected walls, LRU cache contracts and touch
calibration.

The bring-up profile is intentionally verbose and is not the product boot.

## Hardware-selected CYD display profile

The real CYD was compared with four exact 160x120 1:1 presentations of the same
logical framebuffer:

```text
A  gamma 1.00 / saturation 1.00
B  gamma 1.30 / saturation 1.00
C  gamma 1.00 / saturation 1.15
D  gamma 1.30 / saturation 1.15
```

The hardware-preferred result was **C**.

Current display profile:

```text
gamma       = 1.00
saturation  = 1.15
resampling  = nearest
```

Nearest-neighbour sampling was already the ESP32 behaviour; the color increment
therefore keeps geometry/scaling unchanged and adds only the saturation profile.

The correction is intentionally applied at the final TFT presentation boundary:

```text
engine / palettes / GFXRM
        -> logical 160x120 RGB565 framebuffer
        -> deterministic FNV hashes
        -> CYD saturation 1.15 output transform
        -> exact nearest-neighbour 2x
        -> ILI9341
```

The logical framebuffer is never modified by the panel tuning. Consequently all
existing engine/menu/render hashes remain valid source-of-truth contracts.

Implementation details:

- RGB565 is expanded to 8-bit channels by bit replication;
- BT.601-style integer luma is preserved;
- only chroma is scaled to 115%;
- neutral grays and black/white remain neutral;
- the transform runs once per logical pixel (19,200 transforms/frame), then the
  corrected pixel is duplicated for exact 2x output;
- no second framebuffer, large lookup table or persistent allocation is added.

Hardware timing after the profile was selected:

```text
normal menu Present            ~= 42.7 ms
bring-up menu Present+overlay   ~= 44.3 ms
old neutral Present             ~= 34.4 ms
```

The extra output cost is accepted for the current milestone and should be kept in
mind when the active gameplay loop is introduced.

Hardware boot marker:

```text
[VIDEO] CYD display profile gamma=1.00 saturation=115% resampling=nearest framebuffer=untouched
```

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
             CYD saturation profile
                          |
                          v
                    TFT exact x2
```

Stable bring-up recovery references, confirmed unchanged with the display profile:

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

Hardware hashes remain unchanged by the display profile:

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

`esp32-cyd-bringup` visualizes the exact touch geometry on the physical TFT:

```text
red rectangle     = accepted logical hitbox scaled exactly 2x
cyan cross        = most recent calibrated semantic touch
small yellow ring = exact touch centre
Serial            = raw / physical / logical touch coordinates
```

The overlay is drawn after framebuffer presentation with TFT_eSPI. It never
modifies the 160x120 framebuffer.

Current main-menu zones:

```text
Start Game logical x=28..119 y=67..78
Options    logical x=28..119 y=79..90
Help/About logical x=28..119 y=91..102
Exit       logical x=28..119 y=103..114
```

Current Options Back zone, validated on real touch hardware:

```text
Back logical  x=15..119 y=65..78
Back physical x=30..239 y=130..157
visible row   y=67..78
top tolerance = 2 logical pixels
```

Video/Input/Sound remain deferred but their diagnostic rectangles stay available
in bring-up.

## Real Options action and fast Back

Confirmed Options executes the original menu path:

```text
MENU_MAIN / Options selected 0cf107b1
    -> MenuSystem_select()
    -> MENU_MAIN_OPTIONS
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
With the saturation profile enabled the complete measured return is currently
about 147 ms in normal mode and about 157 ms in bring-up with the overlay.

## Original debug/developer menu

The reverse-engineered original engine contains hidden development menus:

```text
MENU_DEBUG
MENU_DEBUG_CHEATS
MENU_DEBUG_MAPS
MENU_DEBUG_STATS
MENU_DEVELOPER
```

They include cheats, maps, statistics, benchmark and developer facilities.
No original touch-hitbox visualization was found; the CYD overlay remains an
ESP32 touch adaptation.

## Current memory baseline

Validated normal firmware baseline with the saturation profile enabled:

```text
heap8    = 29064
largest8 = 17396
```

Bring-up baseline with hitbox diagnostics:

```text
heap8    = 28592
largest8 = 17396
```

Normal menu navigation keeps:

```text
shapeData    = NULL
mediaTexels  = NULL
wall cache   = inactive
sprite cache = inactive
```

## Build note

Use a clean build after PlatformIO profile, generated-source or significant
presentation changes:

```bash
cd ESP32
pio run -e esp32-cyd -t clean
pio run -e esp32-cyd -t upload
```

## Current safe boundary

Hardware validated:

- real core/layout/pre-render startup
- real Render startup
- real config + mappings
- real menu runtime structures through the pre-bitshape boundary
- bounded native wall/sprite rendering and LRU contracts in bring-up
- opaque deterministic `MENU_MAIN`
- calibrated touch / released double-tap semantics
- permanent bring-up hitbox overlay
- real Options action through `MenuSystem_select()`
- real `MENU_MAIN_OPTIONS`
- real Back through `MenuSystem_back()`
- fast opaque Back repaint
- no historical loading-bar flicker in normal mode
- hardware-selected CYD color profile: gamma 1.00 / saturation 1.15 / nearest
- framebuffer hashes unchanged by physical color tuning
- `shapeData == NULL`
- `mediaTexels == NULL`

Still deferred:

- Video/Input/Sound actions
- Help/About and Exit real actions
- Start Game / gameplay loader
- active normal multi-frame game loop
- gameplay controls
- future optimization of saturation cost if gameplay frame pacing requires it
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
