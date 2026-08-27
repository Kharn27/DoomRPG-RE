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
PR   = #101 — generic Entrance startup/gameplay route
main = 33b05385771b45acabff6dcf14d1da2c18d1818f
status = MERGED
```

## Current candidate boundary

```text
branch = agent/esp32-native-action-select-exec
base main = 33b05385771b45acabff6dcf14d1da2c18d1818f
hardware-tested implementation SHA = ed353c0799520b82464c0066c3a53c731488c168
status = REAL-CYD HARDWARE PASS
merge-ready = YES
post-test commits = documentation-only
```

Latest hardware archives:

- [`MAP1_NATIVE_GENERIC_DOOR_MOVE_CLOSE.md`](MAP1_NATIVE_GENERIC_DOOR_MOVE_CLOSE.md) — generic MOVE source/EXIT `EV_CLOSELINE` transaction.
- [`MAP1_NATIVE_GENERIC_DOOR_ANIMATION.md`](MAP1_NATIVE_GENERIC_DOOR_ANIMATION.md) — generic regular-door 4-frame visual interpolation on SELECT OPEN and MOVE CLOSE.

## Permanent architectural conclusion

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
 -> generic renderer/HUD/input/collision/events/actions
```

Historical `MAP1_*`, `ENTRANCE*` and `JUNCTION*` probes/log labels remain evidence only. New permanent runtime behavior must be map-generic and data-driven.

## Current real-CYD hardware proof

Entrance startup/playback canon:

```text
file=/intro.bsp
name=Entrance
resourceMapId=1
spawn tile=904
position=544,1824,36
angle=64
```

Generic session has reached:

```text
first world frame = YES
sprites/glows = YES
native HUD = YES
resident small cache = YES
exact 2048-B cache = YES
touch = YES
TURN/MOVE = YES
native collision = YES
SELECT front-tile provenance = YES
SELECT regular-door execution = YES
MOVE EXIT/ENTER regular-door events = YES
regular-door animation = YES
shapeData = NULL
mediaTexels = NULL
legacy entities = 0
legacy monsters = 0
```

## Current gameplay boundary

Production-enabled:

```text
resident load
spawn/player/view
planes/walls
sprites/glows
HUD
12-zone calibrated touch + transient feedback
TURN_LEFT/TURN_RIGHT
FORWARD/BACK/STRAFE
static/entity/line collision
dynamic per-line collision
SELECT front-tile resolver/provenance
bounded SELECT EV_OPENLINE/EV_CLOSELINE door semantics when eligible
bounded MOVE source EXIT + destination ENTER door events
generic regular-door visual interpolation
```

The current door executor remains intentionally narrow. It does not broad-enable legacy `Game_executeEvent`.

Semantically deferred / fail-closed:

```text
SELECT dialog/UI families
broad MOVE tile-event opcode execution
secret/MOVELINE animation
door sound playback
legacy entity relink objects
PASS_TURN
menu/automap/weapon gameplay
combat / monsters / turn advance
unsupported opcode families
```

## Generic door Action proof

Real resident Entrance data, not hard-coded gameplay, produced:

```text
SELECT front tile 837
 -> event 86
 -> line 275
 -> EV_OPENLINE
 -> open 0 -> 1
```

A SELECT issued at tile 838 sees event 87 / `EV_CLOSELINE`, but its flags do not match SELECT and the action correctly remains `NO_ELIGIBLE` with no mutation.

## Generic MOVE event proof

Backing away from tile 838 produced the bounded legacy EXIT/ENTER route:

```text
EXIT tile 838 flags=0x00000420
 -> event 87
 -> EV_CLOSELINE line275
 -> open 1 -> 0

ENTER tile 839 flags=0x80000408
 -> NO_EVENT
```

Both phases are preflighted before mutation. Unsupported/complex eligible commands keep MOVE deferred/fail-closed. The view/door transaction rollback lease closes only after the rendered frame succeeds.

## Generic regular-door animation proof

Hardware-tested SHA: `ed353c0799520b82464c0066c3a53c731488c168`.

Permanent owner:

```text
76 B BSS
max 8 active regular-door lines
4 total frames
3 moving frames
step 16
immutable EspMapRuntime
```

OPEN on line 275:

```text
6c5debde -> 2d05fe08 -> a522f925 -> 7105fa5f stable
```

CLOSE on line 275:

```text
35e3784d -> d005cd93 -> 808e96c7 -> 808e96c7 stable
```

Both report `generic=yes`, `immutableRuntime=yes`, `render=ok` and finish with `state=stable transaction=committed`.

The renderer applies animation only to transient line geometry. No map-wide legacy `Line_t`/`Vertex_t` ownership is introduced. World raster and sprite-depth see the same animated read view.

## Performance note

The hardware tester considers both door animation and general navigation a little slow, but correct.

Observed scale in the supplied run:

```text
PlatformVideo_present ~= 34 ms
complete gameplay redraw around current views ~= 0.32-0.35 s
```

This points future optimization toward recomposition/cache/redraw scheduling and redraw-on-demand rather than premature TFT-present tuning.

No timing change is part of this validated closeout.

## Render-cache hardware canon

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

The plane renderer does not require one contiguous 12288-B allocation; it uses bounded 2048-B leases.

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

Animation-run stable heartbeat:

```text
heap=97448
heap8=31740
largest8=8692
```

## Event/script executor boundary

Generic `EspMapOpcodeExecutor` remains intentionally limited to:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

Production regular-door Action/MOVE semantics are a separate bounded native route around resident event/filter provenance. Unsupported broad event execution remains fail-closed.

Two old production log tokens are now stale diagnostics, not current capability statements:

```text
animation=deferred
tileEvents=deferred
```

They are intentionally not changed after the hardware-tested SHA so the closeout remains docs-only.

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

## Recommended next direction

Correctness-wise, this branch has reached a coherent merge boundary. After merge and exact `main` recovery, choose one bounded next family from the real resident data rather than broadening everything at once.

Likely candidates:

```text
SELECT UI/dialog intent family
or
bounded redraw/performance work preserving turn-based redraw-on-demand architecture
```

Do not enable all of legacy `Game_executeEvent` and do not optimize `PlatformVideo_present()` merely because navigation currently feels slow.

## Milestone archive groups

### Runtime/data

Structural BSP/load/inventory, native runtime/access/state/events/descriptor/filter.

### Event/script semantics

Opcode executor, UI/string/status/dialog/notebook, key/password, line door/unlock, automap/save/change-map/show-hide/exit-state families.

### Transition/player

Transition preflight, resident handoff/committed transition, spawn/player view, HUD refresh/player setup/tile-enter/facing/post-load.

### Render/performance

Graphics catalog, first frame, sprites/glows, gameplay hotpath, resident small cache, exact 2048-B large-range cache, dynamic-line rendering, regular-door animation.

### Gameplay

HUD, touch input, TURN, MOVE/collision, SELECT provenance/action, bounded movement events, regular doors.

## Merge boundary

```text
base main = 33b05385771b45acabff6dcf14d1da2c18d1818f
hardware-tested implementation SHA = ed353c0799520b82464c0066c3a53c731488c168
REAL-CYD HARDWARE PASS
Entrance visible/walkable/turnable = YES
sprites/HUD/touch/TURN/MOVE/collision = YES
SELECT EV_OPENLINE = YES
MOVE EXIT EV_CLOSELINE = YES
generic regular-door animation = YES
immutable runtime = YES
MERGE-READY = YES
```

All commits after `ed353c0799520b82464c0066c3a53c731488c168` are documentation-only closeout. After merge, recover the exact new `main` SHA before creating the next `agent/*` branch.
