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
latest merged main = 439d963c39d1dc6435ee9bcd91b3292fc5b59c53
branch = agent/esp32-native-renderer-naming
base main = 439d963c39d1dc6435ee9bcd91b3292fc5b59c53
hardware-tested final code HEAD = cc2a8c6e7baa6c1267cde2629feead68cb2602de
status = REAL-CYD PASS
merge-ready = YES
```

This branch is a bounded naming cleanup only. It converts the historical
Junction-named sprite renderer implementation/header/API into the permanent
map-generic renderer naming, with no intended rendering-behavior change.

Hardware checkpoints on the normal `esp32-cyd` firmware covered the initial
physical file rename, the generic API promotion, and the final removal of the
Junction compatibility header. On the final code HEAD the reported hardware
behavior remained unchanged with no visible/apparent `FAILED` condition.

All commits after `cc2a8c6e7baa6c1267cde2629feead68cb2602de` are documentation-only.

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

The active sprite renderer is now fully generic by filename, header, stats type
and exported render function:

```text
ESP32/src/esp_native_sprite_renderer.c
ESP32/include/esp_native_sprite_renderer.h
EspNativeSpriteStats
EspNativeSpriteRenderer_render()
```

The historical `esp_native_junction_sprite_renderer.*` source/header/API has
been retired from the active tree. Junction remains only a regression data corpus,
not a renderer architecture.

The older `native_map1_*`, Junction and Entrance validation ladders were already
physically removed by the merged native engine cleanup. They are not production
prerequisites and must not be reintroduced for a new level. Git history remains
the detailed archaeology.

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

The old `MAP1_*.md` and retired probe sources are historical evidence. Recover
any exact old transcript/FNV/implementation from Git history around the relevant
PR/commit rather than restoring those files to the production architecture.

The durable conclusions and currently useful canons have been promoted into
`ARCHITECTURE.md` and `PORTING_STATUS.md`.

## Known transitional cleanup items

See `ARCHITECTURE.md` for the full design. The most visible remaining tree debt
is intentionally explicit:

```text
legacy ZIP-backed menu/HUD startup in main.cpp
some active legacy-wrapper entry points still named *probe*
live CHANGEMAP/save/password/key/automap promotion incomplete
combat/monsters/player-stat pickup families incomplete
```

The historical Junction renderer naming item is complete and should not return.
Fix the remaining debt by moving ownership into permanent generic modules, not by
introducing new level-specific bridges.

## After this branch merges

Do not continue from this branch by assumption. Re-read the actual GitHub `main`,
record its exact merge SHA, then create the next `agent/*` branch from that SHA.

The natural next cleanup is a bounded audit of one remaining active `*probe*`
compatibility family at a time. Do not mass-delete those modules: determine the
current production consumer, establish a permanent generic replacement/owner,
prove it on the normal firmware when behavior changes, then retire only the
obsolete compatibility surface.
