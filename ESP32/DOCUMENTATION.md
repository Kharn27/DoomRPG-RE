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
latest merged main = d65621cb0c308d648c4b578f2a474aee3cc481a4
latest merged PR = #108 — ESP32 render startup bridge cleanup
branch = agent/esp32-legacy-prerender-startup
base main = d65621cb0c308d648c4b578f2a474aee3cc481a4
hardware-tested final code HEAD = e55465c3f0d93930c7946c1293a1e7ac4f149aae
status = REAL-CYD PASS
merge-ready = YES after docs-only finalization
```

This branch is a bounded compatibility naming cleanup. The live stage that sits
between the validated DoomCanvas/layout startup and `EspRenderStartupBridge` is
not native gameplay; it is retained legacy bootstrap ownership for three systems:

```text
ParticleSystem_startup()
MenuSystem_startup()
EntityDef_startup()
```

It is now exposed as:

```text
ESP32/src/esp_legacy_prerender_startup.c
ESP32/include/esp_legacy_prerender_startup.h
EspLegacyPrerenderStartup_start()
```

The historical `pre_render_probe.c/.h` naming is retired. The implementation
still preflights `gibs_24.bmp`, `p.bmp`, `q.bmp`, `j.bmp` and `entities.db` from
the legacy ZIP compatibility path and stops before Render startup.

The real-CYD normal `esp32-cyd` firmware passed both cleanup stages. Final code
HEAD `e55465c3...` reached the native Entrance gameplay session with stable
resident cache ownership and heartbeat:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430
[ENGINECACHE] OWNER bytes=21160 payload=16384 entries=256
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap=92816 heap8=27052 largest8=16372
PRERENDER=ready RENDER=ready MAPPINGS=ready MENUBSP=ready
```

No visible/apparent `FAILED`, panic or reboot was reported. All commits after
`e55465c3f0d93930c7946c1293a1e7ac4f149aae` must remain documentation-only.

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

Permanent/current source should be read by responsibility:

```text
ESP32/src/esp_map_*                    native map/runtime/event ownership
ESP32/src/esp_player_*                 native player/view ownership
ESP32/src/esp_native_gameplay_*        reusable live gameplay semantics
ESP32/src/esp_native_*renderer*        reusable rendering paths
ESP32/src/esp_legacy_prerender_startup.c retained legacy startup bridge
ESP32/src/render_startup_bridge.c      retained Render compatibility startup
ESP32/src/esp_asset_pack.cpp           native PAK backing store/cache
ESP32/src/esp_bsp_reader.c             BSP structural inventory/parser
ESP32/src/esp_native_startup.c         generic new-game resident/spawn bootstrap
ESP32/src/platform_*                   CYD input/video bridges
ESP32/src/native_intro_*               bounded intro compatibility path
ESP32/src/native_main_menu_*           bounded menu compatibility path
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

## Startup compatibility sequence

The retained desktop-derived startup sequence is being promoted one bounded
family at a time rather than hidden or deleted wholesale. The currently explicit
sequence is:

```text
DoomCanvas/layout startup
 -> EspLegacyPrerenderStartup_start()
      -> ParticleSystem_startup()
      -> MenuSystem_startup()
      -> EntityDef_startup()
 -> EspRenderStartupBridge_start()
 -> config/mappings compatibility stage
 -> menu BSP/menu compatibility stage
```

The first two compatibility owners now have permanent non-probe APIs. This does
not make their ZIP-backed resources part of the desired native map architecture;
it makes the remaining legacy dependency explicit and bounded.

## Native data path

Map runtime backing store:

```text
/DoomRPG-ESP32.pak
```

Map loading must use the native pack plus compact resident owners. Do not add a
new map path that reads/decompresses the old ZIP into map-wide memory.

`/DoomRPG.zip` is still touched by transitional menu/HUD/bootstrap compatibility
code. In particular, `EspLegacyPrerenderStartup` still reaches the five legacy
resources listed above and `EspRenderStartupBridge` still reaches
`sintable.bin` / `palettes.bin`. This is explicit compatibility debt, not
permission to move migrated BSP/runtime data back to ZIP ownership.

## Hardware logging

Serial is the final hardware truth. Stable production/compatibility tags include:

```text
[PRERENDER]
[RENDERSTART]
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

For a new capability or cleanup:

```text
recover exact legacy behavior/current consumer
 -> choose one bounded reusable semantic/compatibility family
 -> implement or identify the permanent owner/API
 -> temporary strict probe only when it adds real evidence
 -> fail closed outside supported cases
 -> commit/push agent/*
 -> build normal esp32-cyd
 -> test on real CYD
 -> document only observed results
 -> retire obsolete probe/shim after permanent integration
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
legacy ZIP-backed menu/HUD/bootstrap resources in main.cpp and retained bridges
config_mappings_probe.* still names a live compatibility stage
menu_bsp_probe.* and related menu graphics probes remain transitional
native menu sprite/overlay wrappers still carry probe symbols
live CHANGEMAP/save/password/key/automap promotion incomplete
combat/monsters/player-stat pickup families incomplete
```

`pre_render_probe.*` is no longer active debt: it was replaced by
`EspLegacyPrerenderStartup_start()` and physically removed.

Do not mass-delete the remaining probe-named families. For each one, identify the
real production consumer, promote or replace it with a permanent owner when
appropriate, hardware-prove the resulting normal firmware, then retire only the
obsolete compatibility surface.

## After this branch merges

Do not continue from this branch by assumption. Re-read actual GitHub `main`,
record its exact merge SHA, then create the next `agent/*` branch from that SHA.

The natural next cleanup candidate is `config_mappings_probe.*`, because it is
directly downstream of the now-permanent pre-render and Render startup bridges.
Audit its exact config/mappings ownership and linker/resource behavior before any
rename or removal.
