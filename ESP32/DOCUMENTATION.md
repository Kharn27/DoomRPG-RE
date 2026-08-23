# ESP32 documentation map

This file defines the current ESP32 CYD documentation map.

## Source of truth

- [`README.md`](README.md): stable build/flash guide.
- [`PORTING_STATUS.md`](PORTING_STATUS.md): authoritative current recovery point.
- Milestone archives: detailed implementation and hardware evidence.

## Recent merged milestones

| Archive | Purpose | PR | Merged `main` |
| --- | --- | ---: | --- |
| [`MAP1_NATIVE_CHANGE_MAP_INTENT.md`](MAP1_NATIVE_CHANGE_MAP_INTENT.md) | CHANGEMAP pending transition intent | #61 | `fc39ac60757e0d992e3729a5044a9d83e9994971` |
| [`MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md`](MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md) | SHOW/HIDE compact topology; all MAP_INTRO opcode families owned | #62 | `ed5cd9a09c9ae36f999661f4284f64400681b1af` |
| [`MAP1_NATIVE_LEVEL_EXIT_STATS.md`](MAP1_NATIVE_LEVEL_EXIT_STATS.md) | map-derived level-exit stats | #63 | `533784b5483e14a12558fb08c9331d8b744caa88` |
| [`MAP1_NATIVE_PLAYER_EXIT_STATE.md`](MAP1_NATIVE_PLAYER_EXIT_STATE.md) | pointer-free player exit writes | #64 | `3759bcd12a3f6d36a6a696457110ab27474c24b8` |
| [`MAP1_NATIVE_STATS_MENU_INTENT.md`](MAP1_NATIVE_STATS_MENU_INTENT.md) | LEVEL/OVERALL stats-menu intent | #65 | `c8679133351fa00e01a67103386b7676660c4a6e` |
| [`MAP1_NATIVE_TRANSITION_PREFLIGHT.md`](MAP1_NATIVE_TRANSITION_PREFLIGHT.md) | map catalog + Junction PAK/BSP preflight | #66 | `9f981f490282200f216aef66d22608d2244beb00` |
| [`MAP1_NATIVE_RESIDENT_HANDOFF.md`](MAP1_NATIVE_RESIDENT_HANDOFF.md) | reversible full resident Entrance/Junction handoff | #67 | `fddae899fd7dc01b20cf6bd532489326380954e3` |
| [`MAP1_NATIVE_COMMITTED_TRANSITION.md`](MAP1_NATIVE_COMMITTED_TRANSITION.md) | transactional committed Entrance -> Junction resident swap | #68 | `00268a100c6662cb883f9a02d979b4f29eecbf12` |
| [`MAP1_NATIVE_JUNCTION_SPAWN.md`](MAP1_NATIVE_JUNCTION_SPAWN.md) | fresh-map Junction spawn/load projection | #69 | `992f38374840113409e776fb82ce57ab014607e5` |

Older milestone archives remain in this directory and are indexed by Git history. `PORTING_STATUS.md` is the preferred recovery point.

## Latest merged boundary

PR #69 hardware-proved deterministic fresh-map player placement for the committed Junction resident map:

```text
EspPlayerSpawnState = 24 B
Junction spawn FNV = ba6af4a7
packed override FNV = e0a5110b
world=992/1888
angle=64
viewZ=36
viewZOld=4
loadType=0
```

Junction remains resident at:

```text
snapshotFNV=bc9071e9
payload=10410 B
actual heap=10540 B
entities=30 enemies=0 destructibles=3
```

## Current merge-ready milestone

[`MAP1_NATIVE_PLAYER_VIEW.md`](MAP1_NATIVE_PLAYER_VIEW.md) hardware-proves the first permanent active native player/view owner on committed Junction.

```text
branch = agent/esp32-native-player-view
base   = 992f38374840113409e776fb82ce57ab014607e5
hardware-tested firmware = fe1630ad5618dfd35bbbc555de8f9762d0b046f8
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

### Permanent API

```text
EspPlayerViewState = 44 B
persistent heap = 0 B

EspPlayerView_reset
EspPlayerView_isReady
EspPlayerView_view
EspPlayerView_applySpawn
```

The owner mirrors the legacy-width placement fields:

```text
viewX/viewY/viewZ/viewAngle
destX/destY/destAngle
viewZOld
```

and owns target/load identity plus explicit pending follow-ups.

### Recovered exact boundary

Recovered `Game_spawnPlayer()` ordering is:

```text
apply coordinates/angle/z
Render.viewZOld = 4
Hud.isUpdate = true
DoomCanvas_checkFacingEntity()
if fresh map:
    Player_setup()
    initial Game_executeTile()
```

The milestone applies only the native player/view portion and records:

```text
hudRefreshPending=1
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
```

without mutating legacy Hud/DoomCanvas/Render/Game/Player.

### Hardware-proven real Junction player/view

Real CYD:

```text
stateBytes=44
stateFNV=d1131d18
view=992/1888/36
viewAngle=64
dest=992/1888
destAngle=64
viewZOld=4
targetMap=9
gameplayLoadMapId=2
loadType=0
active=1
spawnApplied=1
```

`d1131d18` is now a hardware canon.

Follow-ups remain pending:

```text
hudRefresh=1
facingRefresh=1
playerSetup=1
tileEnter=1
hudApplied=no
facingApplied=no
playerSetupApplied=no
tileEnterApplied=no
```

### Hardware-proven packed override view

```text
param=00030167
view=480/736/36
angle=192
dest=480/736
destAngle=192
stateFNV=9ed47d08
sourceProjectionFNV=e0a5110b
```

`9ed47d08` is now a hardware canon.

### Fail-closed hardware proof

```text
nullSpawn=1
inactive=1
badGeometry=1
badPending=1
repeat=1
repeatAtomic=yes
reset=1
stateAtomic=yes
```

The active owner refuses a second spawn application without mutation.

### Resident / RAM integrity

```text
snapshotFNV=bc9071e9->bc9071e9
targetLeftResident=yes
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes

heap8=72956->72956
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Stable heartbeat:

```text
heap=138720
heap8=72956
largest8=34804
```

### Legacy / framebuffer integrity

Same-build witnesses:

```text
placementFNV=5d1076bf->5d1076bf
playerFNV=a1725bcb->a1725bcb
frameFNV=a3e3cc8e->a3e3cc8e
legacyRuntimeClear=yes
DoomCanvasMutation=no
GameMutation=no
PlayerMutation=no
RenderMutation=no
HudMutation=no
```

These hashes are same-build witnesses, not cross-build canons.

Final hardware PARK:

```text
state=9 / ST_INTRO
page=3
mapSwapCommitted=yes
targetMap=9
junctionResident=yes
nativeSpawnState=yes
nativePlayerView=yes
spawnAppliedNative=yes
legacySpawnApplied=no
hudRefreshPending=yes
facingPending=yes
playerSetupPending=yes
tileEnterPending=yes
ST_PLAYING=no
legacy entities=0
legacy monsters=0
noGameplay=yes
```

This is the first hardware-proven active native player/view state on a non-Entrance committed map.

## Hardware-proven canons through current milestone

```text
Entrance snapshotFNV=b3811f3d
Entrance heap=18008 B
Junction snapshotFNV=bc9071e9
Junction heap=10540 B
Junction sourceFNV=fefaf5ca
catalogFNV=ce322e3f
preflightFNV=108e5c7b
statsMenuIntentFNV=96afe901
levelExitStatsFNV=bd41bcfa
playerExitAppliedFNV=298eaaa4
committedTransitionFNV=2c595a62
playerSpawnBytes=24
JunctionSpawnFNV=ba6af4a7
packedOverrideFNV=e0a5110b
playerViewBytes=44
JunctionPlayerViewFNV=d1131d18
packedOverrideViewFNV=9ed47d08
```

## Architecture direction

```text
original Doom RPG behavior/data
 -> native pack-backed parsers
 -> compact immutable map + explicit mutable owners
 -> native event/script ownership
 -> exit chain
 -> map catalog/preflight
 -> explicit resident lifecycle
 -> reversible full resident handoff              [hardware-proven]
 -> committed stats-ack-gated transition          [hardware-proven]
 -> fresh-map load semantic                       [hardware-proven]
 -> native player spawn projection                [hardware-proven]
 -> permanent native player/view application      [hardware-proven]
 -> HUD refresh / facing query
 -> Player_setup-equivalent initialization
 -> initial tile-enter
 -> ST_PLAYING / native gameplay/render loop
```

Still outside:

```text
actual stats-menu rendering/input
HUD refresh consumption
native facing-entity query
Player_setup-equivalent native initialization
initial tile-enter execution
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

The player/view owner deliberately has a different lifetime from map-resident arena state and is not reset by `EspMapResidentLifecycle_resetAll()`.

`shapeData == NULL` and `mediaTexels == NULL` remain mandatory.

## Next bounded milestone after merge

Recover from the true post-merge `main` before implementation. The natural next small boundaries are explicit HUD-refresh consumption and compact-topology facing lookup. Keep Player_setup, tile-enter and ST_PLAYING separate unless a fresh legacy audit proves otherwise.

## Merge recommendation

```text
MERGE agent/esp32-native-player-view
```

Hardware-tested firmware:

```text
fe1630ad5618dfd35bbbc555de8f9762d0b046f8
```

Every later commit on this branch must remain documentation-only unless another firmware is flashed.
