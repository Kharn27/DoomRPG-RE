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

## Hardware-selected CYD display profile

Current hardware-selected profile:

```text
gamma       = 1.00
saturation  = 1.15
resampling  = nearest
```

The correction is applied only at the final TFT presentation boundary:

```text
engine / palettes / GFXRM
        -> logical 160x120 RGB565 framebuffer
        -> deterministic FNV hashes
        -> CYD saturation 1.15 output transform
        -> exact nearest-neighbour 2x
        -> ILI9341
```

The logical framebuffer is never modified by panel tuning.

Representative hardware timing:

```text
normal full-screen Present          ~= 42.7 ms
bring-up Present + physical overlay ~= 44.3 ms
old neutral Present                 ~= 34.4 ms
```

The saturation cost remains a future frame-pacing optimization point once the
active intro/game loop is running.

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
    -> synchronize DoomCanvas to ST_MENU
    -> touch armed
    -> READY
```

Expected normal final menu markers include:

```text
[MENUTOUCH] STATE SYNC canvas=0->2 source=native-MENU_MAIN activation
[MAINOPAQUE] ... finalFNV=58a11171 ...
[BOOT] NORMAL READY mainMenuFNV=58a11171 ... shapeData=0x0 mediaTexels=0x0
```

The explicit state synchronization is required because the ESP32 native menu
boot intentionally bypasses the original heavy `MenuSystem_setMenu()` painter,
but must preserve its state-machine contract.

## Real menu runtime boundary

Normal boot still uses the real menu map loader. The wrapper stops the original
structural load on the seventh `DoomCanvas_updateLoadingBar()` callback,
immediately before the legacy `Render_loadBitShapes()` / `Render_loadTexels()`
phase.

Hardware-validated state while MENU_MAIN is active:

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

The pack stores entries uncompressed behind an on-disk hash-sorted index, so
future migrated consumers can seek/read directly without ZIP inflate peaks.

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

A real CYD Start tap landed at logical `y=64`; Start therefore has a deliberately
narrow extra top tolerance while the other rows retain their prior geometry:

```text
Start Game logical x=28..119 y=64..78
Options    logical x=28..119 y=79..90
Help/About logical x=28..119 y=91..102
Exit       logical x=28..119 y=103..114
```

Options Back remains:

```text
Back logical  x=15..119 y=65..78
Back physical x=30..239 y=130..157
visible row   y=67..78
top tolerance = 2 logical pixels
```

## Bring-up touch-hitbox overlay

`esp32-cyd-bringup` visualizes the accepted touch geometry on the physical TFT:

```text
red rectangle     = accepted logical hitbox scaled exactly 2x
cyan cross        = most recent calibrated semantic touch
small yellow ring = exact touch centre
Serial            = raw / physical / logical touch coordinates
```

The overlay is drawn after framebuffer presentation and never modifies the
160x120 framebuffer.

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

## Real fresh Start Game path

A first-boot profile with no compatible save now executes the original Start
path directly from MENU_MAIN:

```text
MENU_MAIN / Start Game
    -> MenuSystem_select()
    -> Menu_select(MENU_MAIN, 0)
    -> Menu_startGame(menu, 1)
    -> Player_reset()
    -> DoomCanvas_setState(ST_INTRO)
    -> DoomCanvas_loadPrologueText()
```

Start preconditions deliberately remain strict:

```text
menu      = MENU_MAIN
selected  = 0
state     = ST_MENU
frame FNV = 58a11171
shapeData = NULL
mediaTexels = NULL
wall/sprite caches inactive
```

### Fresh-start RAM cleanup

The first hardware attempt reached the real intro loader but OOMed while
inflating `c.bmp`. The cause was not the intro itself; the native boot was still
holding memory that the original lifecycle would already have released.

Before the irreversible fresh New Game transition, the ESP32 path now frees:

```text
imgLegals / g.bmp
menu.bsp runtime
mapping/runtime arrays
```

through:

```text
DoomRPG_freeImage(imgLegals)
Render_freeRuntime(render)
Game_unloadMapData(game)
```

Hardware measurement:

```text
heap8     29064 -> 84480
largest8  17396 -> 36852
gained    55416 B
```

After cleanup:

```text
nodes       = NULL
lines       = NULL
mapSprites  = NULL
mappings    = NULL / NULL
shapeData   = NULL
mediaTexels = NULL
```

The cleanup is only used for the fresh-profile branch. If an existing compatible
save is detected, menu runtime is kept because the original action must instead
open `MENU_MAIN_CONTINUE`.

### Intro assets

The original prologue loader then successfully loads all four indexed BMPs:

```text
c.bmp  192x128 4-bpp packed pixels=12288 B palette=16
d.bmp  192x128 4-bpp packed pixels=12288 B palette=3
e.bmp  128x128 4-bpp packed pixels=8192 B  palette=16
f.bmp  9x9     4-bpp packed pixels=45 B    palette=8
```

ZIP sizes observed on hardware:

```text
c.bmp c=3675 u=12408
d.bmp c=149  u=12356
e.bmp c=1352 u=8312
f.bmp c=114  u=160
```

The existing packed indexed ESP32 BMP path is sufficient once dead menu memory
is released, so this increment does not yet migrate intro images to the native
`.pak` reader.

### Validated Start result

Hardware result after the real original action returns:

```text
menu            = MENU_NONE (0)
state           = ST_INTRO (9)
framebuffer FNV = 485915c5
heap8           = 50712
largest8        = 13300
shapeData       = NULL
mediaTexels     = NULL
```

Fresh `Player_reset()` contract:

```text
level           = 1
currentXP       = 0
nextLevelXP     = 80
credits         = 0
keys            = 0
ammo[1]         = 8
weapon          = 2
weapons         = 0x00000004
disabledWeapons = 0
totalDeaths     = 0
```

The three prologue text buffers are allocated and story page counters are reset
to page 0.

### Why the display is currently black after `Loading...`

This is the intended stop point for the current increment.

`DoomCanvas_loadPrologueText()` shows `Loading...`, loads the text and four intro
images, then clears the framebuffer to black and presents it. The current ESP32
`loop()` is still intentionally parked and does not yet drive the original
`ST_INTRO` drawing state.

So after Start:

```text
Loading...
    -> intro assets become resident
    -> final black clear
    -> state remains ST_INTRO
    -> heartbeat continues
```

The next increment is to drive **one real intro frame**, not to enter map loading.

## Current memory baselines

Interactive normal-menu baseline before Start:

```text
heap8    = 29064
largest8 = 17396
```

Bring-up menu baseline with hitbox diagnostics:

```text
heap8    = 28592
largest8 = 17396
```

Fresh Start boundary after prologue load:

```text
heap8    = 50712
largest8 = 13300
```

The higher free-heap value after Start reflects deliberate release of the legal
screen and menu-map runtime before the intro assets become resident.

## Original debug/developer menu

The reverse-engineered engine also contains hidden development menus:

```text
MENU_DEBUG
MENU_DEBUG_CHEATS
MENU_DEBUG_MAPS
MENU_DEBUG_STATS
MENU_DEVELOPER
```

They remain useful future references, but Start Game / intro progression is now
the priority path.

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
- native MENU_MAIN canvas-state synchronization to `ST_MENU`
- real Options action through `MenuSystem_select()`
- real `MENU_MAIN_OPTIONS`
- real Back through `MenuSystem_back()`
- fast opaque Back repaint
- no historical loading-bar flicker in normal mode
- hardware-selected CYD color profile: gamma 1.00 / saturation 1.15 / nearest
- fresh direct Start Game through the original `MenuSystem_select()` path
- original `Player_reset()` contract
- fresh-start release of dead legal/menu runtime, recovering 55,416 B
- successful load of original prologue strings and `c/d/e/f.bmp`
- real `ST_INTRO` reached with `heap8=50712`, `largest8=13300`
- `shapeData == NULL`
- `mediaTexels == NULL`

Still deferred:

- rendering/driving the first active `ST_INTRO` frame
- intro touch/key progression
- first map/gameplay load after the intro
- existing-save Continue / New Game submenu painter/action
- Video/Input/Sound actions
- Help/About and Exit real actions
- active normal multi-frame game loop
- gameplay controls
- future optimization of saturation cost if active frame pacing requires it
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
