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
PR   = #104 — native dialog font hotpath
main = 37d4bc760e0715216f1adaea9d69548a5ab31ab7
status = MERGED
```

## Current candidate boundary

```text
branch = agent/esp32-native-gameplay-force-message
base main = 37d4bc760e0715216f1adaea9d69548a5ab31ab7
hardware-tested implementation SHA = 0c6f7ffac0e3cf47a60aa2315da04f551d2a51bc
status = REAL-CYD HARDWARE PASS
merge-ready = YES
post-test commits = documentation-only
```

Latest hardware archive:

- [`MAP1_NATIVE_GAMEPLAY_INTERACTION_EXPANSION.md`](MAP1_NATIVE_GAMEPLAY_INTERACTION_EXPANSION.md) — production FORCE_MESSAGE/NOTE interaction ownership, bounded mixed dialog continuations, native weapon presentation/pickup ownership, systematic interaction inventory, and ESP32 mappings reload peak recovery.

Previous relevant archives:

- [`MAP1_NATIVE_DIALOG_FONT_HOTPATH.md`](MAP1_NATIVE_DIALOG_FONT_HOTPATH.md)
- [`MAP1_NATIVE_GAMEPLAY_DIALOG_RESUME.md`](MAP1_NATIVE_GAMEPLAY_DIALOG_RESUME.md)
- [`MAP1_NATIVE_GENERIC_DOOR_ANIMATION.md`](MAP1_NATIVE_GENERIC_DOOR_ANIMATION.md)
- [`MAP1_NATIVE_GENERIC_DOOR_MOVE_CLOSE.md`](MAP1_NATIVE_GENERIC_DOOR_MOVE_CLOSE.md)
- [`MAP1_NATIVE_STATUS_MESSAGE.md`](MAP1_NATIVE_STATUS_MESSAGE.md) — earlier probe-level FORCE_MESSAGE semantic owner; superseded for live gameplay ownership by the current candidate.

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

Historical `MAP1_*`, `ENTRANCE*` and `JUNCTION*` names are evidence only. After startup validation, probes are witnesses only and cannot stop live gameplay.

## Current real-CYD gameplay proof

Entrance startup/playback canon:

```text
file=/intro.bsp
name=Entrance
resourceMapId=1
spawn tile=904
position=544,1824,36
angle=64
```

The generic resident session has reached and retained:

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
EV_DIALOG / EV_DIALOGNOBACK presentation = YES
progressive dialog text / paging / fast-forward = YES
dialog close + script resume = YES
generic indexed-BMP dialog font hotpath = YES
EV_FORCEMESSAGE resident top-bar fallback = YES
EV_NOTE bounded prefix before dialog = YES
mixed saved dialog continuation = YES
post-dialog SHOW/HIDE/UNLOCK = YES
native idle first-person weapon painting = YES
eType=5 weapon remove/ownership/auto-select = YES
resident interaction/pickup corpus = YES
ESP32 mappings reload peak guard = YES
shapeData = NULL
mediaTexels = NULL
legacy entities = 0
legacy monsters = 0
```

The current candidate hardware boundary is tester-accepted at implementation SHA `0c6f7ffac0e3cf47a60aa2315da04f551d2a51bc`. Exact serial values not supplied for that final accepted run are not invented in the documentation.

## Current gameplay boundary

Production-enabled:

```text
resident load / spawn / player-view
planes / walls / sprites / glows / HUD
12-zone calibrated touch + transient feedback
TURN_LEFT / TURN_RIGHT
FORWARD / BACK / STRAFE
static/entity/line collision
dynamic per-line collision
SELECT front-tile resolver/provenance
bounded SELECT EV_OPENLINE / EV_CLOSELINE
bounded MOVE source EXIT + destination ENTER event execution
regular-door visual interpolation
EV_DIALOG / EV_DIALOGNOBACK presenter
native dialog owner/typewriter/paging/fast-forward/close
EV_FORCEMESSAGE top-bar owner/painter
EV_NOTE prefix into owned dialog
saved dialog continuation cap = 12 eligible commands
post-dialog EV_SHOW / EV_HIDE / EV_UNLOCK
post-dialog state ops 11 / 19 / 20
native idle first-person weapon renderer
post-move eType=5 weapon consume/own/select overlay
one-shot opcode/pickup interaction inventory
```

The engine still does not broad-enable legacy `Game_executeEvent`.

## Saved dialog continuation semantics

The old production path accepted only zero-or-one state opcode after dialog close. The current candidate preflights and executes a bounded continuation using one fixed event-state/run-flags snapshot, matching the recovered legacy `Game_runEvent()` pause/resume model.

Owned continuation opcode families:

```text
7  EV_SHOW
18 EV_HIDE
13 EV_UNLOCK
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

Maximum eligible continuation commands:

```text
12
```

Rollback covers removed bits, state changes, line lock/texture state and SHOW/HIDE topology. The transaction journal and topology snapshot are lazy-gameplay owners.

The real Entrance event 60 is the motivating mixed corpus: DIALOG can be followed by SHOW + state changes; a later DIALOGNOBACK path can be followed by HIDE + state changes + UNLOCK. Unsupported eligible commands still fail closed before UI opens.

## FORCE_MESSAGE / NOTE production ownership

`EV_FORCEMESSAGE` now owns the resident 160x20 status fallback rather than only a probe-level semantic state. It keeps a compact map string reference and bounded 384-B scratch, with at most 21 visible characters. Painting uses `k.bmp` + `a.bmp` and does not mutate legacy `Hud`.

`EV_NOTE` is a bounded prefix immediately before an owned dialog interaction. Its notebook/scratch owner is allocated lazily only when first needed.

## Native weapon / pickup boundary

Idle first-person weapon presentation is native and PAK-backed:

```text
logical sprite = 240 + weapon
shapeData = NULL
mediaTexels = NULL
```

Post-move pickup ownership currently supports only legacy `eType=5` weapon entities:

```text
world remove = consumed-bit overlay
ownership = uint16 native mask
new weapon select = native HUD overlay
rerender failure = exact pickup rollback
owner scope = runtime arena FNV/map
```

Still deferred:

```text
weapon ammo increment from EntityDef.parm
weapon first-acquisition popup/dialog
weapon pickup sound
eType 3 player-stat/world item
eType 4 inventory item
eType 6 ammo
eType 16 alternate ammo
```

These are explicit semantic boundaries, not bugs hidden by fallback to legacy entities.

## Interaction inventory / opcode frontier

Known Entrance opcode IDs:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

The candidate logs a one-shot read-only interaction inventory identifying owned and deferred event/pickup families.

Still production-deferred/fail-closed:

```text
2  EV_CHANGEMAP  -> transition consumer not promoted here
9  EV_GIVEMAP    -> automap production route not promoted here
10 EV_PASSWORD   -> password input UI missing
27 EV_SAVEGAME   -> save consumer missing
41 EV_CHECK_KEY  -> native player key owner missing
```

Existing earlier probes/intents for these commands remain references only.

## Final no-PSRAM startup correction

The hardware run immediately before the accepted candidate exposed this deterministic failure:

```text
first mappings load succeeds
heap8 = 34036
largest8 = 9716
mapping payload = 8376
Render_beginLoadMap(MAP_MENU)
 -> second Render_loadMappings()
 -> out of memory allocating inflate state for mappings.bin
```

Root cause: legacy `Render_loadMappings()` opens/inflates the replacement file before freeing its four currently resident mapping arrays.

The current ESP32-only guard releases exactly those rebuildable immutable arrays before the real `Render_beginLoadMap()`:

```text
mediaTexelOffsets
mediaBitShapeOffsets
mediaTexturesIds
mediaSpriteIds
```

The legacy parser still reloads/rebuilds the exact same mappings. This changes the allocation peak, not the file format or semantics.

The dialog-chain transaction journal was also moved to lazy-gameplay allocation during the same closeout. The corrected normal `esp32-cyd` firmware at SHA `0c6f7ffa...` was accepted by the hardware tester.

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

## Generic door proof remains canonical

Entrance OPEN:

```text
SELECT tile837 -> event86 -> EV_OPENLINE line275 -> 0->1
```

MOVE CLOSE:

```text
EXIT tile838 flags=0x00000420 -> event87 -> EV_CLOSELINE line275 -> 1->0
```

Regular-door animation:

```text
OPEN  6c5debde -> 2d05fe08 -> a522f925 -> 7105fa5f
CLOSE 35e3784d -> d005cd93 -> 808e96c7 -> 808e96c7
```

Permanent animator owner = 76 B BSS, max 8 lines, immutable `EspMapRuntime`.

## Native dialog baseline remains canonical

Previously proven Entrance event 88:

```text
state 0: EV_DIALOG -> EV_NEXTSTATE
state 1: EV_DIALOG -> EV_CHANGESTATE
```

Known dialog visual fingerprints:

```text
page1 complete = 1cf6fa50
page transition/start = 35de63a8
page2 complete = 0741a2e6
world after resume = ed061192
```

The grouped indexed-BMP hotpath from merged PR #104 remains the accepted font path.

## Render-cache canon

```text
owner=21160 B
payload=16384 B
SMALL-COLD=2119886 us
SMALL-WARM=256807 us
LARGE-LEARN=247770 us
LARGE-WARM=229719 us
large entries=2
```

The plane renderer uses bounded leases rather than one contiguous map-wide texture allocation. `PlatformVideo_present()` remains around 34 ms and should not be optimized without evidence.

## Event/script executor boundary

Generic `EspMapOpcodeExecutor` remains intentionally limited to:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

Door, UI, topology and pickup production paths are separate bounded native semantic owners around resident event/filter provenance.

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

## Recommended next direction after merge

Do not broaden arbitrary event execution. Recover the exact merged `main` first, then choose one coherent bounded family from the current explicit frontier.

Strong candidates:

```text
complete eType=5 weapon pickup parity:
  EntityDef.parm ammo + compact ammo owner + acquisition feedback

or

promote one remaining event family with its missing consumer/owner:
  CHECK_KEY + native player keys
  PASSWORD + input UI
  GIVEMAP + production automap route
```

Keep renderer performance work separate unless profiling shows it is the blocking semantic milestone.

## Merge boundary

```text
base main = 37d4bc760e0715216f1adaea9d69548a5ab31ab7
hardware-tested implementation SHA = 0c6f7ffac0e3cf47a60aa2315da04f551d2a51bc
REAL-CYD HARDWARE PASS
FORCE_MESSAGE production = YES
NOTE prefix production = YES
bounded mixed dialog continuation = YES
SHOW/HIDE/UNLOCK continuation = YES
native idle weapon = YES
eType=5 remove/ownership/select = YES
mappings reload peak guard = YES
unsupported families remain fail closed = YES
immutable runtime = YES
MERGE-READY = YES
```

All commits after `0c6f7ffac0e3cf47a60aa2315da04f551d2a51bc` are documentation-only closeout. After merge, recover the exact new `main` SHA before creating the next `agent/*` branch.
