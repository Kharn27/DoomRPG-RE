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
| [`MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md`](MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md) | SHOW/HIDE compact topology | #62 | `ed5cd9a09c9ae36f999661f4284f64400681b1af` |
| [`MAP1_NATIVE_LEVEL_EXIT_STATS.md`](MAP1_NATIVE_LEVEL_EXIT_STATS.md) | level-exit stats | #63 | `533784b5483e14a12558fb08c9331d8b744caa88` |
| [`MAP1_NATIVE_PLAYER_EXIT_STATE.md`](MAP1_NATIVE_PLAYER_EXIT_STATE.md) | player exit-state | #64 | `3759bcd12a3f6d36a6a696457110ab27474c24b8` |
| [`MAP1_NATIVE_STATS_MENU_INTENT.md`](MAP1_NATIVE_STATS_MENU_INTENT.md) | stats-menu intent | #65 | `c8679133351fa00e01a67103386b7676660c4a6e` |
| [`MAP1_NATIVE_TRANSITION_PREFLIGHT.md`](MAP1_NATIVE_TRANSITION_PREFLIGHT.md) | Junction preflight | #66 | `9f981f490282200f216aef66d22608d2244beb00` |
| [`MAP1_NATIVE_RESIDENT_HANDOFF.md`](MAP1_NATIVE_RESIDENT_HANDOFF.md) | resident handoff | #67 | `fddae899fd7dc01b20cf6bd532489326380954e3` |
| [`MAP1_NATIVE_COMMITTED_TRANSITION.md`](MAP1_NATIVE_COMMITTED_TRANSITION.md) | committed map swap | #68 | `00268a100c6662cb883f9a02d979b4f29eecbf12` |
| [`MAP1_NATIVE_JUNCTION_SPAWN.md`](MAP1_NATIVE_JUNCTION_SPAWN.md) | fresh Junction spawn | #69 | `992f38374840113409e776fb82ce57ab014607e5` |
| [`MAP1_NATIVE_PLAYER_VIEW.md`](MAP1_NATIVE_PLAYER_VIEW.md) | active player/view owner | #70 | `8a82891bb8d9c62582170cc4b3b74d270849e77b` |
| [`MAP1_NATIVE_HUD_REFRESH.md`](MAP1_NATIVE_HUD_REFRESH.md) | HUD dirty ownership | #71 | `02b7f143a12e6df86ada094af10ef580ad572aad` |
| [`MAP1_NATIVE_PLAYER_SETUP.md`](MAP1_NATIVE_PLAYER_SETUP.md) | fresh-map Player_setup | #72 | `9077ae4496bdcc06b6b99846332ab43b38943a8a` |
| [`MAP1_NATIVE_INITIAL_TILE_ENTER.md`](MAP1_NATIVE_INITIAL_TILE_ENTER.md) | first fresh-map tile dispatch | #73 | `0bc171affad8416ed1a7918a4a67fd4d53d61efe` |
| [`MAP1_NATIVE_FINISH_ROTATION_ORIENTATION.md`](MAP1_NATIVE_FINISH_ROTATION_ORIENTATION.md) | finishRotation orientation | #74 | `2decae5067438dc1a2d9c29335cfc0cad5538645` |
| [`MAP1_NATIVE_FINISH_ROTATION_SECOND_TILE.md`](MAP1_NATIVE_FINISH_ROTATION_SECOND_TILE.md) | finishRotation second tile | #75 | `7a0e57cf13d02320be3a238dc73499a023c9f04c` |
| [`MAP1_NATIVE_DURABLE_FACING.md`](MAP1_NATIVE_DURABLE_FACING.md) | finishRotation durable facing | #76 | `3ab143110a1f44ebb44bc130d12d1844f3ae73ca` |

Older archives remain indexed by Git history. `PORTING_STATUS.md` is the preferred recovery point.

## Latest merged boundary

PR #76 hardware-proved the final durable `DoomCanvas_checkFacingEntity()` and
therefore completed recovered `DoomCanvas_finishRotation()` natively.

```text
EspPlayerFacingState=32 B
stateFNV=95aa1108
PlayerView FNV=afcdcf74
finishRotationComplete=yes
persistent heap=0 B
```

## Current merge-ready milestone

[`MAP1_NATIVE_POST_LOAD_HUD_CLEAR.md`](MAP1_NATIVE_POST_LOAD_HUD_CLEAR.md)
hardware-proves the first caller-side operation after finishRotation.

```text
branch = agent/esp32-native-post-load-hud-clear
base   = 3ab143110a1f44ebb44bc130d12d1844f3ae73ca
hardware-tested firmware = 469abe119fbc401d812c21f96d94fd8aaae06ff3
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

### Permanent owner

```text
ESP32/include/esp_hud_post_load_clear_state.h
ESP32/src/esp_hud_post_load_clear_state.c
EspHudPostLoadClearState = 8 B
stateFNV = b7383e18
persistent heap = 0 B
```

Hardware-proven state:

```text
messageCount=0
statBarMessagePresent=0
logMessageLength=0
cleared=1
active=1
targetMap=9
gameplayLoadMapId=2
loadType=0
```

The owner represents these exact legacy caller writes without touching `Hud_t`:

```text
Hud.msgCount=0
Hud.statBarMessage=NULL
Hud.logMessage[0]='\0'
```

PlayerView and durable facing remain unchanged:

```text
PlayerView FNV=afcdcf74
Facing FNV=95aa1108
finishRotationComplete=yes
```

### Hardware fail-closed proof

```text
nullView=1
nullFacing=1
nullOutput=1
inactiveView=1
inactiveFacing=1
facingMismatch=1
loadType=1
order=1
prepareAtomic=yes
repeat=1
repeatAtomic=yes
```

### Hardware RAM / resident / legacy integrity

```text
snapshotFNV=bc9071e9->bc9071e9
automapFNV=0b2ae445->0b2ae445
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes
Game_givemapDeferred=yes

heap8=72732->72732
largest8=34804->34804
persistentHeapBytes=0
```

Same-build equality witnesses:

```text
gameFNV=d073b2d5->d073b2d5
playerFNV=c64e7862->c64e7862
hudFNV=b18611d2->b18611d2
canvasFNV=70a8ad15->70a8ad15
renderFNV=f9344dec->f9344dec
frameFNV=9eb7ce0f->9eb7ce0f
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
JunctionDurableFacingFNV=95aa1108
postFacingPlayerViewFNV=afcdcf74
JunctionPostLoadHudClearFNV=b7383e18
```

Resident owners:

```text
runtime=bc432a0f
map=c5cdfc04
script=bc9b18ff
line=3658710d
texture=537319ad
automap=0b2ae445
topology=d6e8df7d
```

## Exact caller order after finishRotation

```text
DoomCanvas_finishRotation()                  [hardware-proven]
Hud.msgCount=0                              [hardware-proven]
Hud.statBarMessage=NULL                     [hardware-proven]
Hud.logMessage[0]='\0'                     [hardware-proven]
if MAP_JUNCTION: Game_givemap()             [NEXT]
else: DoomCanvas_uncoverAutomap()
Player_selectWeapon(current weapon)         [deferred]
initial Game_saveState when !isLoaded       [deferred]
clear isLoaded/isSaved/activeLoadType       [deferred]
clear queued events / particles             [deferred]
isUpdateView=true                           [deferred]
DoomCanvas_setState(ST_PLAYING)             [deferred]
idleTime=time+8000                          [deferred]
```

Recovered direct `Game_givemap()` semantics:

```text
all non-hidden lines: flags |= 0x80
all map sprites: info |= 0x10000000
all BIT_AM_ENTRANCE tiles: add BIT_AM_VISITED
```

## Architecture direction

```text
original behavior/data
 -> /DoomRPG-ESP32.pak
 -> compact immutable map                        [hardware-proven]
 -> compact mutable world overlays               [hardware-proven]
 -> native event semantics                       [hardware-proven by family]
 -> native transition/residency                  [hardware-proven]
 -> native fresh-map player chain                [hardware-proven]
 -> finishRotation durable facing                [hardware-proven]
 -> post-load HUD message reset                  [hardware-proven]
 -> direct Junction Game_givemap                 [next]
 -> remaining caller-side load completion
 -> ST_PLAYING
 -> native gameplay
 -> native renderer
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
nativeFacing=yes
nativeHudClear=yes
facingPending=no
finishRotationComplete=yes
Game_givemapPending=yes
weaponReselectPending=yes
initialSavePending=yes
postLoadCleanupPending=yes
ST_PLAYING=no
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
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

After merge, recover from the exact new `main` SHA and port only the direct
caller-side Junction `Game_givemap()` onto the native automap/map-state owners.
Do not bundle weapon selection, save, cleanup, `ST_PLAYING`, gameplay entities or
rendering into it.

## Merge recommendation

```text
MERGE agent/esp32-native-post-load-hud-clear
```

Hardware-tested firmware:

```text
469abe119fbc401d812c21f96d94fd8aaae06ff3
```

All later commits on this branch must be documentation-only unless another
firmware is flashed.
