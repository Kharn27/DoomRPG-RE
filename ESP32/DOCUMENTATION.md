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
base main = b6bb8f99c61afdd61f4a17c079f64c7d540aede6
branch = agent/esp32-native-engine-cleanup
hardware-tested generic-startup SHA = 5d78c65ec2e0fbeeba3db6f93038eab288bc3354
production-build pruning SHA = eaf6c935c4e3a086e920b57f78f77bd5b39f7fa8
build-pruning status = awaiting real-CYD validation
```

The architecture/status documentation after `eaf6c935` is docs-only. Exact
hardware observations belong in `PORTING_STATUS.md` after a real run.

## Build environments

Normal reference environment:

```text
pio run -e esp32-cyd
```

This is the only environment whose RAM/runtime behavior is used as the normal
hardware authority.

Optional diagnostic environment:

```text
pio run -e esp32-cyd-bringup
```

Bring-up defines additional diagnostics and can perturb memory. Do not use its
heap figures as production canons.

## Source layout

Permanent source should be read by responsibility:

```text
ESP32/src/esp_map_*                native map/runtime/event ownership
ESP32/src/esp_player_*             native player/view ownership
ESP32/src/esp_native_gameplay_*    reusable live gameplay semantics
ESP32/src/esp_native_*renderer*    reusable rendering paths
ESP32/src/esp_asset_pack.cpp       native PAK backing store/cache
ESP32/src/esp_bsp_reader.c         BSP structural inventory/parser
ESP32/src/esp_native_startup.c     generic new-game resident/spawn bootstrap
ESP32/src/platform_*               CYD input/video bridges
ESP32/src/native_intro_*           bounded intro compatibility path
ESP32/src/native_main_menu_*       bounded menu compatibility path
```

Historical `native_map1_*`, Junction and Entrance validation ladders are no
longer production prerequisites. The current cleanup candidate removes those
translation units from the normal build. After hardware validation they can be
physically removed from the active tree; Git history remains the detailed
archive.

## Native data path

Map runtime backing store:

```text
/DoomRPG-ESP32.pak
```

Map loading must use the native pack plus compact resident owners. Do not add a
new map path that reads/decompresses the old ZIP into map-wide memory.

`/DoomRPG.zip` is still touched by transitional legacy menu/HUD bootstrap code in
`main.cpp`. That is explicit technical debt, not the desired native map/storage
architecture. Remove it only when the affected menu/HUD resources have a clean
PAK-native startup owner; do not create a second parallel loader just to hide the
dependency.

## Hardware logging

Serial is the final hardware truth. Useful stable production tags now include:

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

When a later map exposes new behavior, implement the behavior once in a generic
module and use that map as another regression corpus.

## Recovery of deleted milestone detail

The old `MAP1_*.md` and probe sources are historical evidence. Once this cleanup
physically removes them, recover any exact old transcript/FNV/implementation from
Git history around the relevant PR/commit rather than restoring the files to the
production architecture.

The durable conclusions and currently useful canons have been promoted into
`ARCHITECTURE.md` and `PORTING_STATUS.md`.

## Known transitional cleanup items

See `ARCHITECTURE.md` for the full design. The most visible remaining tree debt
is intentionally explicit:

```text
legacy ZIP-backed menu/HUD startup in main.cpp
some active legacy-wrapper entry points still named *probe*
historical Junction name in part of the generic sprite renderer implementation
live CHANGEMAP/save/password/key/automap promotion incomplete
combat/monsters/player-stat pickup families incomplete
```

Fix these by moving ownership into permanent generic modules, not by introducing
new level-specific bridges.
