# ESP32 documentation map

The ESP32 port no longer treats one Markdown file per milestone as its working
architecture. Recovery and development should start from this small set:

1. current GitHub `main` and its exact SHA;
2. [`PORTING_STATUS.md`](PORTING_STATUS.md) — current tested/candidate boundary;
3. [`ARCHITECTURE.md`](ARCHITECTURE.md) — permanent native engine design;
4. this file — build/layout/recovery pointers.

If repository state and chat history disagree, the repository wins. Serial logs
from the real classic CYD are the final runtime truth.

## Current branch

```text
main = 45d634449faa511dd02ab25ac5bb980fa4ef86b1
branch = agent/esp32-native-action-engine
base main = 45d634449faa511dd02ab25ac5bb980fa4ef86b1
hardware-tested final code HEAD = 1f88ca6488d78be181fc97b500beefbe0eb9a751
status = REAL-CYD PASS
merge-ready = YES after docs-only tail verification
```

This branch establishes a bounded native Action/Combat presentation frontier:

```text
empty/human Action -> native top-bar feedback
1200 ms feedback lease -> topbar-only clear
strict touch-feedback arbitration
trace -> enemy/destructible fail-closed routing
adjacent extinguisher -> native fire removal overlay + attack frame + idle settle
mutable line texture variants -> live door lock visual update
generic weapon frame cache keyed by weapon + animation frame
```

Still deliberately deferred:

```text
generic monster/destructible HP/damage combat
ammo/XP/sound/turn consequences
remote extinguisher miss/no-effect presentation
player-stat/inventory/ammo pickups
```

The exact hardware evidence, timings and known divergences are recorded in
[`PORTING_STATUS.md`](PORTING_STATUS.md).

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

`esp32-cyd-bringup` extends the normal environment with diagnostics. Bring-up
instrumentation can perturb memory, so its heap figures are not production
canons.

## Header/include hygiene

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
as `ESP32/include/esp_timer.h` therefore intercepts not only one native C file
but also Arduino -> FreeRTOS -> portmacro includes across unrelated `.cpp`
translation units.

For elapsed gameplay time, use an already-owned canonical clock such as
`DoomRPG_GetUpTimeMS()` when its semantics are sufficient instead of adding an
ESP-IDF timer dependency solely for millisecond timestamps.

## Linker-wrapper hygiene

Wrappers are a compatibility boundary, not a license for hidden recursive call
graphs.

Permanent rule:

```text
A linker wrapper must not call a higher-level API that can indirectly re-enter
that wrapped symbol.
```

Prefer:

```text
__real_* direct access
already-materialized immutable/mutable owner views
small explicitly non-reentrant helpers
```

Do not call convenience getters from a wrapper unless their complete call graph
is known not to return through the same wrapped symbol.

This rule was recovered from a real failure: the first mutable line-texture
renderer wrapper called a texture getter that internally called
`EspMapRuntime_getLine()` again, producing an infinite wrapper recursion until
the `loopTask` stack canary fired. The final code resolves the texture bit from
the already-owned line-texture view instead.

## Framebuffer owner arbitration

The logical framebuffer is shared by several bounded presenters. A temporary
visual owner that saves exact pixels must not have its snapshot invalidated by
another subsystem during its lease.

Current example:

```text
EspNativeGameplayControls_begin()
 -> saves exact touched pixels
 -> 120 ms control flash lease
 -> EspNativeGameplayControls_restore()
```

Action feedback expiry must therefore wait while the touch-control overlay is
active. Do not weaken the exact restore/FNV check to hide cross-owner writes;
arbitrate ownership instead.

## Source layout

Permanent/current source should be read by responsibility:

```text
ESP32/src/esp_map_*                       native map/runtime/event ownership
ESP32/src/esp_player_*                    native player/view ownership
ESP32/src/esp_native_gameplay_*           reusable live gameplay/action semantics
ESP32/src/esp_native_sprite_renderer.c    reusable sprite renderer
ESP32/src/esp_native_dynamic_line_render.c mutable line presentation overlay
ESP32/src/esp_asset_pack.cpp              native PAK backing store/cache
ESP32/src/esp_bsp_reader.c                BSP structural inventory/parser
ESP32/src/esp_native_startup.c            generic new-game resident/spawn bootstrap
ESP32/src/platform_*                      CYD input/video bridges
ESP32/src/native_intro_*                  bounded intro compatibility path
ESP32/src/native_main_menu_*              bounded menu compatibility path
```

Retained desktop compatibility startup remains explicit and bounded rather than
being treated as the permanent ESP32 architecture.

## Native data path

Map runtime backing store:

```text
/DoomRPG-ESP32.pak
```

Map loading must use the native pack plus compact resident owners. Do not add a
new map path that reads/decompresses the old ZIP into map-wide memory.

`/DoomRPG.zip` is still touched by transitional menu/HUD/bootstrap compatibility
code. That is explicit compatibility debt, not permission to move migrated BSP
or runtime data back to ZIP ownership.

## Action / Combat recovery notes

The recovered desktop SELECT behavior is specification input, not the permanent
ESP32 architecture:

```text
front tile event first
 -> trace up to 8 tiles with mask 0x5687
 -> human: do not attack
 -> destructible: weapon mask decides attack eligibility
 -> fire: extinguisher only
 -> enemy: attack
 -> otherwise: Nothing to use
```

Important nuance for fire: the trace can find a fire farther than one tile away,
and legacy `Player_fireWeapon()` still enters combat. The extinguisher has
`rangeMin=0`, so `CombatEntity_calcHit()` misses beyond squared distance 4096 and
the desktop combat path reports `No effect!` after the attack presentation.

Current ESP32 code intentionally does **not** fake that whole combat transaction.
Until generic miss/ammo/turn ownership exists:

```text
adjacent fire -> native FIRE_CLEARED transaction
remote fire   -> FIRE_RANGE_DEFERRED, mutation=no, attack presentation deferred
```

That is a documented temporary divergence, not final gameplay behavior.

## Pickup frontier

Current production pickup owner:

```text
eType=5 weapon
world remove = consumed-sprite bit overlay
ownership = native uint16 weapon mask
new weapon select = native HUD overlay
rollback = exact on redraw failure
```

Still deferred:

```text
eType=3 world/player-stat item
eType=4 inventory item
eType=6 ammo
eType=16 alternate ammo
weapon acquisition ammo/message/sound consequences
```

Do not implement helmets, health items or ammo boxes as one-off cases. Add a
small generic player-stat/inventory owner when that family becomes the chosen
milestone.

## Performance rule

Do not optimize `PlatformVideo_present()` just because the loaded room feels
slow. Current real-CYD timings in the first fire room show approximately:

```text
sprite phase  ~545 ms
world phase   ~178 ms
present       ~47 ms in full Action frame; raw VIDEO present commonly ~34 ms
full frame    ~843 ms
sprite reads  ~100 per full frame
```

The first bounded performance target is therefore:

```text
sprite renderer / resident sprite-asset cache audit
```

The milestone should explain the physical reads and remove avoidable repeated
sprite asset work while preserving exact visuals, bounded RAM and no-PSRAM
constraints. Do not add a map-wide texel pool to make the benchmark look good.

## Cleanup rule for probes

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

The cleanup goal is a legible future engine, not reduced observability.

## Hardware logging

Serial is the final hardware truth. Relevant stable tags now include:

```text
[NATIVEBOOT]
[ENGINESESSION]
[ENGINECACHE]
[RESIDENTGAMEPLAY]
[ACTION]
[ACTIONENGINE]
[ACTIONFEEDBACK]
[WEAPON]
[DOORANIM]
[DYNAMICLINES]
[DIALOGCHAIN]
[PICKUP]
[PICKUPCORPUS]
[INTERACTMAP]
```

When performance is under study, preserve per-phase measurements rather than
replacing them with only one aggregate frame time.

## Development workflow

For a new capability or optimization:

```text
recover exact legacy behavior/current consumer
 -> identify the permanent native owner/API
 -> choose one bounded reusable family
 -> add strict observability where needed
 -> fail closed outside supported cases
 -> commit/push agent/*
 -> build normal esp32-cyd
 -> test on real CYD
 -> use Serial as hardware truth
 -> document only observed results
 -> verify post-test commits are docs-only
 -> declare merge-ready
```

Never claim a local build or hardware pass that did not occur.

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

## After this branch merges

Do not continue from this branch by assumption. Re-read actual GitHub `main`,
record its exact merge SHA, then create the next `agent/*` branch from that SHA.

Current evidence strongly favors a bounded sprite-render/cache performance audit
as the next milestone, with player-stat pickups and generic combat remaining
separate correctness families.
