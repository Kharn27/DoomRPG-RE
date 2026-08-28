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

## Current normal new-game route

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
