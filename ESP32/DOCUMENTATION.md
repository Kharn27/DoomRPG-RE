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
| [`MAP1_NATIVE_HUD_REFRESH.md`](MAP1_NATIVE_HUD_REFRESH.md) | post-spawn HUD dirty ownership and corrected facing order | #71 | `02b7f143a12e6df86ada094af10ef580ad572aad` |
| [`MAP1_NATIVE_PLAYER_SETUP.md`](MAP1_NATIVE_PLAYER_SETUP.md) | fresh-map Player_setup semantic owner | #72 | `9077ae4496bdcc06b6b99846332ab43b38943a8a` |

Older milestone archives remain in this directory and are indexed by Git history. `PORTING_STATUS.md` is the preferred recovery point.

## Latest merged boundary

PR #72 hardware-proved native ownership of the supported fresh-map `Player_setup()` semantics:

```text
EspPlayerFreshMapState=24 B
setup semanticFNV=3b27c6a1
post-setup PlayerView FNV=c21fba3c
persistent heap=0 B

hudRefreshPending=0
facingRefreshPending=1
playerSetupPending=0
tileEnterPending=1
```

Junction remains resident at:

```text
snapshotFNV=bc9071e9
payload=10410 B
actual heap=10540 B
entities=30
enemies=0
destructibles=3
```

## Current merge-ready milestone

[`MAP1_NATIVE_INITIAL_TILE_ENTER.md`](MAP1_NATIVE_INITIAL_TILE_ENTER.md) hardware-proves the first fresh-map tile dispatch.

```text
branch = agent/esp32-native-initial-tile-enter
base   = 9077ae4496bdcc06b6b99846332ab43b38943a8a
hardware-tested firmware = d8fb3e0e372b89d95c37cce558420f7fcb474419
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

### Exact recovered call

```text
Game_executeTile(viewX, viewY, 0x40f | DoomCanvas_flagForFacingDir())
world=992/1888
tile=943
angle=64
facing flag=0x10000000
input flags=0x1000040f
```

### Permanent API and hardware canon

```text
EspPlayerInitialTileState = 24 B
persistent heap = 0 B
stateFNV=f73e28b2

EspPlayerInitialTile_reset
EspPlayerInitialTile_isReady
EspPlayerInitialTile_view
EspPlayerInitialTile_prepare
EspPlayerInitialTile_route
EspPlayerView_consumeTileEnter
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

Tile 943 therefore has a real event but no eligible command under the exact first fresh-map flags. No opcode executes and the script overlay remains unchanged:

```text
script FNV bc9b18ff -> bc9b18ff
```

The generic opcode executor remains intentionally restricted to IDs 11/19/20. This milestone does not broaden that executor.

### Player/view ownership transfer

```text
beforeFNV=c21fba3c
afterFNV=1bd0f09b
hudRefreshPending=0
facingRefreshPending=1
playerSetupPending=0
tileEnterPending=0
placementExact=yes
```

`1bd0f09b` is now hardware-proven and canonical.

### Hardware fail-closed proof

```text
nullView=1
nullSetup=1
nullOutput=1
angle=1
blocked=1
missingTile=1
missingFacing=1
setupMismatch=1
prepareAtomic=yes
repeat=1
repeatAtomic=yes
```

### Hardware RAM / integrity

```text
heap8=72868->72868
largest8=34804->34804
persistentHeapBytes=0

heap=138632
heap8=72868
largest8=34804
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
runtimeStable=yes
nonScriptMutableStable=yes
packClosed=yes
```

Same-build equality witnesses:

```text
gameFNV=c655ff85->c655ff85
playerFNV=774ed642->774ed642
frameFNV=7a95b5b5->7a95b5b5
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
 -> finishRotation orientation preparation       [next]
 -> second tile dispatch
 -> durable facing query
 -> ST_PLAYING / native gameplay/render loop
```

Current hardware PARK:

```text
state=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativePlayerView=yes
nativePlayerSetup=yes
nativeInitialTile=yes
tileEnterPending=no
facingPending=yes
finishRotationPending=yes
secondTilePending=yes
finalFacingPending=yes
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
finishRotation-equivalent orientation preparation
second tile event
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

After the branch is merged, recover from the exact new `main` SHA. The natural next boundary is the orientation preparation inside `DoomCanvas_finishRotation()` (`viewSin`, `viewCos`, `viewStepX`, `viewStepY`). Keep the second tile dispatch and final durable facing separate unless the fresh legacy/repo audit proves a tighter safe boundary.

## Merge recommendation

```text
MERGE agent/esp32-native-initial-tile-enter
```

Hardware-tested firmware:

```text
d8fb3e0e372b89d95c37cce558420f7fcb474419
```

All later commits on this branch are documentation-only unless another firmware is flashed.
