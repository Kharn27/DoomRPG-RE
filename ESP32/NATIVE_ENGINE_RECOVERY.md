# Doom RPG ESP32-native recovery checklist

This file is the short restart contract for future work on the classic CYD port.
It deliberately does **not** retell the historical MAP1 probe sequence.

## Recover from the repository, not chat memory

At the start of a new session or after a long interruption:

1. read the exact current GitHub `main` SHA;
2. read [`PORTING_STATUS.md`](PORTING_STATUS.md) for the last real-CYD hardware boundary and canonical fingerprints;
3. read [`ARCHITECTURE.md`](ARCHITECTURE.md) for permanent ownership and design rules;
4. read [`DOCUMENTATION.md`](DOCUMENTATION.md) for build/test/documentation workflow;
5. inspect the current production source only for the subsystem being changed.

If any conversation, old milestone note or remembered SHA disagrees with the repository, the repository wins.

## Permanent engine rule

**A new BSP is data, not a new engine.**

The runtime direction is:

```text
/DoomRPG-ESP32.pak
 -> native parsers/catalogs
 -> compact immutable EspMapRuntime
 -> small explicit mutable owners
 -> EspPlayerView
 -> native event/gameplay semantics
 -> native renderer/HUD/dialog/input
```

Do not create `native_map2_*`, `native_map3_*`, Entrance-specific or Junction-specific gameplay pipelines. If another map exposes an unsupported behavior, recover that behavior from the legacy executable specification and implement the reusable family once.

## Hardware / memory invariants

```text
board       = ESP32-2432S028R classic CYD
MCU         = ESP32-D0WD-V3 dual core 240 MHz
flash       = 4 MB
PSRAM       = none
framebuffer = 160x120 RGB565 = 38400 B
shapeData   = NULL
mediaTexels = NULL
native pack = /DoomRPG-ESP32.pak
```

For migrated native world paths, legacy `Game.entities` and `Game.monsters` remain empty. Prefer compact immutable arenas, indices/offsets, bounded caches and lazy/small mutable owners.

Runtime ZIP access is transitional compatibility only for paths not yet migrated. Never reintroduce ZIP/map-wide inflation into a path already backed by the native PAK.

## Current startup authority

Historical MAP1/Entrance/Junction lifecycle probes are no longer startup prerequisites.

The permanent new-game route is composed by `esp_native_startup.c`:

```text
post-intro safe boundary
 -> EspMapCatalog
 -> EspBspReader_inventoryPackEntry
 -> EspMapResidentLifecycle_loadFromEmpty
 -> EspPlayerSpawn_prepareInitial
 -> EspPlayerView + post-spawn owners
 -> EspNativeGameplayDispatch_adoptView
 -> EspNativeGameplaySession
```

A different resident BSP must enter the same engine through the same generic owners.

## Current gameplay architecture

The hardware-proven resident session owns generic rendering, HUD, cache, collision, touch input, TURN/MOVE/SELECT, regular doors and native dialog presentation/resume. Exact supported/deferred opcode families and pickup boundaries are maintained in `PORTING_STATUS.md` rather than duplicated here.

The engine does not broad-enable legacy `Game_executeEvent()`. Unsupported semantic families remain fail-closed until their native owner/consumer exists.

## Development workflow

For every new bounded behavior family:

```text
recover exact legacy behavior
 -> design reusable native API/owner
 -> add a strict temporary probe only if needed
 -> keep unsupported cases fail-closed
 -> commit + push agent/*
 -> build/flash normal esp32-cyd
 -> treat Serial logs as hardware truth
 -> fix on the same branch if needed
 -> document only observed PASS results
 -> retire temporary probe scaffolding once production integration is proven
```

Do not use bring-up RAM as a canonical memory figure.

## Historical evidence

Detailed MAP1/Junction probe transcripts and milestone files are archaeology, not active documentation. Once their knowledge is synthesized into `ARCHITECTURE.md` / `PORTING_STATUS.md`, Git history is the archive.

When an exact old witness is genuinely needed, recover it from the commit/PR history instead of restoring the historical probe ladder to the production tree.
