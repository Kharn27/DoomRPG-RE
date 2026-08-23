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
| [`MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md`](MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md) | SHOW/HIDE compact topology; all MAP_INTRO opcode families bounded | #62 | `ed5cd9a09c9ae36f999661f4284f64400681b1af` |
| [`MAP1_NATIVE_LEVEL_EXIT_STATS.md`](MAP1_NATIVE_LEVEL_EXIT_STATS.md) | map-derived level-exit stats | #63 | `533784b5483e14a12558fb08c9331d8b744caa88` |
| [`MAP1_NATIVE_PLAYER_EXIT_STATE.md`](MAP1_NATIVE_PLAYER_EXIT_STATE.md) | pointer-free player exit writes | #64 | `3759bcd12a3f6d36a6a696457110ab27474c24b8` |
| [`MAP1_NATIVE_STATS_MENU_INTENT.md`](MAP1_NATIVE_STATS_MENU_INTENT.md) | LEVEL/OVERALL stats-menu intent | #65 | `c8679133351fa00e01a67103386b7676660c4a6e` |
| [`MAP1_NATIVE_TRANSITION_PREFLIGHT.md`](MAP1_NATIVE_TRANSITION_PREFLIGHT.md) | map catalog + Junction PAK/BSP preflight | #66 | `9f981f490282200f216aef66d22608d2244beb00` |
| [`MAP1_NATIVE_RESIDENT_HANDOFF.md`](MAP1_NATIVE_RESIDENT_HANDOFF.md) | reversible full resident Entrance/Junction handoff | #67 | `fddae899fd7dc01b20cf6bd532489326380954e3` |
| [`MAP1_NATIVE_COMMITTED_TRANSITION.md`](MAP1_NATIVE_COMMITTED_TRANSITION.md) | transactional committed Entrance -> Junction resident swap | #68 | `00268a100c6662cb883f9a02d979b4f29eecbf12` |
| [`MAP1_NATIVE_JUNCTION_SPAWN.md`](MAP1_NATIVE_JUNCTION_SPAWN.md) | fresh-map Junction spawn/load projection | #69 | `992f38374840113409e776fb82ce57ab014607e5` |
| [`MAP1_NATIVE_PLAYER_VIEW.md`](MAP1_NATIVE_PLAYER_VIEW.md) | permanent active Junction player/view owner | #70 | `8a82891bb8d9c62582170cc4b3b74d270849e77b` |
| [`MAP1_NATIVE_HUD_REFRESH.md`](MAP1_NATIVE_HUD_REFRESH.md) | post-spawn HUD dirty ownership | #71 | `02b7f143a12e6df86ada094af10ef580ad572aad` |
| [`MAP1_NATIVE_PLAYER_SETUP.md`](MAP1_NATIVE_PLAYER_SETUP.md) | fresh-map Player_setup semantic owner | #72 | `9077ae4496bdcc06b6b99846332ab43b38943a8a` |
| [`MAP1_NATIVE_INITIAL_TILE_ENTER.md`](MAP1_NATIVE_INITIAL_TILE_ENTER.md) | first fresh-map Game_executeTile dispatch | #73 | `0bc171affad8416ed1a7918a4a67fd4d53d61efe` |
| [`MAP1_NATIVE_FINISH_ROTATION_ORIENTATION.md`](MAP1_NATIVE_FINISH_ROTATION_ORIENTATION.md) | finishRotation orientation preparation | #74 | `2decae5067438dc1a2d9c29335cfc0cad5538645` |

Older milestone archives remain indexed by Git history. `PORTING_STATUS.md` is the preferred recovery point.

## Latest merged boundary

PR #74 hardware-proved the orientation preparation at the start of `DoomCanvas_finishRotation()`:

```text
EspPlayerOrientationState=24 B
stateFNV=acc754a6
destAngle=64
viewSin=65536
viewCos=0
viewStepX=0
viewStepY=-64
native-vs-legacy exact=yes
persistent heap=0 B
```

## Current merge-ready milestone

[`MAP1_NATIVE_FINISH_ROTATION_SECOND_TILE.md`](MAP1_NATIVE_FINISH_ROTATION_SECOND_TILE.md) hardware-proves the second `Game_executeTile()` inside `DoomCanvas_finishRotation()`.

```text
branch = agent/esp32-native-finish-rotation-second-tile
base   = 2decae5067438dc1a2d9c29335cfc0cad5538645
hardware-tested firmware = df4f62687d99eb3b3e9569ae6861b6909d59c82d
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

### Exact recovered call

```text
Game_executeTile(destX, destY, DoomCanvas_flagForFacingDir() | 0x400)
world=992/1888
tile=943
destAngle=64
input flags=0x10000400
```

### Permanent API and hardware canon

```text
ESP32/include/esp_player_finish_rotation_tile.h
ESP32/src/esp_player_finish_rotation_tile.c

EspPlayerFinishRotationTileState=24 B
stateFNV=09e58e0d
persistent heap=0 B
```

Real event result:

```text
eventFound=1
eventIndex=61
eventState=0
eventFlags=0
blocked=0
eligible=0
executed=0
removed=0
skipAdvanceTurn=0
playerKeys=00000000
```

The second call therefore finds the same real event 61 as the first fresh-map tile call but, under exact flags `0x10000400`, no command is eligible. No opcode executes and the script overlay remains unchanged:

```text
script FNV bc9b18ff -> bc9b18ff
```

The generic opcode executor remains intentionally restricted to IDs 11/19/20.

### Input ownership remains unchanged

```text
PlayerView FNV=1bd0f09b -> 1bd0f09b
InitialTile FNV=f73e28b2 unchanged
Orientation FNV=acc754a6 unchanged
facingRefreshPending=1
tileEnterPending=0
```

### Hardware fail-closed proof

```text
nullView=1
nullInitial=1
nullOrientation=1
nullOutput=1
inactive=1
tilePending=1
missingFacing=1
angle=1
blocked=1
initialMismatch=1
orientationInactive=1
orientationMismatch=1
prepareAtomic=yes
repeat=1
repeatAtomic=yes
```

### Hardware RAM / integrity

```text
heap8=72796->72796
largest8=34804->34804
persistentHeapBytes=0

heartbeat heap=138560
heartbeat heap8=72796
heartbeat largest8=34804
```

Resident integrity:

```text
runtime=bc432a0f
map=c5cdfc04
script=bc9b18ff
line=3658710d
texture=537319ad
automap=0b2ae445
topology=d6e8df7d
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes
nonScriptStable=yes
```

Same-build equality witnesses:

```text
gameFNV=c655ff85->c655ff85
playerFNV=774ed642->774ed642
canvasFNV=1b7ba23f->1b7ba23f
renderFNV=f9344dec->f9344dec
frameFNV=e6066fb0->e6066fb0
legacyRuntimeClear=yes
GameMutation=no
PlayerMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
```

## Hardware-proven canons through current branch

```text
Entrance snapshotFNV=b3811f3d
Entrance heap=18008 B
Junction snapshotFNV=bc9071e9
Junction heap=10540 B
Junction sourceFNV=fefaf5ca
catalogFNV=ce322e3f
committed COMMITTED FNV=2c595a62
JunctionSpawnFNV=ba6af4a7
JunctionPlayerViewFNV=d1131d18
postHudPlayerViewFNV=d17fa0d1
JunctionHudRefreshFNV=6965ee06
PlayerSetupSemanticFNV=3b27c6a1
postSetupPlayerViewFNV=c21fba3c
JunctionInitialTileFNV=f73e28b2
postInitialTilePlayerViewFNV=1bd0f09b
JunctionOrientationFNV=acc754a6
JunctionSecondTileFNV=09e58e0d
```

## Architecture direction

```text
original Doom RPG behavior/data
 -> /DoomRPG-ESP32.pak backing store
 -> compact immutable native map                 [hardware-proven]
 -> explicit compact mutable overlays            [hardware-proven]
 -> native event semantic owners                 [hardware-proven by family]
 -> transition/resident lifecycle                [hardware-proven]
 -> native player spawn projection               [hardware-proven]
 -> permanent native player/view application     [hardware-proven]
 -> post-spawn HUD dirty ownership               [hardware-proven]
 -> Player_setup-equivalent session root         [hardware-proven]
 -> initial tile-enter dispatch                  [hardware-proven]
 -> finishRotation orientation preparation       [hardware-proven]
 -> finishRotation second tile dispatch          [hardware-proven]
 -> durable facing query                         [next]
 -> ST_PLAYING / native gameplay/render loop
```

Current hardware PARK:

```text
state=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativePlayerView=yes
nativeInitialTile=yes
nativeOrientation=yes
nativeSecondTile=yes
secondTilePending=no
finalFacingPending=yes
finishRotationComplete=no
ST_PLAYING=no
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

Still outside:

```text
actual stats-menu rendering/input
actual HUD rendering / renderer dirty consumption
weapon restore/select ownership when disabledWeapons!=0
final native facing-entity query
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

Mandatory invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
runtime ZIP map access forbidden
legacy Game.entities = 0
legacy Game.monsters = 0
ST_PLAYING not reached
```

## Next bounded milestone after merge

After merge, recover from the exact new `main` SHA. The next exact legacy operation is:

```text
DoomCanvas_checkFacingEntity()   # durable final facing result
```

Implement this against the compact native resident topology and hardware-proven player position/orientation. Do not revive legacy entity/render runtime. Keep `ST_PLAYING` progression separate until durable facing ownership is hardware-proven.

## Merge recommendation

```text
MERGE agent/esp32-native-finish-rotation-second-tile
```

Hardware-tested firmware:

```text
df4f62687d99eb3b3e9569ae6861b6909d59c82d
```

All later commits on this branch are documentation-only unless another firmware is flashed.
