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
- gameplay viewport: 160x80 at framebuffer y=20
- physical output: exact nearest-neighbour 2x to 320x240
- audio still disabled during bring-up

## Project direction

DoomRPG-RE is an executable specification for behaviour, data formats and useful
rendering semantics, not an architecture contract. The ESP32 port is progressively
becoming its own constrained engine:

- bounded deterministic RAM use
- SD as immutable backing store
- measured working sets and evidence-driven caches
- no monolithic `shapeData`
- no map-wide `mediaTexels`
- one shared 160x120 RGB565 framebuffer
- storage isolated behind GFXRM
- cache policy isolated from storage/rasterization
- original BSP, projection, game and menu behaviour preserved where useful
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
- faithful original `MENU_MAIN` overlay reference `86c38260`
- clean TFT game ownership, Serial diagnostics retained
- fitted 160x120 `MENU_MAIN` geometry, historical framebuffer `1afa0223`
- touch-ready hand-only `MENU_MAIN` with real XPT2046 selection
- selected-frame hashes:
  - Start Game `cbc99461`
  - Options `961109a7`
  - Help/About `e4eadfbb`
  - Exit `5ff2a5cd`
- double-tap semantic confirmation validated on hardware
- real Options action through `MenuSystem_select()`
- real `MENU_MAIN_OPTIONS` model (`Back / Video / Input / Sound`)
- bounded Options framebuffer `6058d47d`
- Options action merge on `main` / PR #30:
  `c2ab0d1135e3c94af61e8c303469a5a96aa3caeb`

## Current validated increment

Branch: `agent/esp32-options-back-roundtrip`

Base `main` SHA:

```text
c2ab0d1135e3c94af61e8c303469a5a96aa3caeb
```

Status: **HARDWARE VALIDATED, DOCUMENTED, READY TO MERGE**.

Objective: prove the first complete real menu hierarchy round trip:

```text
MENU_MAIN
    -> double tap Options
    -> real MenuSystem_select()
    -> MENU_MAIN_OPTIONS
    -> double tap Back
    -> real MenuSystem_back()
    -> rebuilt native MENU_MAIN scene
    -> touch-ready MENU_MAIN re-armed
```

`Video`, `Input`, `Sound`, `Start Game`, `Help/About` and `Exit` remain deferred.

## Real Options -> Back transition

The Options screen arms a deliberately narrow touch callback. Only `Back` can
execute an action in this increment.

Hardware-validated Options touch geometry:

```text
Back logical x=15..119 y=67..78
Back physical x=30..239 y=134..157
```

The UX remains two released taps:

```text
first Back tap  -> ARM
release
second Back tap -> MenuSystem_back()
```

`MenuSystem_back()` correctly restores the original menu model:

```text
menu          = MENU_MAIN / 1
type          = 4
oldMenu       = -1
selectedIndex = 0
numItems      = 4
state         = ST_MENU / 2
```

This proves the hierarchy transition itself is real original Doom RPG behaviour.

## Re-entry regression found and fixed on hardware

The first hardware attempt reached the correct `MENU_MAIN` model, then failed
while reconstructing the native wall frame:

```text
[OPTIONBACK] MODEL menu=1 type=4 old=-1 selected=0 items=4 state=2
[MENUWALL] framebufferFNV=b6f86faa expected=a6d87c4a
[MENUWALL] FAILED cached real menu regression contract changed
```

All geometric/resource invariants were still identical:

```text
walls        = 25
wall requests= 25
unique       = 8
requestFNV   = 4db9da28
spans        = 224
pixels       = 5589
cache        = 14 hits / 11 misses / 8 evictions
```

Root cause: `Render_setGrayPalettes()` is destructive and non-idempotent. It
rewrites `mediaPalettes`, `floorColor[0]` and `ceilingColor[0]` in place. The
menu scene had already been converted to grayscale during initial startup; the
Back reconstruction converted the already-gray RGB565 values a second time,
introducing rounding changes and therefore framebuffer `b6f86faa`.

The fix leaves the original wall probe and renderer source unchanged. A tiny ESP32
re-entry bridge wraps the probe boundary and `Render_setGrayPalettes()`:

```text
normal boot
    -> real Render_setGrayPalettes()

Options framebuffer 6058d47d + model already returned to MENU_MAIN
    -> arm one-shot re-entry guard
    -> skip exactly the next destructive grayscale conversion
    -> keep normal floor/ceiling replication and native render path
```

Hardware markers for the fixed path:

```text
[MENUWALL] REENTRY detected from Options framebuffer=6058d47d; preserving already-gray palette
[MENUWALL] REENTRY grayscale already applied; skipping destructive second Render_setGrayPalettes pass
```

The corrected wall frame is again exact:

```text
[MENUWALL] framebufferFNV=a6d87c4a expected=a6d87c4a
```

## Authoritative round-trip hardware validation

The real CYD then produced the complete deterministic return:

```text
[OPTIONBACK] CONFIRM Back action=MenuSystem_back

=== Doom RPG ESP32 real Options -> MENU_MAIN Back roundtrip ===
[OPTIONBACK] Begin menu=7 selected=0 old=1 framebufferFNV=6058d47d shapeData=0x0 mediaTexels=0x0
[OPTIONBACK] MODEL menu=1 type=4 old=-1 selected=0 items=4 state=2
[MENUWALL] REENTRY detected from Options framebuffer=6058d47d; preserving already-gray palette
...
[MENUWALL] framebufferFNV=a6d87c4a expected=a6d87c4a
...
[MENUSPRITE] framebufferFNV=ffe0995e expected=ffe0995e wallsFNV=a6d87c4a
...
[MAINTOUCHLAYOUT] framebufferFNV=cbc99461 sceneFNV=ffe0995e
[MENUTOUCH] GATE READY initialSelected=0 firstSameTap=arm secondReleasedSameTap=confirm optionsAction=enabled
...
[OPTIONBACK] End framebufferFNV=cbc99461 expected=cbc99461 menu=1 selected=0 touchActive=1 shapeData=0x0 mediaTexels=0x0
[OPTIONBACK] READY real MenuSystem_back returned to bit-identical touch-ready MENU_MAIN
[OPTIONBACK] READY MENU_MAIN touch re-armed; Options can be entered again with the same deterministic hashes
```

Deterministic round-trip chain:

```text
MENU_MAIN selected Options = 961109a7
MENU_MAIN_OPTIONS          = 6058d47d
Back walls                 = a6d87c4a
Back walls + sprites       = ffe0995e
Back MENU_MAIN             = cbc99461
```

## Memory boundary after round trip

The final hardware build reports:

```text
heap8    = 28680
largest8 = 17396
```

During wall/sprite cache activity the heap drops transiently as expected, then
returns to the same baseline. At the final Options -> Main boundary:

```text
shapeData   = NULL
mediaTexels = NULL
wall cache  = inactive
sprite cache= inactive
```

The small drop from the previous `28704` baseline is the static cost of the new
round-trip/re-entry control state, not a per-navigation leak.

## Important UX/performance finding: Back currently feels like a mini reboot

The round trip is functionally correct, but the current return mechanism is a
**validation/recovery path, not the desired final menu-navigation path**.

It does **not** rerun the full startup sequence. In particular, Back does not rerun:

- `ParticleSystem_startup()`
- `MenuSystem_startup()`
- `EntityDef_startup()`
- `Render_startup()`
- config/mappings load
- `menu.bsp` structural parsing
- asset-pack/bitshape preflight probes

However, it does rerun the two expensive already-validated scene probes so the
shared framebuffer can be reconstructed without a second 38,400-byte buffer:

```text
MENUWALL    ~= 1355 ms on measured hardware
MENUSPRITE  ~= 1039 ms on measured hardware
plus framebuffer presents / menu composition
```

So a Back navigation takes roughly 2.5 seconds and emits a large amount of probe
logging. The user's observation that it feels like the game rebooted is therefore
accurate from a UX perspective even though no actual reboot/startup occurred.

Do **not** treat this heavy rebuild as the final menu architecture.

## Touch architecture status

Validated path:

```text
XPT2046
   -> PlatformInput
   -> one tap per press/release cycle
   -> 50 ms stable-release rearm
   -> physical 320x240 -> logical 160x120
   -> menu-specific hit test
   -> real MenuSystem model transition
```

`MENU_MAIN` cursor movement still uses four static 13x10 RGB565 patches:

```text
4 * 13 * 10 * 2 B = 1,040 B
```

No second full framebuffer exists.

## Linker-wrapper rules learned so far

1. Do not blindly `--wrap` a callback whose address is taken in its own
   translation unit; local function-pointer identity may bypass the wrapper and
   break pointer comparisons.
2. A narrow wrapper can still be useful at an explicit cross-translation-unit
   boundary. The grayscale re-entry bridge is acceptable because it guards one
   deterministic transition and is transparent during normal boot.

## Deterministic regression boundaries

```text
sprite 172 texel FNV              = 0c0a7acd
wall 112 texel FNV                = 92d40704
synthetic projected wall FNV      = ad191f54
real walls framebuffer            = a6d87c4a
viewSprites list FNV              = 962cd657
sprite request FNV                = 4457ac94
walls + sprites framebuffer       = ffe0995e
faithful MENU_MAIN layout         = 86c38260
ESP32 160x120 layout config       = 47b3656e
historical fitted MENU_MAIN       = 1afa0223
touch Start Game selected         = cbc99461
touch Options selected            = 961109a7
touch Help/About selected         = e4eadfbb
touch Exit selected               = 5ff2a5cd
MENU_MAIN_OPTIONS model           = e1ef01f7
MENU_MAIN_OPTIONS framebuffer     = 6058d47d
failed double-gray wall frame     = b6f86faa
```

## Display ownership

Normal mode remains:

```text
Serial -> startup/probe/debug/touch diagnostics
TFT    -> shared game framebuffer only
```

`DOOMRPG_ESP32_SCREEN_DIAGNOSTICS=0` remains the default.

## Open visual issue: color / contrast

Comparison against the J2ME reference still shows the main-menu presentation on
CYD looking flatter / less saturated / lower contrast. Do not change palette
values blindly.

The new grayscale re-entry finding is separate from that visual issue: it explains
why a second grayscale pass changed deterministic hashes, not why the initial
hardware presentation looks less saturated than the J2ME reference.

## Current safe stop boundary

Validated and executed:

- complete engine startup through mappings
- real menu BSP structural data
- native asset-pack access
- zero resident `shapeData`
- zero map-wide `mediaTexels`
- bounded GFXRM wall/sprite frames and LRU caches
- native projected walls and BSP-sorted sprites
- deterministic native menu scene `ffe0995e`
- real `MENU_MAIN` model/assets and fitted 160x120 presentation
- calibrated physical touch hit-testing
- real `MenuSystem.selectedIndex` changes from touch
- independently movable real `p.bmp` hand cursor
- second released tap on same row detected as `CONFIRM`
- real `MenuSystem_select()` execution for confirmed Options
- real `MENU_MAIN_OPTIONS` model and bounded framebuffer `6058d47d`
- **real `MenuSystem_back()` execution from Options**
- **bit-identical reconstruction of MENU_MAIN after Back**
- **MENU_MAIN touch re-armed after the round trip**
- grayscale re-entry made deterministic/idempotent at the ESP32 boundary

Still intentionally out of scope:

- fast menu-scene restoration
- Video/Input/Sound actions
- Help/About and Exit real actions
- Start Game / gameplay loader activation
- original monolithic bitshape/texel loaders
- textured floor/ceiling planes
- active normal multi-frame engine/game loop
- final color/contrast correction
- normal gameplay controls
- audio

## Recommended next increment after merge

Before enabling more submenus, remove the 2.5-second "mini reboot" effect from
normal menu navigation.

Recommended objective:

> Replace the full `MENUWALL + MENUSPRITE` Back reconstruction with a bounded
> reusable representation of the already deterministic `ffe0995e` menu scene.

Do **not** allocate a second 38,400-byte framebuffer: the current largest free
8-bit block is only about 17 KB. First measure an exact compact representation
(RLE/indexed/other lossless bounded encoding) of the static menu scene and choose
a format from hardware data. Keep the current full native rerender as a recovery /
regression path, not as the eventual interactive navigation path.
