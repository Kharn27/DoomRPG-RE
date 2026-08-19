# Doom RPG ESP32 CYD porting status

This file is the recovery point for the ESP32-2432S028R classic Cheap Yellow
Display port. Update it on the same branch as every hardware-validated increment,
before merge.

## Target

- ESP32-2432S028R / classic CYD, no PSRAM
- ESP32-D0WD-V3, dual core, 240 MHz
- 4 MB flash
- ILI9341 320x240 landscape
- XPT2046 touch
- microSD-backed game data
- internal framebuffer: 160x120 RGB565 = 38,400 B
- physical output: exact nearest-neighbour 2x to 320x240
- audio still disabled during bring-up

## Project direction

DoomRPG-RE is an executable specification for behaviour, data formats and useful
rendering semantics, not an architecture contract. The ESP32 port is becoming its
own constrained engine:

- bounded deterministic RAM use
- SD as immutable backing store
- measured working sets and evidence-driven caches
- no monolithic `shapeData`
- no map-wide `mediaTexels`
- one shared 160x120 RGB565 framebuffer
- original BSP, projection, game and menu behaviour preserved where useful
- ESP32-specific presentation/resource architecture where the original design is
  unsuitable for the target
- one small hardware-validated subsystem per increment

Core philosophy:

> We are no longer forcing DoomRPG-RE onto ESP32. We are building an ESP32 Doom
> RPG engine from the behaviour and data model proven by DoomRPG-RE.

## Increment discipline

1. Start from the exact latest hardware-validated `main` SHA.
2. One branch = one small measurable objective.
3. Build/flash/test on the real CYD.
4. Fix failures on the same branch.
5. Only after hardware PASS, update all relevant `.md` files on that branch.
6. Only then is the branch merge-ready.
7. Merge.
8. Only after merge acknowledgement start the next increment.

Documentation is part of the increment, not a later cleanup task.

## Important merged milestones

- native 160x120 framebuffer + exact 2x TFT output
- real engine object graph / HUD / MenuSystem / Render startup
- real `menu.bsp` structural load stopped before legacy graphics inflation
- native asset pack v2, 241 random-access resources
- zero resident `shapeData`
- zero map-wide `mediaTexels`
- bounded GFXRM wall/sprite frames
- native projected wall + sprite rasterization
- real `menu.bsp` BSP traversal, camera and visible scene
- wall LRU3: 25 logical requests -> 11 physical loads
- sprite LRU3: 11 logical requests -> 9 physical loads
- native scene framebuffer `ffe0995e`
- fitted 160x120 `MENU_MAIN` geometry / layout FNV `47b3656e`
- touch-ready hand-only main menu with real XPT2046 selection
- released double-tap confirmation
- real Options action through `MenuSystem_select()`
- real `MENU_MAIN_OPTIONS` model (`Back / Video / Input / Sound`)
- real Back action through `MenuSystem_back()`
- opaque black main-menu presentation with deterministic touch hashes
- fast Options -> Main return without replaying MENUWALL/MENUSPRITE
- fast-menu branch merged as PR #32 at
  `cc2cb40cf026b5a5e232dba67f884905aca42488`

## Current validated increment

Branch: `agent/esp32-normal-boot-cleanup`

Base `main` SHA:

```text
cc2cb40cf026b5a5e232dba67f884905aca42488
```

Status: **HARDWARE VALIDATED; DOCUMENTATION UPDATED; READY TO MERGE**.

Objective: stop running the historical graphics/resource validation laboratory on
every normal boot while keeping all of those probes available on demand.

The firmware now has two explicit PlatformIO environments:

```text
esp32-cyd
    normal daily firmware

esp32-cyd-bringup
    full historical validation / diagnostic firmware
```

`esp32-cyd` remains the PlatformIO default environment.

## Two boot modes

### 1. Normal firmware: `esp32-cyd`

Build / flash:

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

Normal boot now executes only the engine state required to reach the current
interactive main menu safely:

```text
Platform video / SD / ZIP
    -> real engine core and layout
    -> real ParticleSystem / MenuSystem / EntityDef startup
    -> real Render_startup
    -> real config + mappings
    -> Render_beginLoadMap(MAP_MENU)
    -> real Render_beginLoadMapData structural phase
    -> stop at the validated boundary immediately before legacy bitshapes/texels
    -> real menu runtime structures remain resident
    -> direct opaque MENU_MAIN repaint
    -> touch gate armed
    -> READY
```

The normal mode deliberately skips the historical proof/demo chain after the
runtime structure boundary:

```text
BSP byte-plan diagnostics
RESOURCEPLAN
ASSETPAK full cross-check
BITSHAPE proof walk
SPRITETEX proof
SPRITERENDER demo
WALLRENDER demo
PROJWALL synthetic projection demo
MENUWALL full scene benchmark
MENUSPRITE full scene benchmark
```

No implementation has been deleted. Only normal execution is skipped.

Hardware marker:

```text
[BOOT] bringupProbes=off; skipping memory-plan/asset/sprite/wall/projected/menu-scene validation suite
```

The user-facing menu is then produced directly:

```text
[MAINOPAQUE] ... finalFNV=58a11171 ...
[BOOT] NORMAL READY mainMenuFNV=58a11171 ... shapeData=0x0 mediaTexels=0x0
```

### 2. Full laboratory firmware: `esp32-cyd-bringup`

Build / flash:

```bash
cd ESP32
pio run -e esp32-cyd-bringup -t upload
pio device monitor -e esp32-cyd-bringup
```

This environment extends `esp32-cyd` and adds:

```text
-D DOOMRPG_ESP32_BRINGUP_PROBES=1
```

It preserves the complete historical validation chain and should be used when
rechecking:

- resource memory budgets
- native asset-pack lookup and full-directory consistency
- bitshape source walk
- sprite texel random access
- GFXRM sprite/wall consumers
- projected-wall bridge
- wall and sprite LRU contracts
- deterministic native scene hashes
- allocator recovery after native graphics activity

In other words, `esp32-cyd-bringup` is the old scientific bring-up behaviour kept
as an explicit diagnostic profile instead of being the default product boot.

## Normal loader boundary remains real

The cleanup does **not** replace the real menu map loader with fake structures.
Normal boot still executes:

```text
Render_beginLoadMap(MAP_MENU)
Render_beginLoadMapData()
```

A linker wrapper around `DoomCanvas_updateLoadingBar()` counts the original loader
progress callbacks. The seventh callback still marks the already validated point
where:

- nodes are resident
- lines are resident
- map sprites/runtime sprite slots are resident
- events and bytecode are resident
- texture/sprite reference lists are resident
- the BSP I/O buffer has already been freed
- legacy `Render_loadBitShapes()` / `Render_loadTexels()` have not started

Hardware normal-mode runtime contract:

```text
nodes          = 53
lines          = 120
mapSprites     = 44
runtimeSprites = 68
events         = 15
mapTextures    = 84
mapSpriteRefs  = 284
planeTextures  = 11
persistent used= 14092 B
```

This is engine state, not a demonstration probe, so it remains part of normal
startup.

## Loading-bar flicker found and removed

The first hardware test of the cleaned normal boot reached the menu correctly but
briefly displayed a black screen with five small white/gray boxes, one filled.

The log showed an intermediate TFT present during the real structural loader:

```text
---Render_beginLoadMapData---
[SDL] Sharing platform framebuffer: 38400 bytes
[VIDEO] Present 160x120 -> 320x240 exact 2x: ...
```

Root cause: original `DoomCanvas_updateLoadingBar()` is a visual pacifier. It:

1. clears graphics to black;
2. draws five small rectangles;
3. fills the current rectangle;
4. advances `fillRectIndex`;
5. flushes the framebuffer to the display.

None of that visual work is required to construct the runtime structures.

Hardware-validated fix:

```text
normal esp32-cyd
    -> still count all loading-bar callbacks
    -> still stop on callback 7 at the exact same loader boundary
    -> suppress intermediate loading-bar drawing / TFT flushes

esp32-cyd-bringup
    -> preserve the historical loading-bar behaviour
```

The user confirmed the flicker is gone: normal boot now goes directly to the main
menu without the transient five-box loading screen.

## Normal-mode memory baseline

The cleaned normal firmware measured on hardware:

```text
heap8    = 29064
largest8 = 17396
```

The prior fast-menu build was approximately:

```text
heap8 = 28688
```

So the normal boot cleanup also leaves roughly 376 additional bytes free at the
current menu-ready baseline.

Strong invariant remains:

```text
shapeData   = NULL
mediaTexels = NULL
```

## Active opaque MENU_MAIN references

The real model remains:

```text
Start Game
Options
Help/About
Exit
```

Geometry:

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

Current hardware hashes:

```text
black + scaled logo        = 0ac1f9c6
Start Game selected         = 58a11171
Options selected            = 0cf107b1
Help/About selected         = 9db82b71
Exit selected               = bdd775f9
MENU_MAIN_OPTIONS model     = e1ef01f7
MENU_MAIN_OPTIONS framebuffer = 6058d47d
```

Historical scene-backed main-menu hashes remain recovery references only:

```text
Start Game old = cbc99461
Options old    = 961109a7
Help old       = e4eadfbb
Exit old       = 5ff2a5cd
```

## Fast Options -> Back remains validated

Normal navigation still uses the real engine model transitions:

```text
MENU_MAIN
    -> MenuSystem_select()
    -> MENU_MAIN_OPTIONS
    -> MenuSystem_back()
    -> MENU_MAIN
```

The return paint is bounded and opaque:

```text
MENU_MAIN_OPTIONS / 6058d47d
    -> double tap Back
    -> real MenuSystem_back()
    -> direct opaque MENU_MAIN repaint
    -> 58a11171
    -> touch re-armed
```

No MENUWALL/MENUSPRITE replay occurs during Back.

Previously measured complete Back model transition + paint + TFT present:

```text
~138 ms
```

## Touch input and Back tolerance

Main-menu physical 320x240 input is converted exactly to logical 160x120 and uses
50 ms stable-release rearming.

Main UX:

```text
first tap on another row -> select
first tap on current row -> arm
second released tap      -> confirm
```

Back touch tolerance validated on hardware:

```text
visible Back row     y=67..78
logical Back hitbox  x=15..119 y=64..78
physical Back hitbox x=30..239 y=128..157
```

The extra upper tolerance handles observed XPT2046 jitter while staying clear of
Video at logical y=79.

## Bring-up mode is now the permanent home for visual hitbox diagnostics

Do **not** create a disposable product-code branch merely to visualize touch
zones. The better long-term design is to add an optional hitbox overlay to the
`esp32-cyd-bringup` profile.

Planned bring-up-only overlay:

```text
- red rectangle outline for every active logical hitbox
- current menu hitboxes shown together
- optional marker/cross for the last physical/logical touch
- Serial still reports raw / physical / logical coordinates and detected item
```

Workflow:

```text
flash esp32-cyd-bringup with hitbox overlay enabled
    -> photograph real CYD
    -> compare visible controls against red rectangles
    -> adjust all zones together
    -> repeat if needed
    -> keep calibrated constants in normal firmware
```

This is intentionally useful beyond the current main menu. Future submenus,
gameplay controls or screen-layout changes may need the same visual calibration,
so keeping the overlay as a bring-up diagnostic is preferable to deleting it once.

Normal `esp32-cyd` must remain free of those red diagnostic overlays.

## Native graphics recovery references

The following contracts are no longer rerun on every normal boot, but remain
available through `esp32-cyd-bringup`:

```text
sprite 172 texel FNV          = 0c0a7acd
wall 112 texel FNV            = 92d40704
synthetic projected wall FNV  = ad191f54
real walls framebuffer        = a6d87c4a
viewSprites list FNV          = 962cd657
sprite request FNV            = 4457ac94
walls + sprites framebuffer   = ffe0995e
faithful original MENU_MAIN   = 86c38260
historical fitted MENU_MAIN   = 1afa0223
failed double-gray wall frame = b6f86faa
```

Validated cache evidence retained for bring-up regression:

```text
Wall LRU3
  requests 25
  hits 14
  misses 11
  evictions 8
  peak payload 6144 B

Sprite LRU3
  requests 11
  hits 2
  misses 9
  evictions 6
  peak logical payload 6038 B
```

## Display and logging policy

Normal firmware:

```text
TFT    -> game/menu framebuffer only
Serial -> concise startup + touch/runtime diagnostics
```

Bring-up firmware:

```text
TFT    -> game framebuffer plus explicitly enabled bring-up visual diagnostics
Serial -> full historical proof/benchmark output
```

`DOOMRPG_ESP32_SCREEN_DIAGNOSTICS=0` remains the normal default.

## Current safe boundary

Hardware validated and executed in normal mode:

- platform video, SD and ZIP
- real core/layout/pre-render startup
- real Render startup
- real config and mappings
- real menu map structural runtime load
- stop before legacy monolithic bitshape/texel loaders
- direct opaque `MENU_MAIN`
- deterministic menu hashes
- calibrated touch and released double-tap semantics
- real Options transition
- real Back transition
- fast opaque Back repaint
- no intermediate loading-bar TFT flicker
- `shapeData == NULL`
- `mediaTexels == NULL`

Validated but moved out of normal execution into `esp32-cyd-bringup`:

- BSP structure planning diagnostics
- resource memory planning
- full native asset-pack proof
- bitshape / sprite-texel proof passes
- standalone sprite/wall render consumers
- synthetic projected-wall regression
- full menu wall/sprite scene reconstruction
- LRU benchmark/accounting regressions

Still intentionally deferred:

- Video/Input/Sound actions
- Help/About and Exit real actions
- Start Game / gameplay loader activation
- active normal multi-frame game loop
- gameplay controls
- bring-up visual hitbox overlay implementation
- final color/contrast investigation
- audio

## Recommended next increment after merge

Add reusable **bring-up-only visual hitbox diagnostics**.

First target:

```text
MENU_MAIN hitboxes
MENU_MAIN_OPTIONS hitboxes
last-touch marker
```

The overlay should reuse the actual hitbox constants used by input logic rather
than duplicating coordinates, so the photograph shows exactly what the firmware
will accept. Keep all visual overlay code disabled from the normal `esp32-cyd`
environment.
