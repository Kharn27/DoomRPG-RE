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

PR #69 hardware-proved deterministic fresh-map player placement for the already committed Junction resident map:

```text
EspPlayerSpawnState = 24 B
persistent heap = 0 B

stateFNV=ba6af4a7
spawnParam=0
source=HEADER
tileIndex=943
tile=15/29
world=992/1888
angle=64
viewZ=36
viewZOld=4
loadType=0
```

Packed override decode is also hardware-proven:

```text
spawnParam=00030167
tile=7/11
world=480/736
angle=192
stateFNV=e0a5110b
```

Junction remains resident and unchanged:

```text
snapshotFNV=bc9071e9
payload=10410 B
actual heap=10540 B
entities=30 enemies=0 destructibles=3
```

Legacy Game/Player/Render/Hud/DoomCanvas remain untouched and ST_PLAYING is still not reached.

## Current candidate

[`MAP1_NATIVE_PLAYER_VIEW.md`](MAP1_NATIVE_PLAYER_VIEW.md) applies the hardware-proven spawn projection to the first permanent active native player/view owner.

```text
branch = agent/esp32-native-player-view
base   = 992f38374840113409e776fb82ce57ab014607e5
firmware candidate = fe1630ad5618dfd35bbbc555de8f9762d0b046f8
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

### Permanent API

```text
EspPlayerViewState = 44 B expected classic-ESP32 ABI
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

and keeps explicit target/load/follow-up state.

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

The candidate owns only the first native player/view application and represents the HUD write as:

```text
hudRefreshPending=1
```

The following remain unexecuted:

```text
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
ST_PLAYING=no
```

### Expected real Junction native player/view

```text
viewX=992
viewY=1888
viewZ=36
viewAngle=64

destX=992
destY=1888
destAngle=64

viewZOld=4
targetMapId=9
gameplayLoadMapId=2
loadType=0
spawnApplied=1
hudRefreshPending=1
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
active=1
```

Static 44-byte FNV prediction:

```text
d1131d18
```

Packed override application prediction:

```text
view/dest=480/736
angle=192
viewZ=36
viewZOld=4
stateFNV=9ed47d08
```

These are candidate values until hardware confirms them.

### Fail-closed rules

Application is once-only while active and validates:

```text
active fresh-map spawn projection
valid target/gameplay identities
tile/world geometry
fixed viewZ/viewZOld
required pending flags
header source semantics
packed override re-decoded from sourceSpawnParam
```

Refused inputs leave the native owner unchanged.

### Candidate hardware acceptance

Expected decisive lines:

```text
[JUNCTIONVIEW] READY stateBytes=44 stateFNV=d1131d18 view=992/1888/36 angle=64 dest=992/1888 angle=64 viewZOld=4 targetMap=9 gameplayLoadMapId=2 loadType=0 active=1 spawnApplied=1

[JUNCTIONVIEW] FOLLOWUPS hudRefresh=1 facingRefresh=1 playerSetup=1 tileEnter=1 hudApplied=no facingApplied=no playerSetupApplied=no tileEnterApplied=no

[JUNCTIONVIEW] OVERRIDE param=00030167 view=480/736/36 angle=192 dest=480/736 angle=192 stateFNV=9ed47d08 sourceProjectionFNV=e0a5110b

[JUNCTIONVIEW] FAILCLOSED nullSpawn=1 inactive=1 badGeometry=1 badPending=1 repeat=1 repeatAtomic=yes reset=1 stateAtomic=yes

[JUNCTIONVIEW] RESIDENT snapshotFNV=bc9071e9->bc9071e9 targetLeftResident=yes payload=10410 entities=30 enemies=0 destructibles=3 packClosed=yes

[JUNCTIONVIEW] RAM heap8=...->... delta=0 largest8=...->... delta=0 persistentHeapBytes=0
```

Final intended PARK:

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

## Hardware-proven canons inherited by candidate

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
 -> permanent native player/view application      [candidate]
 -> HUD refresh / facing / setup / tile-enter
 -> native gameplay/render loop
```

Still outside candidate:

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

The player/view owner is deliberately not part of `EspMapResidentLifecycle_resetAll()`: map-resident data and player state have different lifetimes.

`shapeData == NULL` and `mediaTexels == NULL` remain mandatory.

Build/flash the candidate with the normal `esp32-cyd` environment. No local build or hardware PASS is claimed.
