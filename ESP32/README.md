# Doom RPG ESP32-native port

This directory contains the classic **ESP32-2432S028R CYD / no-PSRAM** port of Doom RPG.

DoomRPG-RE desktop/J2ME code is an executable specification for original behavior and formats. The permanent ESP32 architecture is native, compact and data-driven.

Start here:

- [`PORTING_STATUS.md`](PORTING_STATUS.md) — exact current real-CYD hardware boundary, allocations and canonical fingerprints.
- [`ARCHITECTURE.md`](ARCHITECTURE.md) — permanent engine ownership and rules.
- [`NATIVE_ENGINE_RECOVERY.md`](NATIVE_ENGINE_RECOVERY.md) — short restart checklist for a new development session.
- [`DOCUMENTATION.md`](DOCUMENTATION.md) — build/test/documentation map.

## Target

```text
board       ESP32-2432S028R classic CYD
MCU         ESP32-D0WD-V3 dual core 240 MHz
flash       4 MB
PSRAM       none
display     ILI9341 320x240
input       XPT2046
storage     microSD
logical FB  160x120 RGB565 = 38400 B
```

Presentation is nearest-neighbor x2 to the physical panel.

Permanent migrated-world invariants:

```text
Render.shapeData == NULL
Render.mediaTexels == NULL
legacy Game.entities == 0
legacy Game.monsters == 0
```

## Build / flash

Normal hardware authority:

```bash
cd ESP32
pio run -e esp32-cyd -t clean
pio run -e esp32-cyd -t upload
pio device monitor -e esp32-cyd
```

Optional diagnostic profile:

```bash
pio run -e esp32-cyd-bringup -t clean
pio run -e esp32-cyd-bringup -t upload
pio device monitor -e esp32-cyd-bringup
```

Use `esp32-cyd` for canonical hardware validation and RAM observations. Bring-up diagnostics can change memory layout.

## Asset model

Native backing store:

```text
/DoomRPG-ESP32.pak
```

Build it from the reference archive with:

```bash
python3 ESP32/tools/build_asset_pack.py \
    /path/to/DoomRPG.zip \
    /path/to/DoomRPG-ESP32.pak
```

`/DoomRPG.zip` is still present for transitional legacy menu/core paths and recovery tooling. It is **not** the runtime architecture for migrated map/gameplay paths. Do not reintroduce ZIP or map-wide inflation where the PAK path already exists.

## Engine model

The permanent direction is:

```text
original Doom RPG data / behavior
        -> native parsers + catalogs
        -> compact immutable EspMapRuntime
        -> small explicit mutable owners
        -> EspPlayerView
        -> native events/gameplay
        -> native renderer/HUD/dialog/input
        -> 160x120 RGB565
        -> CYD x2 presentation
```

**A new BSP is data, not a new engine.** Loading another level must not create `native_mapN_*` modules, per-level renderers or another lifecycle ladder. New code is justified by a reusable behavior family or explicit native owner.

## Current main-menu routes

The CYD main menu contains, in display order:

```text
Start Game
Load Game
Options
Help/About
```

`Exit` from the original application model has no useful meaning on the
standalone device and is replaced by `Load Game`.

`Options` reuses the same finger-first 2x2 Doom-tech dashboard instead of
falling back to the thin J2ME rows:

```text
BACK  | VIDEO
INPUT | SOUND
```

`BACK` keeps the released double-tap confirmation and returns through the real
menu hierarchy. `VIDEO`, `INPUT` and `SOUND` remain deliberately subdued and
non-interactive until their settings backends are ported. This redesign builds
successfully and still awaits its real-CYD visual/touch pass.

The normal new-game route is:

The historical MAP1 startup probe ladder has been replaced by the generic bootstrap:

```text
Start Game
 -> bounded legacy intro compatibility path
 -> post-intro safe boundary
 -> EspMapCatalog
 -> EspMapResidentLifecycle_loadFromEmpty
 -> generic initial spawn owners
 -> EspNativeGameplaySession
 -> resident native gameplay service
```

The real CYD has hardware-proven this route into Entrance with movement, scientist dialog, regular-door traversal and an ENTER-triggered `EV_DIALOGNOBACK`, while preserving `shapeData == NULL` and `mediaTexels == NULL`.

The alternate resume route is:

```text
Load Game
 -> validate the one-slot native checkpoint
 -> release menu-only runtime
 -> rebuild the immutable BSP and restore V8 mutable owners
 -> configure the resumed EspNativeGameplaySession
 -> ST_PLAYING without replaying the intro
```

If the checkpoint is missing or invalid, the menu remains active and replaces
the selected row with a red `No Save` response. Both successful resume and the
no-save response are hardware-proven on the real CYD at `18c1cfb`.

## Current in-game HUB

The compact native HUB currently exposes:

```text
INV | WPN | STAT | SYS
```

- `INV` presents Notebook, carried items, Credits and keys as a centered
  previous/current/next card window.
- `WPN` presents the nine normal weapon IDs 0..8 as a complete 3x3 grid.
  Familiar IDs 9..11 remain excluded. Source BGR565 weapon palettes are
  converted to framebuffer RGB565 before drawing.
- `STAT` remains read-only and now uses a denser 3x5 information dashboard:
  slightly raised HP/Armor cards with intermediate 5x7 values and real
  proportional green/red health and blue armor rails, level/XP with progress
  bar, an aligned 2x2 attribute grid, and a footer that renders owned keys as
  green/true-yellow/blue/red key cards rather than exposing the internal
  bitmask. Only the tab remains touch-active.
  The firmware build passes; the refined typography still awaits its real-CYD
  visual check.
- `SYS` owns the dedicated one-slot checkpoint UI. SAVE and LOAD require a
  second SELECT/tap to confirm; missing checkpoints display `NO SAVE`. A
  successful SAVE closes the HUB immediately and queues `Game saved` in the
  gameplay message bar for about 1.2 seconds.

The HUB repaints its industrial title bar while active and reconstructs the
normal gameplay HUD when closing. Touch feedback is bounded and conflict-aware:
large checkpoint buttons fit the static edit owner, and newer pickup, message
or viewport-flash overlays win if they touch the same framebuffer pixels.
The lower HUD band is the close-time integrity boundary; the top message band
is deliberately recomposed by the following world frame. When `Game saved`
expires, the normal priority chain restores a permanent status message first,
otherwise the current facing-entity label, otherwise an empty bar.

## Source-tree rule

Permanent production code should describe responsibility, not the map on which it was discovered.

Good examples:

```text
esp_map_*
esp_player_*
esp_native_gameplay_*
esp_native_*_renderer
platform_*
```

Historical MAP1/Junction/Entrance probes are not production architecture. Once their behavior is owned by generic modules and hardware-proven, Git history is their archive.

Some transitional menu/intro/legacy wrapper names remain. They should be removed only when their replacement path is explicit and hardware-tested, not by broad cosmetic deletion.

## Workflow

1. Recover exact current `main` SHA and read `PORTING_STATUS.md` + `ARCHITECTURE.md`.
2. Implement one bounded reusable objective.
3. Commit and push on `agent/*`.
4. Flash normal `esp32-cyd` on the real CYD.
5. Serial output is the hardware truth.
6. Fix failures on the same branch without broadening scope.
7. After PASS, document the observed boundary and retire temporary scaffolding.
8. Never merge into `main` without an explicit request.
