# Doom RPG ESP32-native architecture

This document is the permanent architecture reference for the classic
ESP32-2432S028R CYD port. It describes the engine we are keeping, not the order
in which historical recovery probes were written.

For current hardware status and exact canons, read [`PORTING_STATUS.md`](PORTING_STATUS.md).
For build/platform notes, read [`DOCUMENTATION.md`](DOCUMENTATION.md).

## 1. Design rule

**A new BSP is data, not a new engine.**

The desktop/J2ME reverse-engineered source is an executable specification for
Doom RPG behavior and formats. It is not the permanent ESP32 memory model.

The target pipeline is:

```text
original Doom RPG data / recovered behavior
        |
        v
/DoomRPG-ESP32.pak
        |
        v
ESP32-native parsers + immutable catalogs
        |
        v
compact immutable EspMapRuntime
        |
        +--> small explicit mutable map owners
        |
        +--> native event/script semantics
        |
        +--> EspPlayerView + gameplay session
        |
        +--> native renderer / HUD / dialog / input
        |
        v
160x120 RGB565 framebuffer -> nearest-neighbor x2 -> ILI9341
```

Adding level 3, 4, ... must not create `native_map3_*`, `native_map4_*`, or
another level-specific renderer. A new source module is justified only by a new
**behavior family**, storage format, renderer primitive, or explicit owner that
is reusable by every map that needs it.

## 2. Hardware and memory contract

Target:

```text
board       ESP32-2432S028R classic CYD
MCU         ESP32-D0WD-V3, dual core, 240 MHz
flash       4 MB
PSRAM       none
display     ILI9341 320x240
logical FB  160x120 RGB565 = 38400 B
input       XPT2046 touch
storage     microSD
```

Permanent invariants:

```text
Render.shapeData == NULL
Render.mediaTexels == NULL
legacy Game.entities == 0 for the migrated native world
legacy Game.monsters == 0 for the migrated native world
```

Forbidden permanent designs:

- map-wide decoded `shapeData`;
- map-wide `mediaTexels`;
- large pointer-heavy copies of the desktop world;
- runtime map loading from `DoomRPG.zip` after a path has been migrated to the
  native pack;
- one implementation per level;
- broad fallback to legacy `Game_executeEvent()` to bypass unsupported native
  semantics.

Preferred ownership:

- immutable compact arenas;
- offsets/indices instead of pointer graphs;
- bounded caches backed by `/DoomRPG-ESP32.pak`;
- small fixed or lazy mutable owners;
- transactional prepare/commit/rollback when rendering or script execution can
  fail after a candidate mutation.

## 3. Map identity and loading

`esp_map_catalog.*` is the permanent Doom RPG map identity table. Native map IDs
mirror the recovered game IDs and resolve to the BSP resource names stored in
the PAK.

The permanent resident loader is:

```text
EspBspReader_inventoryPackEntry(resource)
        |
        v
EspMapResidentLifecycle_loadFromEmpty(resource, inventory)
        |
        +--> EspMapRuntime
        +--> EspMapState
        +--> EspMapScriptState
        +--> EspMapLineState
        +--> EspMapLineTextureState
        +--> EspMapAutomapState
        +--> EspMapSpriteTopology
```

`EspMapRuntime` is immutable after publication. Runtime records are accessed by
indices through allocation-free accessors. Mutable game changes never rewrite
that arena; they live in the owners listed above.

`EspMapResidentLifecycle_capture()` gives a compact snapshot useful for hardware
regression evidence. FNVs are validation witnesses, not dispatch logic.
Production behavior must never branch on an Entrance/Junction fingerprint.

## 4. Generic new-game bootstrap

Since hardware-tested cleanup SHA `5d78c65ec2e0fbeeba3db6f93038eab288bc3354`,
new-game startup no longer builds Entrance by running the historical MAP1 probe
ladder.

After the bounded legacy intro teardown, `esp_native_startup.c` composes the
permanent APIs directly:

```text
DoomCanvas.startupMap
  -> mapFiles[] resource name
  -> EspMapCatalog_idForName()
  -> EspBspReader_inventoryPackEntry()
  -> EspMapResidentLifecycle_loadFromEmpty()
  -> EspPlayerSpawn_prepareInitial()
  -> EspPlayerView_applySpawn()
  -> post-spawn HUD/fresh-map/tile/orientation/facing owners
  -> EspNativeGameplayDispatch_adoptView()
  -> EspNativeGameplaySession_service()
```

The bootstrap contains no level fingerprint and no Entrance/Junction renderer
selection. The real CYD has run this route into Entrance, then moved and
interacted normally while retaining `shapeData == NULL` and
`mediaTexels == NULL`.

## 5. Player and gameplay ownership

`EspPlayerView` is the durable native pose owner. It stores current/destination
position and angle plus explicit pending setup bits. Runtime motion follows a
prepare/commit model rather than mutating legacy DoomCanvas/Player state.

`EspNativeGameplaySession` is map-independent. It consumes the current resident
runtime and `EspPlayerView` and owns the level-session startup order:

```text
native graphics catalog
 -> first native world frame
 -> HUD
 -> sprite dependency closure
 -> small resident PAK cache cold/warm
 -> large-range cache learn/warm
 -> resident gameplay input service
```

`EspNativeResidentGameplay` owns touch-delivered TURN/MOVE/SELECT dispatch,
dialog interaction and redraw boundaries. Doom RPG is turn-based; presentation
is requested by gameplay/UI needs, not architected as a mandatory high-FPS game
loop.

## 6. Collision, entities and pickups

The native world uses compact sprite topology plus `entities.db` type metadata.
It does **not** materialize the desktop Entity pointer graph.

Collision is split into generic reusable layers:

- static BSP/topology collision;
- entity-type collision from compact topology;
- current mutable line/door state;
- view prepare/commit transaction.

Pickup mutation is also explicit. The current production implementation owns
`eType=5` weapon pickup with a compact consumed-sprite bitset and native selected
weapon overlay. Other pickup families remain fail-closed/deferred until their
player-stat/inventory/ammo owners exist.

## 7. Event and script engine

The native event path is layered deliberately:

```text
tile/front interaction
 -> EspMapEvents descriptor
 -> EspMapEventFilter eligibility
 -> bounded semantic family
 -> explicit owner mutation / UI intent
 -> optional continuation transaction
```

`EspMapOpcodeExecutor` itself remains deliberately small and currently owns only
state opcodes:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

Other supported commands use dedicated reusable semantic owners instead of
turning the executor into a copy of desktop `Game_executeEvent()`.

Current production-bounded Entrance families include:

```text
7  EV_SHOW
8  EV_DIALOG
11 EV_CHANGESTATE
13 EV_UNLOCK
15 EV_OPENLINE
16 EV_CLOSELINE
18 EV_HIDE
19 EV_NEXTSTATE
20 EV_PREVSTATE
24 EV_FORCEMESSAGE
26 EV_DIALOGNOBACK
40 EV_NOTE
```

Current known deferred families include:

```text
2  EV_CHANGEMAP   transition consumer not yet promoted into live gameplay
9  EV_GIVEMAP     native automap production route pending
10 EV_PASSWORD    password input UI pending
27 EV_SAVEGAME    save consumer pending
41 EV_CHECK_KEY   native player-key owner pending
```

The rule is fail closed. A probe or parser existing for an opcode does not grant
permission for live gameplay execution.

## 8. Dialog and continuation model

Dialog presentation is native and PAK-backed. Text remains referenced by compact
map string identity and is read through bounded scratch storage. No map-wide
string duplication is required for presentation.

A dialog pauses script execution with explicit provenance. Before UI opens, the
saved continuation is preflighted. Supported continuation mutations are journaled
and can roll back if the following redraw fails.

The current bounded continuation family includes SHOW/HIDE/UNLOCK and state ops
11/19/20, with a fixed maximum command count. This keeps the recovered
`Game_runEvent()` pause/resume semantics without importing the legacy world.

## 9. Mutable line / door model

Immutable line geometry remains in `EspMapRuntime`.

Mutable door state lives in `EspMapLineState` and line texture state. Regular
OPENLINE/CLOSELINE visual interpolation uses a bounded native animator and a
render-only transient line view. The immutable source/runtime records do not
change.

MOVE source EXIT and destination ENTER event phases are transactionally coupled
to the committed player move and rendered result.

## 10. Rendering and retained startup compatibility

Native rendering consumes compact map records and PAK-backed image resources:

- plane renderer;
- projected wall bridge/consumer;
- generic native sprite projection/consumer;
- dynamic line view;
- native HUD/dialog/weapon painters;
- bounded wall/sprite/resource caches.

The active sprite renderer is map-generic:

```text
ESP32/src/esp_native_sprite_renderer.c
ESP32/include/esp_native_sprite_renderer.h
EspNativeSpriteRenderer_render()
```

There is no active Junction-specific renderer implementation/API.

A bounded part of the original startup still exists because retained menu,
entity-definition and Render helpers require legacy object/resource state before
the fully native map path takes over. Those dependencies must be explicit and
named by responsibility rather than left as permanent production probes.

The retained startup chain is currently:

```text
DoomCanvas/layout startup
 -> EspLegacyPrerenderStartup_start()
      -> ParticleSystem_startup()
      -> MenuSystem_startup()
      -> EntityDef_startup()
 -> EspRenderStartupBridge_start()
 -> EspLegacyConfigMappingsStartup_start()
      -> Game_loadConfig()
      -> validate mappings.bin ZIP metadata and allocation plan
      -> decode into the permanent framebuffer scratch
      -> install four immutable persistent mapping arrays
 -> menu BSP/menu compatibility stage
```

`EspLegacyPrerenderStartup` initializes exactly `ParticleSystem`, `MenuSystem`
and `EntityDef`, preflights their still-ZIP-backed compatibility resources, and
stops before Render. It is not a map loader or gameplay owner.

`EspRenderStartupBridge` aliases `Render.framebuffer` to PlatformVideo's
permanent 160x120 RGB565 framebuffer, keeps desktop `piDIB` absent, and loads the
legacy sintable/palette resources still required by retained Render helpers.

`EspLegacyConfigMappingsStartup` owns the retained config/mapping stage. It
allows a missing first-boot Config file, validates the legacy `mappings.bin`
allocation plan, then uses `readZipFileEntryInto()` to decode directly into the
already-owned 38,400-byte framebuffer. `EspLegacyMappings_load()` parses that
scratch payload and installs the same four persistent mapping arrays without a
second inflated heap buffer. ESP32 `Render_loadMappings()` delegates to this
bounded loader.

The mapping arrays are immutable after installation. They stay resident across
menu and gameplay BSP loads; a later `Render_loadMappings()` call recognizes a
complete owner and reports `REUSE` instead of allocating or decoding again.
`esp_render_mapping_reload_guard.c` preserves this ownership before
`Render_beginLoadMap()` and emits the `RETAIN-BEFORE-MAP` witness. It no longer
frees the arrays ahead of each map load.

These are compatibility boundaries, not permission to move migrated world data
back to ZIP ownership.

`PlatformVideo` owns the framebuffer and presents x2 to the ILI9341. Do not
optimize `PlatformVideo_present()` ahead of measured world/PAK hot paths merely
because it is visible in profiles.

## 11. Source-tree and diagnostics policy

Permanent **production** names should describe responsibility, not the map on
which behavior was discovered or the temporary recovery method used to prove it.

Preferred prefixes:

```text
esp_map_*                  compact map/runtime/event owners
esp_player_*               durable native player/view owners
esp_native_gameplay_*      live reusable gameplay semantics
esp_native_*_renderer      reusable rendering primitives
esp_render_*               retained generic Render compatibility boundaries
esp_legacy_*               explicit retained legacy compatibility boundaries
platform_*                 CYD hardware abstraction
native_intro_*             still-bounded intro compatibility path
native_main_menu_*         still-bounded menu compatibility path
```

Disallowed for new permanent production engine code:

```text
native_map2_*
native_map3_*
native_junction_* for map-independent behavior
*_probe.c as the permanent always-built implementation of gameplay/live compatibility semantics
```

That rule does **not** ban useful diagnostics. The project has two distinct
build intents:

```text
esp32-cyd          normal production/runtime hardware authority
esp32-cyd-bringup  optional diagnostic profile with extra probes/overlays
```

Bringup-only probes, regression witnesses and bounded instrumentation may retain
probe naming while they provide useful observability. They are not cleanup debt
merely because their names contain `probe`. Before removing or renaming any
remaining probe family, classify its real consumers: normal production,
bringup-only diagnostics, linker compatibility, or regression tooling.

Historical MAP1/Entrance/Junction probe ladders were removed because they had
become obsolete production scaffolding after permanent generic owners existed.
That is different from deleting active diagnostic instrumentation during an
unfinished engine port.

## 12. Test policy

Normal hardware authority is `esp32-cyd`. Bring-up builds can perturb RAM and do
not define canonical heap figures.

For a new semantic family or compatibility promotion:

```text
recover exact legacy behavior/current consumer
 -> classify normal-runtime vs bringup/regression use
 -> design or identify the reusable permanent API/owner
 -> add a temporary strict probe only if needed
 -> keep unsupported cases fail-closed
 -> commit + push agent/*
 -> test normal esp32-cyd on the real CYD
 -> treat Serial output as hardware truth
 -> document only observed results
 -> retire only obsolete production probe/shim surface
```

A milestone probe used as scaffolding should have an exit plan. A deliberate
bringup diagnostic instead needs a clear diagnostic purpose and build scope.

## 13. What is still transitional

The cleanup is intentionally incremental. Remaining transitional areas include:

- `main.cpp` still performs old platform/core/layout/menu bring-up and directly
  indexes `DoomRPG.zip` for legacy menu/HUD/bootstrap resources;
- `EspLegacyPrerenderStartup` still owns ZIP-backed compatibility resources for
  ParticleSystem/MenuSystem/EntityDef until those systems are replaced or their
  required assets move to appropriate native owners;
- `EspRenderStartupBridge` still loads legacy sintable/palette resources needed
  by retained Render helpers;
- `EspLegacyConfigMappingsStartup` still reads retained config/mapping resources
  through legacy compatibility APIs;
- menu BSP/menu graphics compatibility still needs classification into permanent
  runtime ownership vs useful bringup/regression diagnostics;
- some native menu sprite/overlay wrappers still expose probe-named linker
  compatibility symbols and must be audited before any cleanup;
- live CHANGEMAP/save/password/key/automap promotion is not complete;
- combat/monsters and several player-stat/inventory families are not native yet.

`pre_render_probe.*` and `config_mappings_probe.*` are no longer transitional
production naming debt because their live behavior now has permanent explicit
compatibility owners.

Remaining boundaries must be replaced or promoted carefully. Do not hide them
behind more per-map files, but also do not delete still-useful diagnostic
indicators just to reduce file count.

## 14. Definition of a clean future level

When the engine reaches another BSP, the expected implementation work is:

```text
0 new map-specific C files
0 new map-specific runtime allocators
0 new map-specific renderers
```

If that BSP exposes an unsupported opcode or entity behavior, implement **that
behavior family once**, test it against every relevant map corpus, and keep the
map itself as data.

## Main-menu selection ownership

MENU_MAIN selection is now an explicit native composition boundary.

Permanent direction:

```text
touch/presentation model
 -> semantic action dispatcher
 -> START | LOAD | OPTIONS | HELP owners
```

Do not reintroduce `MenuSystem_select()` or `Menu_select()` as a generic
routing shortcut. Their ESP32 linked implementations are already gone.

The pre-game model bridge is now explicit and bounded. `MenuSystem_t` remains
temporary storage, but fixed MAIN/CONTINUE/OPTIONS construction is owned by
`native_main_menu_model.c` and Help is parsed by the same owner's bounded
PAK-backed parser. The final ESP32 ELF contains neither `Menu_initMenu()` nor
`Menu_LoadHelpResource()`. Do not recreate the retired broad switch under a
new native name.

HELP and OPTIONS Back navigation are also explicit semantic routes through
`DoomRPG_esp32MainMenuReturnToMain()`; `MenuSystem_back()` is absent from
the linked ESP32 firmware.

Main-menu presentation invariants now have one owner,
`native_main_menu_present`, for framebuffer hashing, graphics-boundary checks
and fail-safe return to MENU_MAIN. A recovery may run only while ST_MENU still
owns the UI; it must never pull ST_INTRO or ST_PLAYING back into menu state.

Action APIs should distinguish expected user outcomes from failures. Main-menu
LOAD therefore exposes typed NO_SAVE / RECOVERED / TRANSITIONED / FATAL
results rather than a boolean whose meaning depends on current menu state.

START and LOAD are distinct permanent semantics. START always creates a new
game. The native checkpoint file is owned only by LOAD; desktop
`Config/Player/Player2/World` save discovery is not part of the ESP32 runtime
contract.

HELP already owns its paging/input natively, so future visual redesign should be
a presentation-only change rather than a reason to restore legacy menu
selection/render orchestration.

## Main-menu Back ownership

Pre-game Back navigation is now semantic rather than generic:

```text
HELP    -> MENU_MAIN
OPTIONS -> MENU_MAIN
```

Both routes use `DoomRPG_esp32MainMenuReturnToMain()`. The child frontend owns
its local touch/frame preconditions; the action owner owns the parent-state
contract, Back cue, native model transition, opaque presentation and touch
handoff.

Do not reintroduce `MenuSystem_back()` for this domain. It is absent from the
linked ESP32 firmware.

A later linked-runtime audit showed that generic `MenuSystem_setMenu()` was
not retained by a live menu/state owner after all. Its only remaining linked
caller was `DoomCanvas_setupmenu()`, itself reachable solely because a
bring-up diagnostic forced the monolithic desktop `DoomRPG_Init()` into the
ELF through `DoomRPG_engineLinkAnchor()`. Retiring that diagnostic anchor let
the linker remove the complete dead closure. Do not recreate
`MenuSystem_setMenu()` under a native name; semantic menu owners remain the
permanent direction.



## Legacy monolithic initialization is not a runtime owner

The ESP32 runtime must not keep `DoomRPG_Init()` alive merely as a linker
reachability test. Core construction, layout, retained compatibility startup,
menu ownership, intro handoff and gameplay startup are already explicit staged
owners.

The retired historical shape was:

```text
diagnostic address print
 -> DoomRPG_engineLinkAnchor
 -> DoomRPG_Init
 -> legacy setup/menu/map closure
```

The permanent rule is:

```text
explicit ESP32 staged owner
 -> only the dependencies required by that owner
 -> linker may discard unrelated desktop/J2ME orchestration
```

This is an architectural win even when the source files remain compiled as
behavioral references. Source-level call sites are not evidence of runtime
ownership; consolidation audits must distinguish source presence, ELF
reachability and real-CYD execution.


## Main-menu model/source ownership

The hardware-tested pre-game menu now follows this permanent direction:

```text
touch / opaque presentation
 -> semantic START | LOAD | OPTIONS | HELP actions
 -> bounded native pre-game model owner
    -> fixed MAIN / CONTINUE / OPTIONS records
    -> bounded HELP PAK parser
```

The implementation was consolidated from 13 `native_main_menu_*.c` translation
units to 7 before retiring the legacy model factory. Consolidation should keep
moving toward coherent ownership rather than one file per micro-step.

`Menu_initMenu()` and its broad desktop closure are absent from the final ELF.
That removal also drops unrelated menu/status/store/note construction helpers
and the 544 B static `vendingMenuTable`. This is a useful example of the
project's linker rule: source presence is not runtime ownership, and a small
explicit owner can let `--gc-sections` remove a much larger legacy closure.

For future consolidation audits, use final-ELF `nm`/`readelf` as the
authority for symbol reachability. A `firmware.map` entry at address zero may
describe a discarded input section and must not be treated as a live final
symbol.


## Pre-game START and intro ownership

The pre-game START path is now a native semantic boundary:

```text
MENU_MAIN START
 -> native new-game reset
 -> ST_INTRO
 -> native story renderer / clock / input
 -> native bounded intro disposal
 -> native transition presentation
 -> resident map bootstrap
```

The ESP32 build must not reintroduce `Menu_startGame()` as a generic entry
point. START owns new-game only; LOAD owns resume. A skip-intro request that
would bypass the resident native bootstrap is fail-closed until it receives its
own explicit native route.

The story renderer also must not call a generic legacy page-change helper.
Renderer-owned automatic progression is bounded to animation page `1 -> 2`.
Final exit from page 2 belongs to the input/clock owner and then
`Esp32IntroDispose_service()`, whose contract is resource disposal only and
explicitly forbids map loading.

The hardware-tested final ELF contains none of
`Menu_startGame`, `DoomCanvas_loadState`, `DoomCanvas_changeStoryPage`,
`DoomCanvas_disposeIntro` or `DoomCanvas_loadMap`.


## Intro disposal and native startup composition

The intro disposer is a resource owner, not a bootstrap interception point.
The permanent ESP32 composition is explicit:

```text
Esp32IntroClock_arm
 -> Esp32IntroDispose_reset
 -> EspNativeStartup_reset

final intro Continue
 -> Esp32IntroClock PARK
 -> Esp32IntroDispose_service
 -> EspNativeStartup_service
 -> resident map bootstrap
```

Do not reintroduce linker wrappers around `Esp32IntroDispose_reset()` or
`Esp32IntroDispose_service()`. The disposer remains bounded to intro images,
story text and clipping state; it never loads a map. `EspNativeStartup` owns
resident-map inventory/load, initial spawn routing and handoff toward the generic
gameplay session. The intro clock is the narrow composition owner because it
already owns the validated final-exit state machine.

This boundary was hardware-validated without changing static RAM or Flash and
reduced active linker wraps from 57 to 55.


## Production main-menu boot ownership

The opaque native pre-game menu no longer depends on a resident `menu.bsp`
world in production.

Permanent normal-boot ownership is:

```text
staged core/layout/startup
 -> config + immutable mappings
 -> native fixed MENU_MAIN model
 -> opaque native dashboard
 -> native touch/action owners
```

Do not rebuild the historical 3D menu map merely as a prerequisite for the
opaque dashboard. In the hardware-tested production image, the menu starts
without `Render_beginLoadMap(MAP_MENU)`, legacy nodes/lines/mapSprites, or the
loading-bar/longjmp structural escape.

The old menu BSP structure, wall and sprite probes remain useful regression
witnesses, but they belong to the explicit `esp32-cyd-bringup` diagnostic
profile. Diagnostic reachability must not force their loader closure back into
the normal firmware.

This distinction is deliberate:

```text
esp32-cyd          permanent product owners only
esp32-cyd-bringup  historical structural/render probes may be retained
```

The real CYD validates native MENU_MAIN, OPTIONS/HELP Back, START, complete
intro/bootstrap, Entrance resident gameplay and a committed move with
`shapeData == NULL` and `mediaTexels == NULL`.
