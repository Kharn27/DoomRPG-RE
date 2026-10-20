# Doom RPG ESP32-native architecture

This document is the permanent architecture reference for the classic
ESP32-2432S028R CYD port. It describes the engine we are keeping, not the order
in which historical recovery probes were written.

For current hardware status and exact canons, read [`PORTING_STATUS.md`](PORTING_STATUS.md).
**PR #224 follow-up (20 October 2026):** current production code hardware exercised
on the classic CYD is `f15c412c757c40eceacd353eec3c3439d33462c2`,
with [CI push success](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39463831546)
and [CI PR success](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39463839288).
The ActionEngine compositor runs only when
`EspNativeResidentGameplay_ownsWorldPresent()` authorizes it: either
resident gameplay is active, or the native resident service temporarily
owns the **initial** world present in `firstWorldPresentArmed`.
The short lease wraps the sole initial `Esp32PlatformVideo_present()`
before activating player input/turns and is cleared even on failure.
Full-screen MENU_MAIN, INTRO, SAVESLOTS and transitions never borrow that
lease. This fixes the review P2 without reverting the menu HUD bleed fix.
Real CYD logs show fresh START, SYS EXIT, first LOAD selector, V11
slot04 restore, resumed WORLD, movement, monster attack and retaliation,
and stable observed gameplay heap; Serial does not independently prove
the exact first-facing-label pixels. Hot-path `NATIVESPRITE CROSS` and
`TRANSIENT` `printf` spam is a separate future trace-level cleanup,
not part of this hardware-proven milestone.

As of PR #224 the latest **targeted hardware-tested** code is
`0686740c1361068f3c96d05eab95e4713b37aa49`: `Render_startup` is
no longer an ESP32 public ABI and is a private static native startup
implementation (FNV `f6343a76`) in `render_startup_bridge.c`. Native
presentation ownership now excludes the gameplay ActionEngine compositor
whenever the resident gameplay session is inactive, fixing the first
MENU_MAIN LOAD selector's inherited top HUD strip after SYS EXIT. The
user confirmed the first LOAD opens with no gray top bar on actual CYD.
This is a presentation ownership boundary, not a gameplay/input timing
change. Other post-EXIT paths and V11 combat were tested on earlier
code but not this final SHA.

Historically, as of PR #223 CYD-tested code `eb93bf2fefe3aa48cd87b933648d3a84c78f9743`,
`Render_init` and `Render_setup` are **not** ESP32 Render ABI symbols.
They live privately within the native engine root/layout as respectively
`EspNativeEngineRoot_initRender` (FNV `884791fe`) and
`EspNativeEngineLayout_setupRender` (FNV `ec9d1151`) in `engine_stubs.c`.
The 1532-byte `Render_t`, 160x80 viewport, three setup arrays totaling
1280 B, and the `Render_startup` implementation semantics remain unchanged despite
its later ABI retirement. `Render_free` is not retired by PR #224.
This SHA passes real CYD START → EXIT → LOAD V11 → monster combat including
two kills, with constant observed gameplay heap. Fatal Render teardown is
not hardware-tested. The user has identified **lost perceived responsiveness**
after animations: `COMBAT-INPUT-BLOCK ... queued=no` rejects taps while
monster attack/retaliation owns the turn. Input admission/buffering must
be its own bounded, turn-safe milestone; no world/RNG mutation until commit.
For build/platform notes, read [`DOCUMENTATION.md`](DOCUMENTATION.md).

Historical ELF census (main `b941048`, 14 October 2026;
older census `9d203e5`): [`CONSOLIDATION_AUDIT.md`](CONSOLIDATION_AUDIT.md).
Its per-source counts and pre-retirement `DoomRPG.o` tables describe the
**old baseline**, not current compilation. Older recovery passages below
(especially §6/7/10/13) can also be historical: use current source and
PORTING_STATUS for runtime ownership and exact CYD evidence.

Historical source boundary after [PR #218](https://github.com/Kharn27/DoomRPG-RE/pull/218)
and [PR #219](https://github.com/Kharn27/DoomRPG-RE/pull/219)
(final code `a59554ed524502d02dbab69149777a94050b0a26`,
[CI success #39162920720](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39162920720)):

- All **21 original desktop C translation units**, including Game, Render,
  DoomCanvas and DoomRPG, are excluded from firmware compilation.
- **No desktop C copy is generated**. `src/DoomRPG.c` and `src/Render.c`
  remain executable behavior/reference sources for direct host CRC/parity
  tests, not firmware object providers. The build still patches and compiles
  only the separate **packed-indexed SDL C++ shim**; its 1/4/8-bpp
  surface→texture ownership stays unchanged.
- Permanent native owners supply math/binary helpers, PAK/BMP images,
  presentation, process clocks/RNG and the small process root/teardown.
  Desktop Config v23 readers and keyboard/controller binding tables are
  retired; brightness persistence belongs to the native SD config.
- **Game**: no `Game_t` process allocation and no linked `Game_*`
  provider remain. The historical `doomRpg->game` root field is retained
  for layout compatibility but is **always NULL** on CYD; startup, intro,
  mappings, resident session and SYS/EXIT gates refuse non-NULL.
  A zero-byte Game diagnostic stage remains and the native root teardown
  does not free a Game instance.
- **Canvas lifecycle**: as of the hardware-proven [PR #223](https://github.com/Kharn27/DoomRPG-RE/pull/223)
  code `201232031e595563a42ca1e0d0f34d0252b1ab07`, **zero desktop Canvas
  lifecycle ABI exports** remain on ESP32. Init/startup/release belong to the
  native process root/layout (`engine_stubs.c`); Canvas MENU state transition
  belongs to the native main-menu model, and native const-text raster with a
  private glyph blitter lives in `esp_native_canvas_text.c`. Blitter source FNV
  remains `4c4a9a93`; the Canvas allocation remains 44 B. The separately
  conditional no-op bringup `DoomCanvas_updateLoadingBar` is not a gameplay or
  lifecycle export. Do not reintroduce the desktop object graph or map-wide
  shape/texel memory.

The generic startup/session replaces the historical Junction post-load
marker ladder (nine source/header pairs); the unrelated ZIP/menu-scene
probe renderer (16 sources and 13 headers) was also retired. Its former
PAK-irrelevant pipeline must not be reintroduced. The earlier census
reported **158 C/C++ sources and 152 include headers**. Two dormant menu
scene/overlay wrappers remain because their removal can change compiler
inlining of active painter functions; handle them separately.

[CI #39162920720](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39162920720)
verifies the final Game and five-export Canvas boundary across host,
normal/bringup/trace builds and compiledb/ELF audits: no original desktop
C, no generated DoomRPG C, no linked Game_* or public Canvas image blitter.
Static RAM remains **49 488 B** normal, Flash **820 997 B** normal.
Real CYD accepted the current Help/About (83 lines / five observed scroll
positions) → Options → START/intro → Entrance route, with stable framebuffer
hashes and `heap8=113816 largest8=86004`; shapeData/mediaTexels NULL.
The last log does not show its boot `exports=5` string or re-test LOAD/combat.
Earlier Game removal CYD evidence includes EXIT → LOAD V11, but belongs to
its separate SHA. No strict ELF bytewise identity, full gameplay coverage,
active audio or runtime SHA fingerprint is claimed.

## 1. Design rule

Damage feedback remains in the existing ActionEngine presentation owner.
Post-COMMIT directional enqueue attempts the first overlay present before
the sequencer resumes other monsters; it never waits for the blink cadence,
renders the world again, or rolls back gameplay on a UI-present failure.
The service admits direction before expiring the underlying border; active
and queued cues protect LIFO restoration. Touch/PAK/level-up/transaction
barriers and the existing FIFO/retry/timeout remain authoritative. An extra
bounded physical overlay present is possible; this is synchronization, not
a latency optimization. Host compositor tests do not prove hardware SPI
timings or complete retaliation rollback. The fix changes normal loaded code,
unlike the earlier ALLOC-identical consolidation increments.

Credit-gated script interactions reuse `EspNativeGameplayEventChain` for their
dialog/WAIT continuation and per-segment journal. PlayerState owns money;
NotePrefix leases its existing candidate/scratch until frame publication;
sprite topology owns SHOW admission/commit; GibFx provides a stateless capture
painter. The WAIT clock lives with the continuation, not a second world engine.
Frame owns the explicit `composeTurn` no-present endpoint used for the effect
background (preview sprites, no activation/automap/fresh-frame publication).
The existing dynamic-line adapter scopes that composition exactly like visual
previews; EventChain adds the effect and owns its single physical publication.
Do not pre-arm PresentGate before rendering: the compositor deliberately refuses
it. Dialog retains cancellation state and exposes a visible native BACK card
only for EV_DIALOG; its tap maps to the original MENU/cancel semantics, not a
new script YES/NO branch.
Unsupported stats/particle families remain closed. This is a new production
boundary requiring CYD validation, not part of the earlier byte-identical
cleanup evidence. Production source/header counts remain 158/152.

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
legacy DoomRPG.game == NULL (no Game_t instance)
no desktop Game.entities / Game.monsters runtime arrays exist
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

The resident owns the automap/world mode, independently of EventChain's script
rollback journal. Opcode-23 startup closes automap before opening a modal;
failed startup must restore the entry mode and render automap again, not just
redraw the world. Failed restoration disables gameplay; a successful modal
pause does not restore automap early. This uses existing resident/render paths,
without transferring credit/RNG/script ownership or adding another state owner.

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
      -> preflight j.bmp/p.bmp through the PAK adapter
      -> EspNativeMenuStorage_startup()
 -> EspRenderStartupBridge_start()
 -> EspLegacyConfigMappingsStartup_start()
      -> Game_loadConfig()
      -> read mappings.bin from the PAK into the framebuffer scratch
      -> validate the allocation plan
      -> install four immutable persistent mapping arrays
 -> native fixed MENU_MAIN model + opaque dashboard (normal profile)
```

`EspLegacyPrerenderStartup` preflights `j.bmp`/`p.bmp` and initializes native
menu storage. Desktop ParticleSystem/EntityDef startup is retired; the native
entity catalog is built from the PAK at resident-map load. This startup adapter
is not a map loader or gameplay owner.

`EspRenderStartupBridge` aliases `Render.framebuffer` to PlatformVideo's
permanent 160x120 RGB565 framebuffer, keeps desktop `piDIB` absent, and loads the
legacy sintable/palette resources still required by retained Render helpers.

`EspLegacyConfigMappingsStartup` owns the retained config/mapping stage. It
allows a missing first-boot Config file, validates the legacy `mappings.bin`
allocation plan, using `EspLegacyAssetSource_readInto()` to read the PAK entry
directly into the already-owned 38,400-byte framebuffer. `EspLegacyMappings_load()` parses that
scratch payload and installs the same four persistent mapping arrays without a
second inflated heap buffer. ESP32 `Render_loadMappings()` delegates to this
bounded loader.

The mapping arrays are immutable after installation. They stay resident across
compatible reload calls; a later `Render_loadMappings()` recognizes a complete
owner and reports `REUSE` instead of allocating or reading again. The dormant
`esp_render_mapping_reload_guard.c` wrapper has been removed: it contributed no
allocated section in either profile and does not own those arrays. Native map
startup retains its explicit legacy-runtime-empty and cache-inactivity gates.

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

That rule does **not** ban useful diagnostics. The project has three distinct
build intents:

```text
esp32-cyd          normal production/runtime hardware authority
esp32-cyd-trace    production-path diagnostics, release + TRACE + one sprite filter
esp32-cyd-bringup  optional diagnostic profile with extra probes/overlays
```

Trace inherits the normal profile, not bringup: it preserves production layouts
and routing and does not enable startup census or touch overlays. Its targeted
sprite logs observe existing renderer reads; they own no state or buffer and
compile out unless `DOOMRPG_ESP32_TRACE_SPRITE` is defined. Serial affects timing,
so final gameplay/latency authority remains normal. Retired one-off story-hand
teardown and divider firmware profiles are covered by host regression tests
(real code, mocked image I/O/intro cleanup, no ESP32 heap/PAK parity claim).

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

- `main.cpp` still composes platform/core/layout and compatibility startup,
  but its resource adapter is PAK-backed, not a live ZIP reader;
- `EspLegacyPrerenderStartup` retains its older name while composing native
  menu storage; desktop ParticleSystem/EntityDef startup is retired;
- `EspRenderStartupBridge` still loads legacy sintable/palette resources needed
  by retained Render helpers;
- `EspLegacyConfigMappingsStartup` still installs native PAK-backed
  `mappings.bin`; desktop Config v23 has been retired, while brightness
  persistence is owned by `EspNativeConfig` on SD;
- two dormant menu scene/overlay wrapper bodies remain inside the live painter
  TU: their removal changes compiler inlining, despite their absence from both
  final ELFs. Retire them with a targeted menu validation, not a false no-op claim;
- historical LRU cache bodies/frame layouts remain only because `isActive()`
  gates and 488 B of static metadata are still linked. Their load APIs are not
  current renderer ownership and must not be reactivated;
- native gameplay families still have explicitly unsupported branches; current
  CHANGEMAP/save/password/combat/inventory limits are in PORTING_STATUS, not a
  blanket claim that these families are still desktop-owned.

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

The old ZIP menu BSP structure/wall/sprite suite is retired. Before this
consolidation, `DoomRPG_probeMenuBspHeader()` already returned 0 on every ESP32
profile, so bringup did not reach that suite either. Its callable fail-closed
report is retained unchanged. In bringup, `initializeMainMenu()` still reports
`MENU BSP FAIL`; this is an existing diagnostic limitation, not a new regression
or an assertion that the production menu fails.

This distinction is deliberate:

```text
esp32-cyd          permanent product owners only
esp32-cyd-bringup  active diagnostics/overlays + retired ZIP endpoint report
```

The real CYD validates native MENU_MAIN, OPTIONS/HELP Back, START, complete
intro/bootstrap, Entrance resident gameplay and a committed move with
`shapeData == NULL` and `mediaTexels == NULL`.


## First-frame diagnostics are not production ownership

The permanent first resident world-frame path is the real native owner:

```text
EspNativeGameplaySession
 -> EspNativeFirstFrame_route
 -> native wall/plane renderer
 -> framebuffer FNV/publication
 -> Esp32PlatformVideo_present
 -> EspNativeFirstFrame_view
```

Production must not interpose a linker wrapper merely to measure viewport color
statistics or arm an optional BMP dump. Those are regression diagnostics and
belong to the explicit `esp32-cyd-bringup` profile.

The hardware-tested direct route preserves the exact Entrance first-frame FNV,
presentation contract and fail-closed session behavior while removing one
wrapper, 8 B of static RAM and 928 B of linked Flash.


## Compile-time logging policy

ESP32 logging is a diagnostic surface, never evidence of functional ownership.
Permanent C/C++ code may classify messages through `doomrpg_log.h`:

```text
ERROR  invariant/fail-closed/OOM/corruption or actionable failure
INFO   operational transitions and normal hardware-test witnesses
DEBUG  bounded one-shot census/inventory/audit detail
TRACE  hot-path timing, periodic profiling and per-read/per-frame telemetry
```

Normal `esp32-cyd` defaults to INFO; `esp32-cyd-bringup` may explicitly
enable TRACE. Disabled levels compile away, including format strings, without a
runtime setting owner.

Instrumentation and ownership must be audited independently. PlatformVideo,
Render plane/cull and EspAssetPack wrappers retain functional responsibilities
even when profiling output is disabled. Hot success summaries should generally
be TRACE, but failure paths from the same owner must remain ERROR/INFO as
appropriate. ALIVE stays INFO until a separate hardware-validated milestone
changes its cadence or level.

The movement hot path classifies 19 diagnostic sites in its existing owners:
16 detailed success/pathfinding messages are TRACE-only; three mixed witnesses
remain operational when prepare, position/RNG rollback, or member publication
fails. Turn BEGIN/COMPLETE and ORDERED-DISPATCH remain INFO, as do the attack
probe/resolution pairs consumed by the PC playtest watcher's RNG checks.
This is compile-time diagnostic consolidation, not batched monster transactions
or shorter attack cadences. Movement's position/activation/capture/sequence
boundaries are still functional, including their thin admission adapter.
An unchanged render FNV is a profiling clue, not sufficient proof to skip a
future invisible-monster redraw. No additional framebuffer/audio RAM is reserved.

Read-only recovery census code is not production ownership. If an inventory or
corpus probe is needed only for recovery, the normal INFO image should neither
execute it nor retain a linker interception solely to reach it. Such probes may
remain compiled/routed by `esp32-cyd-bringup` at DEBUG/TRACE while the
permanent gameplay/transition owners stay direct. Map-specific recovery scans
must never be a prerequisite for session readiness or a live transition.
