# Doom RPG ESP32 port

This directory contains the ESP32-specific Doom RPG engine/port for the classic
**ESP32-2432S028R Cheap Yellow Display (CYD), without PSRAM**.

DoomRPG-RE is used as an **executable specification** for game behavior, data
formats and useful rendering semantics. It is not the architecture contract for
the final firmware. The ESP32 project is deliberately becoming its own engine,
and the desktop-derived engine sources may eventually disappear entirely from
the ESP32 build once their behavior/data contracts have been recovered.

Compatibility wrappers and calls into `Render_*`, `DoomCanvas_*` or other legacy
objects are therefore transitional scaffolding: useful for measurement and
behavior comparison, but not dependencies that the final CYD engine must keep.

For exact current RAM/FNV/state recovery values, read
[`PORTING_STATUS.md`](PORTING_STATUS.md).
For the documentation structure and milestone archive rules, read
[`DOCUMENTATION.md`](DOCUMENTATION.md).

## Target hardware

- ESP32-2432S028R / classic CYD
- ESP32-D0WD-V3, dual core, 240 MHz
- 4 MB flash
- **no PSRAM**
- ILI9341 320x240 landscape TFT
- XPT2046 resistive touch
- microSD-backed game data
- audio currently disabled during bring-up

The engine renders into one shared logical framebuffer:

```text
160x120 RGB565 = 38,400 bytes
```

Presentation is an exact nearest-neighbor 2x conversion to the 320x240 panel.

## Build, flash and monitor

Normal firmware:

```bash
cd ESP32
pio run -e esp32-cyd -t clean
pio run -e esp32-cyd -t upload
pio device monitor -e esp32-cyd
```

Diagnostic bring-up firmware:

```bash
cd ESP32
pio run -e esp32-cyd-bringup -t clean
pio run -e esp32-cyd-bringup -t upload
pio device monitor -e esp32-cyd-bringup
```

A clean build is recommended after PlatformIO profile changes, generated-source
changes or significant presentation/resource work.

## PlatformIO environments

### `esp32-cyd`

Everyday optimized firmware. It runs only the real initialization and currently
reachable game flow needed for the normal port. Historical proof probes and the
physical touch-hitbox overlay are not part of the normal presentation path.

### `esp32-cyd-bringup`

Diagnostic laboratory. It adds:

```text
DOOMRPG_ESP32_BRINGUP_PROBES=1
DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY=1
```

Use it for resource-pack regressions, memory budgets, bitshapes, projected walls
and sprites, LRU cache checks, menu geometry and touch calibration.

Both environments use the same selected CYD display profile.

## Engine direction

The no-PSRAM target cannot afford the original desktop/J2ME-derived architecture
as a whole. Permanent design rules are:

- bounded deterministic RAM use
- SD as immutable backing storage
- one shared 160x120 RGB565 framebuffer
- no resident monolithic `shapeData`
- no resident map-wide `mediaTexels`
- random-access or streaming native resources where legacy ZIP inflation becomes unsuitable
- bounded wall/sprite working sets and measured LRU caches
- preserve original game behavior and resource formats where useful
- replace desktop/J2ME ownership, allocation and lifecycle models that are inappropriate for the ESP32
- do not preserve a legacy type/function merely because existing desktop code uses it
- one small hardware-validated increment at a time

The intended long-term ownership is:

```text
Doom RPG data / recovered behavior
              |
              v
      ESP32-native parsers
              |
              v
      ESP32-native runtime
              |
              v
   ESP32-native renderer/game
              |
              v
       160x120 RGB565
              |
              v
         CYD 320x240
```

Current graphics shape already follows this direction for walls/sprites:

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
                  CYD output transform
                          |
                          v
                    exact TFT 2x
```

The exact cache measurements and regression hashes live in
[`PORTING_STATUS.md`](PORTING_STATUS.md).

## SD card and native asset pack

The migration setup currently expects:

```text
/DoomRPG.zip
/DoomRPG-ESP32.pak
```

`DoomRPG.zip` remains available for legacy paths and format recovery not yet
migrated. It is not a commitment to retain the legacy loader architecture.
`DoomRPG-ESP32.pak` is the ESP32-native random-access resource pack.

Build the native pack with:

```bash
python3 ESP32/tools/build_asset_pack.py \
    /path/to/DoomRPG.zip \
    /path/to/DoomRPG-ESP32.pak
```

The native pack stores entries uncompressed behind a hash-sorted on-disk index so
migrated consumers can seek/read only the requested resource without a large ZIP
inflate peak.

## Display profile

Hardware-selected output:

```text
gamma       = 1.00
saturation  = 1.15
resampling  = nearest
```

The correction is applied only at the final panel boundary:

```text
engine / palettes / GFXRM
        -> logical 160x120 RGB565 framebuffer
        -> deterministic framebuffer FNVs
        -> CYD saturation 1.15 transform
        -> exact nearest-neighbor 2x
        -> ILI9341
```

The logical framebuffer remains the source of truth and is not modified by panel
color tuning.

Representative hardware timing is currently about 42.7 ms for a normal full
screen presentation. The bounded intro clock therefore cannot physically present
every nominal 50 ms / 20 FPS virtual tick and measures about 14 rendered FPS with
skipped stale ticks. The virtual story timeline remains correct.

There is deliberately **no fixed gameplay FPS target yet**. Doom RPG is
turn-based, so input/game-turn cadence must not be tied to full-screen panel
presentation. The preferred gameplay model is demand-driven: static scenes should
not be redrawn continuously, while actions/animations render only frames with
visual value. A lower animation cadence than a real-time shooter may be perfectly
acceptable, but perceived smoothness and hardware measurements will decide that
after the native gameplay renderer exists.

Performance priority remains:

```text
correct behavior
-> bounded RAM
-> responsive input/game logic
-> correct visuals
-> measured rendering optimization
```

`PlatformVideo_present()` and the final saturation transform remain measured
optimization candidates; do not optimize them before the gameplay architecture is
correct.

## Normal boot and menu path

Normal boot currently follows this bounded path:

```text
Platform video / SD / ZIP
    -> transitional engine core + layout startup
    -> Render_startup
    -> config + mappings
    -> Render_beginLoadMap(MAP_MENU)
    -> structural menu-map runtime load
    -> stop before legacy monolithic bitshape/texel inflation
    -> direct opaque MENU_MAIN
    -> synchronize DoomCanvas.state to ST_MENU
    -> touch armed
    -> READY
```

This path is hardware-proven scaffolding, not a declaration that the final native
engine must retain `Render_t`/`DoomCanvas_t`.

The native menu intentionally bypasses the heavy original menu painter, but it
preserves the original `MENU_MAIN` model:

```text
Start Game
Options
Help/About
Exit
```

Real resources are retained for the visible menu (`j.bmp` logo, `p.bmp` cursor and
the Doom bitmap font). Exact fitted geometry, framebuffer hashes and runtime
structure counts are maintained in `PORTING_STATUS.md`.

## Touch model

The XPT2046 path is shared by menu and intro interaction:

```text
XPT2046
  -> PlatformInput
  -> calibrated physical 320x240 point
  -> semantic callback on press edge
  -> hold produces no repeat
  -> stable release for 50 ms rearms the next tap
  -> logical 160x120 hit testing
```

The menu adds its own selection/confirmation semantics on top of that platform
callback. Bring-up mode can draw the accepted hitboxes and last calibrated touch
on the physical TFT without modifying the logical framebuffer.

The current intro prompt band is the bottom of the fitted 120x120 story square:

```text
story viewport = x20..139 y0..119
prompt band    = x20..139 y102..119
```

Page 1 has no prompt, so the whole story viewport is accepted for the optional
animation skip.

A press while story text is still progressively revealing completes that text;
a later press performs `More` / `Continue`. Because the platform rearms only
after a stable 50 ms release, an extremely fast second press can feel less
responsive than a conventional button. Current hardware evidence shows accepted
hits inside the intended prompt band; this is tracked as a non-blocking UX polish
item rather than a state-machine failure.

## Fresh Start Game lifecycle

On a fresh profile with no compatible save, the real original action is used as
the current executable behavioral reference:

```text
MENU_MAIN / Start Game
    -> MenuSystem_select()
    -> Menu_select(MENU_MAIN, 0)
    -> Menu_startGame(menu, 1)
    -> Player_reset()
    -> DoomCanvas_setState(ST_INTRO)
    -> DoomCanvas_loadPrologueText()
```

Before that irreversible transition, the ESP32 path deliberately frees resources
that the original lifecycle would already have discarded but the native menu boot
still owns:

```text
DoomRPG_freeImage(imgLegals)
Render_freeRuntime(render)
Game_unloadMapData(game)
```

Real-CYD measurement showed an exact 55,416-byte 8-bit heap recovery from this
cleanup. Detailed build-specific baselines and the intro asset sizes remain in
`PORTING_STATUS.md`.

The existing-save branch is intentionally different: menu runtime is retained so
the original Continue/New Game model can later be implemented.

## Bounded intro architecture

The original story renderer assumes a 128x128 viewport. The CYD logical screen is
only 120 pixels high, so the ESP32 path treats 128x128 as a **virtual story space**
and maps it directly to a centered 120x120 viewport:

```text
virtual story space : 128x128
ESP32 viewport      : 120x120
viewport origin     : x=20, y=0
logical framebuffer : 160x120
physical TFT        : exact 2x -> 320x240
```

There is no intermediate 128x128 framebuffer and no fit-time heap allocation.
The fitted path covers the scrolling background, progressive Doom font, hand,
`More` / `Continue`, animated layers, spaceship and laser geometry.

After the first deterministic intro frame, an ESP32-owned 50 ms virtual clock
drives at most one fitted frame per Arduino loop service. Stale virtual ticks are
skipped rather than rendered in catch-up bursts.

Intro input reproduces the original visible semantics without entering the broad
legacy `DoomCanvas_run()` loop:

```text
page 0 text 0: reveal -> More
page 0 text 1: reveal -> Continue
page 1:        natural ~10 s timeout OR touch skip
page 2:        reveal -> final Continue
```

The final Continue is guarded in two stages. It first performs the validated
`intro-exit-ready` PARK with the intro assets still resident. On the next Arduino
loop service, a one-shot native disposer mirrors the resource-freeing behavior
needed by the data contract while deliberately excluding the legacy immediate map
load:

```text
page 2 final Continue
    -> clock/input PARK
    -> free c.bmp / d.bmp / e.bmp / f.bmp
    -> free all three story text buffers
    -> reset render clip
    -> storyPage = 3
    -> PARK again
    -> NO map load
```

The PR #41 real CYD recovered **33,768 bytes of 8-bit heap** during this teardown
and grew the largest free 8-bit block from **13,300** to **36,852 bytes**. The
framebuffer FNV remained unchanged across disposal, proving that the last rendered
intro image can remain visible after its source resources have been released.

Detailed evidence lives in:

- [`FIRST_INTRO_FRAME.md`](FIRST_INTRO_FRAME.md)
- [`INTRO_CLOCK.md`](INTRO_CLOCK.md)
- [`INTRO_INPUT.md`](INTRO_INPUT.md)
- [`INTRO_DISPOSE.md`](INTRO_DISPOSE.md)

## First post-prologue BSP measurement

The current development branch probes `startupMap=1`. The recovered map enum is
important:

```text
MAP_MENU     = 0
MAP_INTRO    = 1 -> /intro.bsp
MAP_SECTOR01 = 2 -> /level01.bsp
```

So the first gameplay transition after the visual prologue opens `/intro.bsp`,
not `level01.bsp`.

The normal `esp32-cyd` hardware probe measured the complete resource as:

```text
/intro.bsp compressed/uncompressed = 11150 / 21823 B
nodes                               = 223
lines                               = 480
mapSprites                          = 344
runtimeSprites                      = 368
events                              = 93
byteCodes                           = 265
strings                             = 94
string payload                      = 7873 B
```

The current desktop-derived structural representation would require **55,341 B**
of runtime allocation while the complete 21,823-byte BSP is still resident. With
mappings and raw BSP resident, only **54,104 B** remained. It therefore does not
fit even with the safety headroom reduced to zero.

The probe correctly refused and returned to the post-intro boundary with stable
heartbeats. That result is the architectural trigger for the next implementation:
an ESP32-native streaming BSP reader, preferably two-pass initially:

```text
pass 1: stream + validate + count
        -> exact native allocation plan

allocate final compact native pools

pass 2: stream again
        -> populate final native runtime directly
```

The full 21,823-byte BSP should never need to coexist with the complete runtime.
Native map structures are not required to match desktop `Node_t`, `Line_t` or
`Sprite_t`, and the old 8,440-byte resident mapping model is also subject to
replacement rather than being treated as mandatory.

See [`MAP1_STRUCTURAL_LOAD.md`](MAP1_STRUCTURAL_LOAD.md) for the complete real-CYD
measurement and native-loader plan.

## Current high-level safe boundary

Merged `main` PR #41 is hardware-validated after intro disposal and before map
loading:

```text
ST_INTRO page 3
intro clock/input stopped
intro images/texts = NULL
render clip        = off
startupMap         = 1
shapeData          = NULL
mediaTexels        = NULL
```

On the current development branch, the MAP_INTRO feasibility probe temporarily
loads mappings and `/intro.bsp`, safely refuses the legacy structural working set,
cleans all temporary runtime data and returns to the same logical PARK. Current
branch heartbeats are stable around:

```text
heap8     = 84384
largest8  = 36852
```

The authoritative current SHA, exact recovery values and next implementation
boundary are kept in [`PORTING_STATUS.md`](PORTING_STATUS.md).

## Original debug/developer menus

The reverse-engineered engine contains useful original hidden development menus:

```text
MENU_DEBUG
MENU_DEBUG_CHEATS
MENU_DEBUG_MAPS
MENU_DEBUG_STATS
MENU_DEVELOPER
```

They remain behavior/UI references; the priority path is still the bounded normal
Start Game -> intro -> native first gameplay-map transition.

## Porting workflow

1. Branch from the exact latest hardware-validated `main`.
2. Implement or measure one small bounded objective.
3. Build, flash and test on the real classic CYD.
4. Fix failures or redesign on the same branch when the measurement defines the next step.
5. Record the hardware evidence.
6. Update `PORTING_STATUS.md` before merge.
7. Update this README only when stable architecture/usage changes.
8. Merge only when the branch reaches a coherent code + hardware + documentation boundary.

See [`DOCUMENTATION.md`](DOCUMENTATION.md) for the full documentation retention
rules.
