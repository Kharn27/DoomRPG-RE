# ESP32 documentation map

This file indexes the current classic-CYD Doom RPG port documentation.

## Source of truth

For recovery, use:

1. current GitHub `main` and its exact SHA;
2. [`PORTING_STATUS.md`](PORTING_STATUS.md);
3. [`NATIVE_ENGINE_RECOVERY.md`](NATIVE_ENGINE_RECOVERY.md);
4. this file;
5. the latest relevant milestone archive.

If chat history and repository state disagree, the repository wins.

## Latest merged boundary

```text
PR   = #100 — native gameplay SELECT front-tile
main = be4a9a666245663da7866a8aa0aa40b98339d076
status = MERGED
```

Merged evidence: [`MAP1_NATIVE_GAMEPLAY_SELECT_FRONT_TILE.md`](MAP1_NATIVE_GAMEPLAY_SELECT_FRONT_TILE.md).

## Current candidate boundary

Latest hardware archive: [`MAP1_NATIVE_GENERIC_RESIDENT_GAMEPLAY.md`](MAP1_NATIVE_GENERIC_RESIDENT_GAMEPLAY.md).

```text
branch = agent/esp32-native-entrance-startup-route
base = be4a9a666245663da7866a8aa0aa40b98339d076
hardware-tested implementation SHA = 300561cfc9b4d06af769fda54613d837fa738f58
status = REAL-CYD HARDWARE PASS
merge-ready = YES
post-test commits = documentation-only
```

## What changed architecturally

The original startup correction established that `/intro.bsp` / Entrance is the first post-cinematic gameplay map. The branch then carried that route all the way through spawn, rendering, HUD, cache priming and native gameplay.

The important conclusion is now permanent:

```text
A NEW BSP IS NOT A NEW ENGINE.
```

Production runtime is:

```text
/DoomRPG-ESP32.pak
 -> compact resident EspMapRuntime
 -> compact mutable overlays
 -> EspPlayerView
 -> EspNativeGameplaySession
 -> generic renderer/HUD/input/collision/events
```

Historical `MAP1_*` probes and level-named render milestones remain executable evidence. They are not allowed to become runtime prerequisites.

The full synthesized contract is [`NATIVE_ENGINE_RECOVERY.md`](NATIVE_ENGINE_RECOVERY.md).

## Current real-CYD hardware proof

Entrance startup/playback:

```text
file=/intro.bsp
name=Entrance
resourceMapId=1
spawn tile=904
position=544,1824,36
angle=64
```

Generic session reached:

```text
first world frame = YES
sprites/glows = YES
native HUD = YES
resident small cache = YES
exact 2048-B cache = YES
touch = YES
TURN/MOVE = YES
native collision = YES
shapeData = NULL
mediaTexels = NULL
legacy entities = 0
legacy monsters = 0
```

Cache prime hardware witness:

```text
owner=21160 B
payload=16384 B
heap8 after owner=31956
largest8 after owner=8692
SMALL-COLD=2119886 us
SMALL-WARM=256807 us
LARGE-LEARN=247770 us
LARGE-WARM=229719 us
large entries=2
```

The tester walked and turned around Entrance repeatedly. Sprites rendered at multiple poses and line collision blocked the closed/locked start-area line correctly.

## Current gameplay boundary

Production-enabled:

```text
resident load
spawn/player/view
world render
planes/walls
sprites/glows
HUD
12-zone calibrated touch
TURN_LEFT/TURN_RIGHT
FORWARD/BACK/STRAFE
static/entity/line collision
dynamic per-line collision
SELECT front-tile resolver/provenance
```

Semantically deferred:

```text
SELECT/Action execution
PASS_TURN
ordinary MOVE tile-event execution
menu/automap/weapon gameplay
combat
unsupported opcode families
```

Entrance SELECT hardware evidence already exposes the next bounded target:

```text
front tile 841 -> event 88 -> eligible EV_DIALOG + EV_NEXTSTATE
front tile 936 -> event 91 -> locked line258 + eligible EV_OPENLINE(258)
```

The resolver is read-only today and gameplay correctly reports `SELECT semantic-not-enabled`.

## Recommended next milestone

Native Action/SELECT execution on Entrance, using only already recovered bounded native semantics:

```text
SELECT intent
 -> resolver provenance
 -> lock/key guard
 -> supported native opcode/UI family
 -> mutation/UI intent
 -> correct turn/redraw behavior
 -> unsupported semantic => fail closed
```

Do not enable all of legacy `Game_executeEvent`.

## Milestone archive groups

The many `MAP1_*.md` files remain useful as evidence, but recovery should start from `NATIVE_ENGINE_RECOVERY.md` rather than replaying them chronologically.

### Runtime/data

Structural BSP/load/inventory, native runtime/access/state/events/descriptor/filter.

### Event/script semantics

Opcode executor, UI/string/status/dialog/notebook, key/password, line door/unlock, automap/save/change-map/show-hide/exit-state families.

### Transition/player

Transition preflight, resident handoff/committed transition, spawn/player view, HUD refresh/player setup/tile-enter/facing/post-load.

### Render/performance

Graphics catalog, first frame, sprites/glows, gameplay hotpath, resident small cache, exact 2048-B large-range cache.

### Gameplay

HUD, touch input, TURN, MOVE/collision, closed/dynamic line collision, SELECT front-tile provenance.

## Stable Entrance canons

```text
sourceBytes=21823
crc32=623f34e4
sourceFNV=d5cc751f
runtimeFNV=c3882516
snapshotFNV=b3811f3d
mapFNV=cd99b98e
scriptFNV=f9e3d9df
lineFNV=e5e74861
textureFNV=f1fc1875
automapFNV=669b1aa7
topologyFNV=3f321e43
spawn=904
direction=64
```

## Historical Junction canons remain valid

Junction is a second hardware corpus for the same engine, not the engine identity.

```text
sourceFNV=fefaf5ca
runtimeFNV=bc432a0f
lineState baseline FNV=3658710d
fresh player=992,1888,36
angle=64
tile=943
HUD frame=ba3e5182
HUD viewport=9206eb24
HUD bands=6c2aa46f
HUD stateFNV=4756db9c
```

## Permanent invariants

```text
classic CYD / ESP32-D0WD-V3
4 MB flash
no PSRAM
160x120 RGB565 framebuffer = 38400 B
shapeData=NULL
mediaTexels=NULL
runtime ZIP forbidden for migrated paths
/DoomRPG-ESP32.pak is native backing store
legacy Game.entities=0
legacy Game.monsters=0
unsupported semantic families fail closed
```

## Merge boundary

```text
hardware-tested implementation SHA = 300561cfc9b4d06af769fda54613d837fa738f58
REAL-CYD HARDWARE PASS
Entrance visible and walkable = YES
sprites/HUD/touch/TURN/MOVE/collision = YES
render cache lifecycle = YES
MERGE-READY = YES
```

All commits after `300561cfc9b4d06af769fda54613d837fa738f58` are documentation-only closeout. After merge, recover the exact new `main` SHA before creating the next `agent/*` branch.
