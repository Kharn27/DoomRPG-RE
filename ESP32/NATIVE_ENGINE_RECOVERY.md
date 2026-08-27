# Doom RPG ESP32-native generic engine recovery

This document is the compact recovery contract for the permanent native engine. It synthesizes the historical `MAP1_*.md` milestone archive into the architecture that production code must use for every resident BSP.

The milestone files remain evidence. They are not runtime architecture.

## Source of truth

For recovery, read in this order:

1. current GitHub `main` and its exact SHA;
2. `PORTING_STATUS.md`;
3. this file;
4. `DOCUMENTATION.md`;
5. only then the specific `MAP1_*.md` archive needed for a behavior or hardware witness.

A historical probe can be an executable regression witness. It must never become a production prerequisite.

## Fundamental rule

A new map is not a new engine.

```text
DoomRPG-ESP32.pak
 -> parse requested BSP
 -> publish compact resident runtime
 -> publish bounded mutable overlays
 -> prepare player/view
 -> enter generic gameplay session
 -> render / HUD / input / collision / events
```

Do not add permanent `LEVEL_X_*`, `ENTRANCE_*` or `JUNCTION_*` gameplay pipelines merely because a different BSP is loaded.

A map may expose a previously unsupported data or behavior family. That is a bounded engine extension: recover its legacy semantics, implement a generic native owner/API, fail closed elsewhere, hardware-test it, and then make it available to every map that uses the same family.

## Hardware-proven generic gameplay session

Real classic CYD hardware validated implementation:

```text
branch = agent/esp32-native-entrance-startup-route
base = be4a9a666245663da7866a8aa0aa40b98339d076
hardware-tested implementation SHA = 300561cfc9b4d06af769fda54613d837fa738f58
map = /intro.bsp / Entrance / resourceMapId 1
status = REAL-CYD HARDWARE PASS
```

The tested lifecycle is now the required production model:

```text
resident EspMapRuntime
 -> settled EspPlayerView
 -> EspNativeGraphicsCatalog
 -> first world frame
 -> initial native HUD
 -> sprite dependency closure
 -> resident PAK cache owner
 -> SMALL-COLD frame
 -> SMALL-WARM frame
 -> enable shared-payload exact 2048-B ranges
 -> LARGE-LEARN frame
 -> LARGE-WARM frame
 -> entity definition collision catalog
 -> native resident gameplay service
 -> touch + TURN/MOVE
```

The first playable Entrance session reached:

```text
[ENGINESESSION] READY map=1 angle=64
residentCache=yes
largeCache=yes
touch=invisible-120ms
TURN+MOVE=armed
shapeData=0x0
mediaTexels=0x0
```

This is proof that the gameplay/render path previously proven on Junction is map-generic when initialized explicitly by engine ownership instead of historical Junction probes.

## Permanent runtime owners

### Resident map

The map parser/runtime owns compact immutable BSP-derived data only.

```text
EspMapRuntime
EspMapState
EspMapScriptState
line state
texture state
automap state
native topology
```

No map-wide `shapeData`. No map-wide `mediaTexels`. No legacy pointer-heavy entity world.

The runtime backing store is `/DoomRPG-ESP32.pak`; runtime ZIP access is forbidden for migrated paths.

### Player/view

The current resident map and load semantics prepare one native player/view owner. The renderer and gameplay session consume that owner; they do not infer a hardcoded level identity.

Initial Entrance hardware proof:

```text
spawn tile = 904
tileXY = 8,28
position = 544,1824,36
oldZ = 4
angle = 64
targetMapId = 1
gameplayLoadMapId = 1
source = BSP HEADER
```

### Graphics catalog

`EspNativeGraphicsCatalog` is the sparse immutable mapping for graphics resources actually required by the current resident runtime. Renderer-owned sprite dependencies are expanded explicitly before gameplay.

Entrance initial catalog hardware witness:

```text
textures = 33
sprites = 45
storage = 3120 B
FNV = 29ffc14a
```

After sprite dependency closure:

```text
sprites = 46
```

No permanent map-wide decoded texel pool is allowed.

### Renderer

The generic gameplay compositor owns:

```text
160x80 world viewport at y=20
textured floor/ceiling
walls
BSP-visible bounded sprite workset
sprite glow companions
HUD direction repaint
one final 160x120 presentation
```

The historical function/file names containing `Junction` are legacy milestone names, not a map restriction. Production code must consume a map-generic facade/API.

Plane reconstruction keeps six bounded 2048-B cache leases. The implementation must not depend on one contiguous 12288-B allocation after other runtime owners fragment the heap.

Entrance proved sprites at multiple poses, including frames with 13 draws / 3789 pixels and 6 draws / 6776 pixels. A valid view may also contain zero visible sprite draws.

### HUD

The native gameplay HUD is resident-map generic. Initial new-game model currently uses recovered fresh-game combat values and map/orientation from `EspPlayerView`.

Hardware witness on Entrance:

```text
hp = 30/30
armor = 0/20
weapon = 2
ammo = 8
resourcesValidated = 5
pixelsWritten = 7538
```

### Input

The current native input layer is the hardware-proven invisible 12-zone touch layout with transient 120 ms feedback. Touch callbacks queue intent; gameplay semantics run in the gameplay service.

Currently armed production semantics:

```text
TURN_LEFT
TURN_RIGHT
FORWARD
BACK
STRAFE_LEFT
STRAFE_RIGHT
```

Currently classified but semantically deferred actions include SELECT, PASS_TURN, menu/automap and weapon actions unless/until their dedicated native executor is enabled.

### Collision

The generic native movement path uses resident topology + entity definition type catalog + line state.

Entrance hardware proof includes the closed/locked line at the start area:

```text
source tile = 904
dest tile = 936
line = 258
texture = 7
flags = 0x00000505
type = 0
defTile = 312
result = BLOCKED
```

Collision remains data-driven; no per-map hardcoded blocker table is permitted.

## Render-cache lifecycle

The render cache is a permanent storage service, not a probe side effect.

The required initialization order is:

```text
1. build graphics catalog and render first frame/HUD with ordinary bounded PAK reads
2. allocate/enable resident small-range owner
3. render SMALL-COLD
4. render SMALL-WARM
5. enable exact 2048-B ranges inside the existing shared payload, with zero owner growth
6. render LARGE-LEARN
7. render LARGE-WARM
8. only then arm interactive gameplay
```

Entrance real-CYD hardware witness:

```text
resident owner bytes = 21160
payload capacity = 16384
range entry capacity = 256
heap8 after owner = 31956
largest8 after owner = 8692

SMALL-COLD  total = 2119886 us
SMALL-WARM  total = 256807 us
LARGE-LEARN total = 247770 us
LARGE-WARM  total = 229719 us

large entries after prime = 2
payload used = 14645 / 16384 B
```

The 2048-B tier reuses the existing payload and must not allocate another large owner.

Normal gameplay frames then stay in roughly the already-proven turn-based latency class, depending on view complexity. The hardware log showed examples around 207-363 ms, plus an intentionally recovered legacy compact wall guard path around 519 ms.

Do not optimize `PlatformVideo_present()` prematurely. A presentation currently costs about 34 ms and is not the dominant historical cold-render bottleneck.

## Gameplay semantics boundary

Hardware-proven and production-usable now:

```text
resident map load
compact immutable runtime
mutable overlays
initial spawn/player/view
first native frame
sprites/glows
HUD
calibrated touch intent
TURN
MOVE
static/entity/line collision
dynamic line collision
SELECT front-tile resolver/provenance
```

SELECT resolution is currently an observer/resolver, not yet the action executor.

Entrance hardware examples:

```text
front tile 841 -> event 88
eligible commands: EV_DIALOG + EV_NEXTSTATE

front tile 936 -> event 91
line 258 locked
eligible command: EV_OPENLINE(258)
```

The current gameplay service correctly logs `SELECT ... semantic-not-enabled` after provenance. No bytecode/door mutation occurs yet.

Next bounded milestone should therefore make the Action/SELECT button execute only the already-supported safe semantic family needed by Entrance, without broad-enabling legacy `Game_executeEvent`.

Tile-enter event execution after ordinary MOVE also remains deferred until its dedicated gameplay milestone reconnects the already-proven event/filter/executor owners.

## Event/script architecture

The `MAP1_*` event milestones proved reusable engine components, not Entrance-only code.

Current generic opcode executor remains intentionally restricted to:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

All other opcode families remain fail-closed until a dedicated permanent API owns their side effects.

Already recovered native semantic families include compact UI intents/string reading, status/dialog/notebook owners, key/password state, line door/unlock state, automap grant, save/change-map intents and exit bookkeeping. Their historical probes prove behavior; production gameplay should call permanent owners directly when each family is enabled.

## How to add the next BSP

For any future level:

```text
1. stream BSP from /DoomRPG-ESP32.pak
2. validate parse bounds/inventory
3. publish the same compact EspMapRuntime owners
4. prepare spawn/transition-specific EspPlayerView
5. call the same EspNativeGameplaySession
6. play
```

Do not create a special renderer/HUD/input/cache pipeline for that map.

If it fails, diagnose the first unsupported data/semantic family. Fix that family generically. Do not fork the engine by level.

The only legitimate map-specific facts are data and recovered behavior such as spawn location, resource sets, topology, events, scripts and transition targets.

## Historical archive map

The milestone archive is still valuable, grouped as follows.

Runtime/data:
- structural BSP/load/inventory archives;
- native runtime/access/state/events/descriptor/filter archives.

Event semantics:
- opcode executor;
- UI intent/string/status/dialog/notebook;
- key/password;
- line door/unlock;
- automap/save/change-map/show-hide/exit state families.

Transition/player:
- transition preflight;
- resident handoff/committed transition;
- spawn/player view;
- HUD refresh/player setup/tile-enter/facing/post-load owners.

Render/performance:
- graphics catalog;
- first frame;
- sprites/glows;
- gameplay render hotpath;
- resident small cache;
- exact 2048-B large-range cache.

Gameplay:
- HUD;
- touch input;
- TURN;
- MOVE/collision;
- closed/dynamic line collision;
- SELECT front-tile provenance.

Use the archive to recover exact behavior or historical fingerprints. Do not reproduce its one-milestone-at-a-time wiring in production.

## Permanent invariants

```text
classic CYD / ESP32-D0WD-V3
4 MB flash
no PSRAM
160x120 RGB565 framebuffer = 38400 B
shapeData == NULL
mediaTexels == NULL
/DoomRPG-ESP32.pak backing store
no runtime ZIP for migrated paths
legacy Game.entities == 0
legacy Game.monsters == 0
bounded caches / small buffers / compact owners
unsupported semantics fail closed
```

## Current merge boundary

```text
hardware-tested implementation SHA = 300561cfc9b4d06af769fda54613d837fa738f58
REAL-CYD HARDWARE PASS
Entrance visible and walkable = YES
sprites/HUD/touch/TURN/MOVE/collision = YES
render caches primed = YES
shapeData/mediaTexels = NULL
SELECT semantic execution = NEXT
MERGE-READY = YES after docs-only closeout
```
