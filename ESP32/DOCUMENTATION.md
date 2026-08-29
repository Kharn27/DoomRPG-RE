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
latest merged main = fc6a271490bd06dad4ae8264ba106281864b53e1
latest merged PR = #109 — ESP32 legacy pre-render startup cleanup
branch = agent/esp32-legacy-config-mappings-startup
base main = fc6a271490bd06dad4ae8264ba106281864b53e1
PASS 1 physical implementation rename = 5e678d49610a98bdc0600b9af640f05b1ce2dd9b
hardware-tested final code HEAD = 571afd1b0665e74ad15f8ae5365433041c55a23e
status = REAL-CYD PASS
merge-ready = YES
```

The config/mappings compatibility stage now has a permanent responsibility-based
API:

```text
ESP32/src/esp_legacy_config_mappings_startup.c
ESP32/include/esp_legacy_config_mappings_startup.h
EspLegacyConfigMappingsStartup_start()
```

It still performs exactly the retained startup sequence:

```text
Game_loadConfig()
 -> inspect mappings.bin sizing against current heap
 -> Render_loadMappings()
 -> validate resident mapping arrays/counts
 -> stop before Render_beginLoadMap()/BSP
```

The separate `esp_render_mapping_reload_guard.c` remains untouched. Its bounded
job is still to release the four mapping arrays immediately before the real
`Render_beginLoadMap()` reload so the classic CYD does not carry both old tables
and mapping inflate state at the same time.

The real-CYD normal `esp32-cyd` firmware passed both cleanup stages. Final code
HEAD `571afd1b...` continued through intro disposal and the native Entrance
session with:

```text
[NATIVEBOOT] READY map=1 gameplayLoadMapId=1 ... shapeData=0x0 mediaTexels=0x0
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430
[ENGINECACHE] OWNER bytes=21160 payload=16384 entries=256
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap=92816 heap8=27052 largest8=16372
PRERENDER=ready RENDER=ready MAPPINGS=ready MENUBSP=ready
```

No visible/apparent `FAILED`, panic or reboot was reported. All commits after
`571afd1b0665e74ad15f8ae5365433041c55a23e` are documentation-only.

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

`esp32-cyd-bringup` extends the normal environment and adds diagnostic compile
flags including `DOOMRPG_ESP32_BRINGUP_PROBES=1` and the touch hitbox overlay.
Bring-up instrumentation can perturb memory, so its heap figures are not
production canons.

### Header/include hygiene

The recovered legacy headers predate modern ESP-IDF/Arduino include conventions.
In particular, `src/DoomRPG.h` declares the historical C type:

```c
typedef enum { false, true } boolean;
```

That collides with `stdbool` macros in C and with C++ language keywords if a
framework header is forced through the wrong compatibility path. Treat this as a
build-boundary invariant, not as an invitation to patch the legacy executable
specification.

Rules for native ESP32 code:

```text
prefer existing project clocks/APIs over importing a new ESP-IDF header
keep SDL/legacy include order explicit when a C unit needs DoomRPG.h/Render.h
never shadow ESP-IDF/Arduino framework headers in ESP32/include for a local fix
never inject DoomRPG.h from a framework-shadow header or transitive C++ path
if an include collision appears, fix the owning translation unit or add a
narrow project-named adapter; do not globally intercept framework includes
```

Why this is strict: `ESP32/include` precedes framework include paths. A file such
as `ESP32/include/esp_timer.h` would therefore intercept not only one native C
file but also Arduino -> FreeRTOS -> portmacro includes across unrelated `.cpp`
translation units. A local workaround can consequently break the whole build.

For elapsed gameplay time, use an already-owned canonical clock such as
`DoomRPG_GetUpTimeMS()` when its semantics are sufficient instead of adding an
ESP-IDF timer dependency solely for millisecond timestamps.

### Cleanup rule for probes

Do **not** remove or rename a file solely because its name contains `probe`.
Classify it first:

```text
always-built production implementation with known permanent responsibility
    -> promote to a permanent owner/API and retire obsolete probe surface

bringup-only diagnostic / regression witness / bounded instrumentation
    -> keep while useful, even if probe-named

unclear consumer or linker wrapper
    -> audit references/build flags first; no blind deletion
```

The cleanup goal is a legible future engine for agents, not reduced observability.
The normal `esp32-cyd` path should not depend on historical probe naming once a
permanent owner exists, while `esp32-cyd-bringup` may intentionally retain probes
that accelerate diagnosis.

## Source layout

Permanent/current source should be read by responsibility:

```text
ESP32/src/esp_map_*                       native map/runtime/event ownership
ESP32/src/esp_player_*                    native player/view ownership
ESP32/src/esp_native_gameplay_*           reusable live gameplay semantics
ESP32/src/esp_native_*renderer*           reusable rendering paths
ESP32/src/esp_legacy_prerender_startup.c  retained legacy pre-Render startup
ESP32/src/render_startup_bridge.c         retained Render compatibility startup
ESP32/src/esp_legacy_config_mappings_startup.c retained config/mapping startup
ESP32/src/esp_render_mapping_reload_guard.c bounded retained mapping reload guard
ESP32/src/esp_asset_pack.cpp              native PAK backing store/cache
ESP32/src/esp_bsp_reader.c                BSP structural inventory/parser
ESP32/src/esp_native_startup.c            generic new-game resident/spawn bootstrap
ESP32/src/platform_*                      CYD input/video bridges
ESP32/src/native_intro_*                  bounded intro compatibility path
ESP32/src/native_main_menu_*              bounded menu compatibility path
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

The retained desktop-derived startup sequence is being made explicit one bounded
family at a time rather than hidden or deleted wholesale:

```text
DoomCanvas/layout startup
 -> EspLegacyPrerenderStartup_start()
      -> ParticleSystem_startup()
      -> MenuSystem_startup()
      -> EntityDef_startup()
 -> EspRenderStartupBridge_start()
 -> EspLegacyConfigMappingsStartup_start()
      -> Game_loadConfig()
      -> Render_loadMappings()
 -> menu BSP/menu compatibility stage
```

These compatibility owners do not make ZIP-backed data part of the desired
native map architecture. They make retained dependencies explicit so they can be
replaced safely later.

## Native data path

Map runtime backing store:

```text
/DoomRPG-ESP32.pak
```

Map loading must use the native pack plus compact resident owners. Do not add a
new map path that reads/decompresses the old ZIP into map-wide memory.

`/DoomRPG.zip` is still touched by transitional menu/HUD/bootstrap compatibility
code. `EspLegacyPrerenderStartup`, `EspRenderStartupBridge` and
`EspLegacyConfigMappingsStartup` still reach bounded legacy resources. This is
explicit compatibility debt, not permission to move migrated BSP/runtime data
back to ZIP ownership.

## Hardware logging

Serial is the final hardware truth. Stable production/compatibility tags include:

```text
[PRERENDER]
[RENDERSTART]
[CONFIG]
[MAPPINGS]
[CONFIGMAP]
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
startup. Bringup-only diagnostics are different: they may remain intentionally
available if they provide useful failure localization or visual/serial evidence.

## Development workflow

For a new capability or cleanup:

```text
recover exact legacy behavior/current consumer
 -> classify production vs bringup/regression instrumentation
 -> choose one bounded reusable semantic/compatibility family
 -> implement or identify the permanent owner/API
 -> temporary strict probe only when it adds real evidence
 -> fail closed outside supported cases
 -> commit/push agent/*
 -> build normal esp32-cyd
 -> test on real CYD
 -> document only observed results
 -> retire only obsolete production probe/shim surface
```

A probe should have an exit plan when it is scaffolding. A deliberately retained
bringup diagnostic instead needs a clear diagnostic purpose and build scope.

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

Remaining visible debt is intentionally explicit:

```text
legacy ZIP-backed menu/HUD/bootstrap resources in main.cpp and retained bridges
menu BSP/menu graphics compatibility still requires audit
native menu sprite/overlay wrappers still carry probe symbols
live CHANGEMAP/save/password/key/automap promotion incomplete
combat/monsters/player-stat pickup families incomplete
```

`pre_render_probe.*` and `config_mappings_probe.*` are no longer active production
naming debt. Their live behavior is owned by permanent compatibility APIs.

Do not mass-delete the remaining probe-named families. For each one, first check
whether it is part of normal `esp32-cyd`, diagnostic-only `esp32-cyd-bringup`, a
linker compatibility wrapper, or still-needed regression instrumentation. Promote
or retire only the obsolete production surface.

## After this branch merges

Do not continue from this branch by assumption. Re-read actual GitHub `main`,
record its exact merge SHA, then create the next `agent/*` branch from that SHA.

A plausible next audit is the menu BSP/menu graphics compatibility area, but it
is **not** pre-approved for removal. Inspect production consumers, bringup flags,
wrappers and diagnostic value before choosing the next bounded milestone.
