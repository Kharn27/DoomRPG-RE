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
| [`MAP1_NATIVE_PLAYER_VIEW.md`](MAP1_NATIVE_PLAYER_VIEW.md) | permanent active Junction player/view owner | #70 | `8a82891bb8d9c62582170cc4b3b74d270849e77b` |

Older milestone archives remain in this directory and are indexed by Git history. `PORTING_STATUS.md` is the preferred recovery point.

## Latest merged boundary

PR #70 hardware-proved the first permanent active native player/view owner on committed Junction:

```text
EspPlayerViewState = 44 B
persistent heap = 0 B
stateFNV=d1131d18
view/dest=992/1888
z=36
angle=64
viewZOld=4
hudRefreshPending=1
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
```

Junction remains resident at:

```text
snapshotFNV=bc9071e9
payload=10410 B
actual heap=10540 B
entities=30 enemies=0 destructibles=3
```

## Current merge-ready milestone

[`MAP1_NATIVE_HUD_REFRESH.md`](MAP1_NATIVE_HUD_REFRESH.md) hardware-proves the semantic post-spawn HUD refresh handoff.

```text
branch = agent/esp32-native-post-spawn-refresh
base   = 8a82891bb8d9c62582170cc4b3b74d270849e77b
hardware-tested firmware = 1761e2929ec50260a1f27373ce477530b84d041a
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

### Permanent HUD API

```text
EspHudRefreshState = 8 B
persistent heap = 0 B

EspHudRefresh_reset
EspHudRefresh_isReady
EspHudRefresh_view
EspHudRefresh_preparePostSpawn
EspHudRefresh_routePostSpawn
```

Real-CYD owner:

```text
stateFNV=6965ee06
reason=POST_SPAWN
refreshPending=1
routed=1
active=1
targetMap=9
gameplayLoadMapId=2
loadType=0
```

### Player/view ownership transfer

The player/view ABI remains 44 B. Routing the HUD dirty semantic atomically clears only the HUD bit:

```text
beforeFNV=d1131d18
afterFNV=d17fa0d1
hudRefreshPending=0
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
placementExact=yes
```

Both `6965ee06` and `d17fa0d1` are hardware canons.

### Corrected legacy post-spawn order

Fresh-map order is:

```text
Game_spawnPlayer:
  placement
  Hud.isUpdate=true
  first checkFacingEntity()       # previous orientation vectors
  Player_setup()
  initial Game_executeTile(...)

DoomCanvas_finishRotation():
  recalc viewSin/viewCos/viewStep
  Game_executeTile(... | 0x400)
  final checkFacingEntity()       # durable facing
```

The first facing result is transitory and is not read by `Player_setup()` or the intervening tile execution. Native facing therefore remains pending until after setup/tile effects and orientation preparation. The current milestone deliberately does not materialize a premature facing query.

### Hardware fail-closed proof

```text
nullView=1
nullOutput=1
inactive=1
loadType=1
missingHud=1
missingFacing=1
reset=1
prepareAtomic=yes
repeat=1
repeatAtomic=yes
```

### Resident / RAM integrity

```text
snapshotFNV=bc9071e9->bc9071e9
targetLeftResident=yes
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes

heap8=72940->72940
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Stable heartbeat:

```text
heap=138704
heap8=72940
largest8=34804
```

### Legacy / framebuffer integrity

Same-build witnesses:

```text
placementFNV=5d1076bf->5d1076bf
playerFNV=a1725bcb->a1725bcb
frameFNV=7a95b5b5->7a95b5b5
legacyRuntimeClear=yes
DoomCanvasMutation=no
GameMutation=no
PlayerMutation=no
RenderMutation=no
HudMutation=no
```

These are same-build equality witnesses, not cross-build canons.

Final hardware PARK:

```text
state=9 / ST_INTRO
page=3
mapSwapCommitted=yes
targetMap=9
junctionResident=yes
nativePlayerView=yes
nativeHudRefresh=yes
hudDirty=yes
hudRouted=yes
hudRefreshPending=0
facingPending=yes
playerSetupPending=yes
tileEnterPending=yes
ST_PLAYING=no
legacy entities=0
legacy monsters=0
noGameplay=yes
```

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
postHudPlayerViewFNV=d17fa0d1
hudRefreshBytes=8
JunctionHudRefreshFNV=6965ee06
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
 -> post-spawn HUD dirty ownership                [hardware-proven]
 -> Player_setup-equivalent initialization
 -> initial tile-enter
 -> finishRotation-equivalent orientation + tile-facing event
 -> durable facing query
 -> ST_PLAYING / native gameplay/render loop
```

Still outside:

```text
actual stats-menu rendering/input
actual HUD rendering / renderer dirty consumption
native Player_setup-equivalent initialization
initial tile-enter execution
finishRotation-equivalent orientation preparation
final native facing-entity query
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

The player/view and HUD owners deliberately have different lifetimes from the map-resident arena and are not part of the seven-owner resident snapshot.

`shapeData == NULL` and `mediaTexels == NULL` remain mandatory.

## Next bounded milestone after merge

Recover from the true post-merge `main` before implementation. The natural next semantic is the native `Player_setup()` equivalent. Keep initial tile-enter, finishRotation/final-facing and `ST_PLAYING` as separate boundaries unless a fresh legacy audit proves a tighter safe grouping.

## Merge recommendation

```text
MERGE agent/esp32-native-post-spawn-refresh
```

Hardware-tested firmware:

```text
1761e2929ec50260a1f27373ce477530b84d041a
```

Every later commit on this branch must remain documentation-only unless another firmware is flashed.
