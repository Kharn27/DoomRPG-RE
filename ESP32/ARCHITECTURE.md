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
another level-specific renderer. A new source module is justified only by a
new **behavior family**, storage format, renderer primitive, or explicit owner
that is reusable by every map that needs it.

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

## 10. Rendering and retained Render compatibility

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

A small part of legacy `Render` remains as a compatibility shell for recovered
fixed-point helpers and startup state while legacy BSP arrays remain NULL. The
permanent CYD boundary for that retained startup shell is:

```text
ESP32/src/render_startup_bridge.c
ESP32/include/esp_render_startup_bridge.h
EspRenderStartupBridge_start()
```

This bridge deliberately aliases `Render.framebuffer` to PlatformVideo's
permanent 160x120 RGB565 framebuffer, keeps desktop `piDIB` absent, and loads the
legacy sintable/palette resources still required by retained Render helpers. It
is a generic platform compatibility boundary; it must never become a map loader
or map-specific owner.

`PlatformVideo` owns the framebuffer and presents x2 to the ILI9341. Do not
optimize `PlatformVideo_present()` ahead of measured world/PAK hot paths merely
because it is visible in profiles.

## 11. Source-tree policy

Permanent production names should describe responsibility, not the map on which
that behavior was discovered.

Preferred prefixes:

```text
esp_map_*                  compact map/runtime/event owners
esp_player_*               durable native player/view owners
esp_native_gameplay_*      live reusable gameplay semantics
esp_native_*_renderer      reusable rendering primitives
esp_render_*               retained generic Render compatibility boundaries
platform_*                 CYD hardware abstraction
native_intro_*             still-bounded intro compatibility path
native_main_menu_*         still-bounded menu compatibility path
```

Disallowed for new permanent engine code:

```text
native_map2_*
native_map3_*
native_junction_* for map-independent behavior
*_probe.c as the permanent implementation of gameplay semantics
```

Historical `native_map1_*`, Entrance and Junction probe ladders were useful
recovery instruments. Once a permanent owner exists and hardware proves the
replacement route, they must leave the production build and can be removed from
the active tree. Git history is the detailed archaeological archive.

## 12. Test policy

Normal hardware authority is `esp32-cyd`. Bring-up builds can perturb RAM and do
not define canonical heap figures.

For a new semantic family or compatibility promotion:

```text
recover exact legacy behavior/current consumer
 -> design or identify the reusable permanent API/owner
 -> add a temporary strict probe only if needed
 -> keep unsupported cases fail-closed
 -> commit + push agent/*
 -> test normal esp32-cyd on the real CYD
 -> treat Serial output as hardware truth
 -> document only observed results
 -> retire the temporary probe/shim after permanent integration
```

A milestone probe is scaffolding, not architecture.

## 13. What is still transitional

The cleanup is intentionally incremental. Remaining transitional areas include:

- `main.cpp` still performs old platform/core/layout/menu bring-up and directly
  indexes `DoomRPG.zip` for legacy menu/HUD/bootstrap resources;
- `pre_render_probe.*`, `config_mappings_probe.*`, `menu_bsp_probe.*` and some
  menu/graphics compatibility entry points still carry historical `probe` names;
- some native menu sprite/overlay wrappers still expose probe-named linker
  compatibility symbols;
- live CHANGEMAP/save/password/key/automap promotion is not complete;
- combat/monsters and several player-stat/inventory families are not native yet.

These are explicit cleanup/feature boundaries. They must be removed by replacing
or promoting them with permanent generic ownership, not by hiding them behind
more per-map files or deleting live compatibility behavior blindly.

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
