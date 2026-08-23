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

Older milestone archives remain in this directory and are indexed by Git history. `PORTING_STATUS.md` is the preferred recovery point.

## Latest merged boundary

PR #68 hardware-proved the first committed native resident map replacement:

```text
EspMapCommittedTransitionState = 24 B
WAIT_STATS FNV  = 66fe636a
READY FNV       = 0ef58ea8
ROLLED_BACK FNV = 2dec1442
COMMITTED FNV   = 2c595a62

mapSwapCommitted=yes
sourceMap=1
targetMap=9
junctionResident=yes
sourceRestored=no
targetLeftResident=yes
spawnParam=0 retained
spawnApplied=no
ST_PLAYING=no
```

Junction resident canon:

```text
snapshotFNV=bc9071e9
payload=10410 B
actual heap=10540 B
runtime/map/script/line/texture/automap/topology FNVs:
bc432a0f / c5cdfc04 / bc9b18ff / 3658710d / 537319ad / 0b2ae445 / d6e8df7d
compact entities=30 enemies=0 destructibles=3
```

## Current merge-ready milestone

[`MAP1_NATIVE_JUNCTION_SPAWN.md`](MAP1_NATIVE_JUNCTION_SPAWN.md) hardware-proves a permanent pointer-free projection of recovered `Game_spawnPlayer()` placement for the already committed Junction map.

```text
branch = agent/esp32-native-junction-spawn
base   = 00268a100c6662cb883f9a02d979b4f29eecbf12
hardware-tested firmware = 08a3a29c5e4e4a64000fa12a877299bbb1e772a0
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

### Permanent API

```text
EspPlayerSpawnState = 24 B
persistent heap = 0 B

EspPlayerSpawn_reset
EspPlayerSpawn_prepareCommitted
```

The API requires a COMMITTED transition plus a complete target inventory matching the current resident runtime. It supports only the ordinary fresh-map load context:

```text
loadType=0
gameIsLoaded=0
```

Saved-game restoration remains fail-closed.

### Hardware-proven real Junction placement

Committed transition:

```text
spawnParam=0
```

Junction header:

```text
spawnIndex=943
spawnDirection=64
```

Real CYD state:

```text
stateBytes=24
stateFNV=ba6af4a7
targetMap=9
gameplayLoadMapId=2
source=HEADER
tileIndex=943
tile=15/29
world=992/1888
angle=64
viewZ=36
viewZOld=4
loadType=0
active=1
```

`ba6af4a7` is now a hardware canon.

### Hardware-proven packed override

Probe-local committed-state copy:

```text
spawnParam=00030167
tileIndex=359
tile=7/11
world=480/736
angle=192
source=OVERRIDE
overrideUsed=1
headerIgnored=yes
stateFNV=e0a5110b
```

`e0a5110b` is now a hardware canon. The real transition was not changed.

### Follow-up boundary

The projection records but does not execute:

```text
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
spawnApplied=no
facingApplied=no
playerSetupApplied=no
tileEnterApplied=no
```

Therefore this milestone still does not call or emulate:

```text
DoomCanvas_checkFacingEntity
Player_setup
initial Game_executeTile
ST_PLAYING
```

### Fail-closed hardware proof

```text
nullTransition=1
nullInventory=1
nullOutput=1
notCommitted=1
loadType=1
loadedWorld=1
targetMismatch=1
runtimeMismatch=1
badHeaderSpawn=1
reset=1
outputAtomic=yes
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

heap8=73012->73012
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Same firmware prerequisite committed transition:

```text
sourceHeap=65544
targetHeap=73012
targetHeapGain=7468
largest=34804->34804
```

The absolute build baseline shifted by 40 B versus the previous PR #68 test, while exact resident costs stayed unchanged:

```text
Entrance heap=18008 B
Junction heap=10540 B
free-heap gain=7468 B
```

### Legacy / framebuffer integrity

Same-probe witnesses:

```text
placementFNV=5d1076bf->5d1076bf
playerFNV=a1725bcb->a1725bcb
frameFNV=833705d2->833705d2
legacyRuntimeClear=yes
DoomCanvasMutation=no
GameMutation=no
PlayerMutation=no
RenderMutation=no
HudMutation=no
```

Final hardware PARK:

```text
state=9 / ST_INTRO
page=3
committedTransition=yes
mapSwapCommitted=yes
targetMap=9
junctionResident=yes
nativeSpawnState=yes
spawnProjected=yes
spawnApplied=no
loadType=0
facingPending=yes
playerSetupPending=yes
tileEnterPending=yes
ST_PLAYING=no
legacy entities=0
legacy monsters=0
noGameplay=yes
```

Stable heartbeat after the probe:

```text
heap=138776
heap8=73012
largest8=34804
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
 -> native player/view application
 -> facing/setup/tile-enter ownership
 -> native gameplay/render loop
```

Still outside:

```text
actual stats-menu rendering/input
application of projected spawn coordinates
native facing-entity query
Player_setup-equivalent native initialization
initial tile-enter execution
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

`shapeData == NULL` and `mediaTexels == NULL` remain mandatory.

## Next bounded milestone after merge

Recover from the true post-merge `main` before implementation. The likely next milestone is a small native player/view owner that applies the already hardware-proven `(992,1888,64)` placement without yet opening facing/setup/tile-enter or `ST_PLAYING`; exact scope must be chosen from the merged repo and legacy audit.

## Merge recommendation

```text
MERGE agent/esp32-native-junction-spawn
```

Hardware-tested firmware:

```text
08a3a29c5e4e4a64000fa12a877299bbb1e772a0
```

Every later commit on this branch must remain documentation-only unless another firmware is flashed.
