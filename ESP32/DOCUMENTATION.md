# ESP32 documentation map

The ESP32 port no longer treats one Markdown file per milestone as its working
architecture. Recovery and development should start from this small set:

1. current GitHub `main` and its exact SHA;
2. [`PORTING_STATUS.md`](PORTING_STATUS.md) — current tested/candidate boundary;
3. [`ARCHITECTURE.md`](ARCHITECTURE.md) — permanent native engine design;
4. this file — build/layout/recovery pointers.

If repository state and chat history disagree, the repository wins.

## Current branch

```text
latest merged main = d0643defd772f83fba07e171950a70b104bbeb6f
latest merged PR = #107 — ESP32 native renderer naming cleanup
branch = agent/esp32-render-startup-bridge
base main = d0643defd772f83fba07e171950a70b104bbeb6f
hardware-tested final code HEAD = 88dc75f9f8e9c7ccd972bf66439eb3dd65b29127
status = REAL-CYD PASS
merge-ready = YES
```

This branch is a bounded compatibility naming cleanup. The retained CYD
`Render_startup()` path is now owned by the permanent API:

```text
ESP32/include/esp_render_startup_bridge.h
ESP32/src/render_startup_bridge.c
EspRenderStartupBridge_start()
```

The historical `render_startup_probe.h` header is removed. `main.cpp` and the
bridge implementation use the permanent API directly. The bridge behavior did
not change: it reuses PlatformVideo's 160x120 RGB565 framebuffer, leaves desktop
`piDIB` absent, initializes retained sintable/palette state, and keeps the
linker wrappers needed by the legacy Render compatibility shell.

Real-CYD PASS was observed on the normal `esp32-cyd` firmware after the final
shim removal. The boot reached:

```text
PRERENDER=ready
RENDER=ready
MAPPINGS=ready
MENUBSP=ready
[NATIVEBOOT] READY ... shapeData=0x0 mediaTexels=0x0
[ENGINESESSION] READY map=1 ...
heap=92816 heap8=27052 largest8=16372
```

No visible/apparent `FAILED`, panic or reboot was reported. All commits after
`88dc75f9f8e9c7ccd972bf66439eb3dd65b29127` are documentation-only.

## Build environments

Normal reference environment:

```text
pio run -e esp32-cyd
```

This is the hardware authority for normal RAM/runtime behavior.

Optional diagnostic environment:

```text
pio run -e esp32-cyd-bringup
```

Bring-up enables extra diagnostics and can perturb memory. Do not use its heap
figures as production canons.

## Source layout

Permanent source should be read by responsibility:

```text
ESP32/src/esp_map_*                native map/runtime/event ownership
ESP32/src/esp_player_*             native player/view ownership
ESP32/src/esp_native_gameplay_*    reusable live gameplay semantics
ESP32/src/esp_native_*renderer*    reusable rendering paths
ESP32/src/esp_render_startup_bridge.c retained Render compatibility startup
ESP32/src/esp_asset_pack.cpp       native PAK backing store/cache
ESP32/src/esp_bsp_reader.c         BSP structural inventory/parser
ESP32/src/esp_native_startup.c     generic new-game resident/spawn bootstrap
ESP32/src/platform_*               CYD input/video bridges
ESP32/src/native_intro_*           bounded intro compatibility path
ESP32/src/native_main_menu_*       bounded menu compatibility path
```

The active sprite renderer is fully generic by filename, header, stats type and
exported function:

```text
ESP32/src/esp_native_sprite_renderer.c
ESP32/include/esp_native_sprite_renderer.h
EspNativeSpriteStats
EspNativeSpriteRenderer_render()
```

Historical MAP1/Entrance/Junction probe ladders and the Junction renderer naming
have been retired from the active engine. Git history remains the detailed
archaeological record.

## Native data path

Map runtime backing store:

```text
/DoomRPG-ESP32.pak
```

Map loading must use the native pack plus compact resident owners. Do not add a
new map path that reads/decompresses the old ZIP into map-wide memory.

`/DoomRPG.zip` is still touched by transitional legacy menu/HUD/bootstrap code in
`main.cpp`, and the retained render-startup bridge still reaches legacy resource
loading for `sintable.bin` / `palettes.bin`. This is explicit compatibility debt,
not permission to move migrated BSP/runtime data back to ZIP ownership.

## Hardware logging

Serial is the final hardware truth. Stable production tags include:

```text
[NATIVEBOOT]
[ENGINESESSION]
[ENGINECACHE]
[RESIDENTGAMEPLAY]
[ACTION]
[DIALOGCHAIN]
[PICKUP]
[PICKUPCORPUS]
[INTERACTMAP]
[RESIDENTRESET]
[RENDERSTART]
```

Historical one-probe-per-milestone transcripts are not required for normal
startup. Failures should be emitted by the permanent owner that actually owns
the failed boundary.

## Development workflow

For a new capability:

```text
recover exact legacy behavior
 -> choose one bounded reusable semantic family
 -> implement the permanent native owner/API
 -> temporary strict probe only when it adds real evidence
 -> fail closed outside supported cases
 -> commit/push agent/*
 -> build normal esp32-cyd
 -> test on real CYD
 -> document only observed results
 -> retire temporary probe after permanent integration
```

A probe should have an exit plan before it is added.

## New-level rule

A new BSP should normally require:

```text
catalog/data recognition only
0 map-specific allocators
0 map-specific renderers
0 native_mapN_* gameplay files
```

When another map exposes new behavior, implement the behavior once in a generic
module and use that map as another regression corpus.

## Known transitional cleanup items

See `ARCHITECTURE.md` for the full design. Remaining visible debt is intentionally
explicit:

```text
legacy ZIP-backed menu/HUD/bootstrap resources in main.cpp
pre_render_probe.* still names a real transitional pre-render startup stage
config_mappings_probe.* still names a live compatibility stage
menu_bsp_probe.* and related menu graphics probes remain transitional
native menu sprite/overlay wrappers still carry probe symbols
live CHANGEMAP/save/password/key/automap promotion incomplete
combat/monsters/player-stat pickup families incomplete
```

Do not mass-delete these. For each family, identify the real production consumer,
promote or replace it with a permanent owner when appropriate, hardware-prove
the resulting normal firmware, then retire only the obsolete compatibility
surface.

## After this branch merges

Do not continue from this branch by assumption. Re-read actual GitHub `main`,
record its exact merge SHA, then create the next `agent/*` branch from that SHA.

The natural next cleanup is another single `*probe*` compatibility family. The
pre-render startup stage is a plausible next audit because it is directly upstream
of the now-permanent render-startup bridge, but its behavior must be inspected
before any rename/removal.
