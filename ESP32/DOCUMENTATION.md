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
PR   = #102 — native Action/regular-door execution
main = a8e0b64dfd9c790f8896279f70427ce6fb3e9859
status = MERGED
```

## Current candidate boundary

```text
branch = agent/esp32-native-action-dialog-resume
base main = a8e0b64dfd9c790f8896279f70427ce6fb3e9859
hardware-tested implementation SHA = 5c53a9a02bfb7c92e4dccb0b6eba424e7d015a9b
status = REAL-CYD HARDWARE PASS
merge-ready = YES
post-test commits = documentation-only
```

Latest hardware archive:

- [`MAP1_NATIVE_GAMEPLAY_DIALOG_RESUME.md`](MAP1_NATIVE_GAMEPLAY_DIALOG_RESUME.md) — production `EV_DIALOG` pause/typewriter/paging/fast-forward/close + bounded state-only resume, including repeated event state 0 -> 1 -> 0.

Previous relevant door archives:

- [`MAP1_NATIVE_GENERIC_DOOR_MOVE_CLOSE.md`](MAP1_NATIVE_GENERIC_DOOR_MOVE_CLOSE.md)
- [`MAP1_NATIVE_GENERIC_DOOR_ANIMATION.md`](MAP1_NATIVE_GENERIC_DOOR_ANIMATION.md)

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
 -> generic renderer/HUD/input/collision/events/actions/dialog UI
```

Historical `MAP1_*`, `ENTRANCE*` and `JUNCTION*` probes/log labels remain evidence only.

A second permanent conclusion is now hardware-proven:

```text
before TransitionPreflightFinal:
  startup regression probes may fail-closed the startup validation

after TransitionPreflightFinal:
  probes are witnesses only
  EspNativeGameplaySession owns runtime authority
```

A historical probe must never stop a live gameplay session again.

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
resident render caches = YES
touch = YES
TURN/MOVE = YES
native collision = YES
SELECT front-tile provenance = YES
SELECT regular-door execution = YES
MOVE EXIT/ENTER regular-door events = YES
regular-door animation = YES
SELECT EV_DIALOG = YES
progressive dialog text = YES
Action fast-forward = YES
4-line paging = YES
dialog close + script resume = YES
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
12-zone calibrated touch + transient 120-ms feedback
TURN_LEFT/TURN_RIGHT
FORWARD/BACK/STRAFE
static/entity/line collision
dynamic per-line collision
SELECT front-tile resolver/provenance
bounded SELECT EV_OPENLINE/EV_CLOSELINE
bounded MOVE source EXIT + destination ENTER regular-door events
generic regular-door visual interpolation
bounded SELECT EV_DIALOG / EV_DIALOGNOBACK preflight path
native dialog owner/typewriter/paging/fast-forward/close
zero-or-one state-only dialog continuation using opcodes 11/19/20
```

The current hardware proof covers `EV_DIALOG` opcode 8. `EV_DIALOGNOBACK` is implemented in the same bounded family but is not yet claimed hardware-proven.

The engine still does **not** broad-enable legacy `Game_executeEvent`.

Semantically deferred / fail-closed:

```text
EV_FORCEMESSAGE / EV_NOTE production UI semantics
broad MOVE tile-event opcode execution
unbounded/mixed dialog continuations
secret/MOVELINE animation
door sound playback
legacy entity relink objects
PASS_TURN
menu/automap/weapon gameplay
combat / monsters / generic turn advance
unsupported opcode families
```

## Native dialog hardware proof

Real resident Entrance event 88, front tile 841:

```text
state 0:
  cmd0 opcode 8  EV_DIALOG string88 = ELIGIBLE
  cmd1 opcode 19 EV_NEXTSTATE       = ELIGIBLE

state 1:
  cmd2 opcode 8  EV_DIALOG string89 = ELIGIBLE
  cmd3 opcode 11 EV_CHANGESTATE     = ELIGIBLE
```

First dialog:

```text
string88
102 B
7 lines
4-line pages
back allowed
PAK open only during active dialog
```

Hardware behavior:

```text
Action during typewriter -> FASTFORWARD current page
next Action             -> PAGE start=4/7
Action on complete end  -> CLOSE packClosed=yes
resume                  -> EV_NEXTSTATE state 0->1
world redraw            -> success
```

Second SELECT at state 1 opens `string89`, then close resumes `EV_CHANGESTATE` state 1->0.

The tester repeated the complete pair several times successfully.

Stable dialog-run heartbeat:

```text
heap=96624
heap8=30916
largest8=16372
```

No stuck input, later `[NATIVEBOOT] BLOCKED`, Guru Meditation or reboot was reported.

## Dialog performance note

Correctness is accepted, but the tester reports some noticeable latency while text types and around Action clicks.

Measured scale:

```text
PlatformVideo_present ~= 34.3 ms
102-B / 7-line dialog:
  paints = 22
  fontReads = 3482 .. 3650
  resource bytes = 250626 .. 262722
DIALOG-RESUME world redraw ~= 206.7 ms
touch feedback hold = 120 ms
```

The thousands of font reads for a 102-byte source string are a concrete bounded optimization target. Prefer reducing redundant font/resource reads and presentation/recomposition cadence while preserving the recovered logical 25-ms-per-character timeline.

Do not prematurely optimize `PlatformVideo_present()` itself. Doom RPG is turn-based; redraw-on-demand remains the long-term direction.

## Generic door Action proof

Real resident Entrance data produced:

```text
SELECT front tile 837
 -> event 86
 -> line 275
 -> EV_OPENLINE
 -> open 0 -> 1
```

MOVE source EXIT later produced:

```text
EXIT tile 838 flags=0x00000420
 -> event 87
 -> EV_CLOSELINE line275
 -> open 1 -> 0
```

Regular-door animation remains hardware-valid:

```text
OPEN  6c5debde -> 2d05fe08 -> a522f925 -> 7105fa5f
CLOSE 35e3784d -> d005cd93 -> 808e96c7 -> 808e96c7
```

Permanent animator owner remains 76 B BSS, max 8 active lines, immutable `EspMapRuntime`.

## Render-cache hardware canon

```text
owner=21160 B
payload=16384 B
SMALL-COLD=2119886 us
SMALL-WARM=256807 us
LARGE-LEARN=247770 us
LARGE-WARM=229719 us
large entries=2
```

The plane renderer uses bounded 2048-B leases rather than requiring one contiguous map-wide texture allocation.

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

## Event/script executor boundary

Generic `EspMapOpcodeExecutor` remains intentionally limited to:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

Production door and dialog Action routes are separate bounded native semantics around resident event/filter provenance; they do not broad-enable the desktop executor.

Hardware-proven production UI opcode:

```text
8 EV_DIALOG
```

Implemented but not yet hardware-proven in this production path:

```text
26 EV_DIALOGNOBACK
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

## Recommended next direction

Correctness-wise, the current branch is a coherent merge boundary.

After merge and exact `main` recovery, a good next bounded milestone is one of:

```text
1. dialog performance: bounded font/resource cache + reduced redraw/present cadence
   while preserving 25-ms logical typewriter behavior

2. next UI opcode family from real resident data:
   EV_FORCEMESSAGE / EV_NOTE, still without broad Game_executeEvent
```

The performance path is particularly well motivated by the measured 3.5k font reads / ~250 KB resource traffic for a 102-B dialog.

## Milestone archive groups

### Runtime/data

Structural BSP/load/inventory, native runtime/access/state/events/descriptor/filter.

### Event/script semantics

Opcode executor, UI/string/status/dialog/notebook, key/password, line door/unlock, automap/save/change-map/show-hide/exit-state families.

### Transition/player

Transition preflight, resident handoff/committed transition, spawn/player view, HUD refresh/player setup/tile-enter/facing/post-load.

### Render/performance

Graphics catalog, first frame, sprites/glows, gameplay hotpath, resident caches, dynamic-line rendering, regular-door animation.

### Gameplay

HUD, touch input, TURN, MOVE/collision, SELECT provenance/action, bounded movement events, regular doors, native dialog/paging/resume.

## Merge boundary

```text
base main = a8e0b64dfd9c790f8896279f70427ce6fb3e9859
hardware-tested implementation SHA = 5c53a9a02bfb7c92e4dccb0b6eba424e7d015a9b
REAL-CYD HARDWARE PASS
Entrance visible/walkable/turnable = YES
sprites/HUD/touch/TURN/MOVE/collision = YES
SELECT regular doors = YES
MOVE regular-door events = YES
generic regular-door animation = YES
EV_DIALOG progressive UI = YES
fast-forward + paging = YES
dialog close + state continuation = YES
state 0->1->0 repeated = YES
historical probes cannot block live gameplay = YES
immutable runtime = YES
MERGE-READY = YES
```

All commits after `5c53a9a02bfb7c92e4dccb0b6eba424e7d015a9b` are documentation-only closeout. After merge, recover the exact new `main` SHA before creating the next `agent/*` branch.
