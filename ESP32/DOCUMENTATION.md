# ESP32 documentation map

This file defines the current ESP32 CYD documentation map.

## Source of truth

- [`README.md`](README.md): stable build/flash guide.
- [`PORTING_STATUS.md`](PORTING_STATUS.md): authoritative current recovery point.
- Milestone archives: implementation contracts and hardware evidence.

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
| [`MAP1_NATIVE_POST_LOAD_HUD_CLEAR.md`](MAP1_NATIVE_POST_LOAD_HUD_CLEAR.md) | post-load HUD message clear | #77 | `56c4211a91e6a95763dd4cc215ef40de6c10a98b` |
| [`MAP1_NATIVE_POST_LOAD_GIVEMAP.md`](MAP1_NATIVE_POST_LOAD_GIVEMAP.md) | direct Junction Game_givemap | #78 | `4737b016d02615b8435cf84909fe3c251b6d338b` |

Older archives remain indexed by Git history. `PORTING_STATUS.md` is the preferred recovery point.

## Latest merged boundary

PR #78 hardware-proved direct caller-side Junction `Game_givemap()` using the
existing compact automap/map-state owners.

```text
EspPostLoadGiveMapState=16 B
stateFNV=448e587d
line/sprite/entrance targets=198/48/15
mutations=198/48/15
mapState c5cdfc04 -> 8dba0bb4
automap  0b2ae445 -> b699bd75
snapshot bc9071e9 -> bb714d80
persistent heap=0 B
ST_PLAYING=no
```

## Current hardware candidate

[`MAP1_NATIVE_POST_LOAD_WEAPON_SELF_SELECT.md`](MAP1_NATIVE_POST_LOAD_WEAPON_SELF_SELECT.md)
owns only the next exact caller operation:

```text
branch = agent/esp32-native-post-load-weapon-self-select
base   = 4737b016d02615b8435cf84909fe3c251b6d338b
status = HARDWARE CANDIDATE — NOT YET CYD-PROVEN
```

Exact callsite:

```c
Player_selectWeapon(player, player->weapon);
```

Recovered implementation:

```c
if (player->weapon != i) {
    DoomCanvas_updateViewTrue(player->doomRpg->doomCanvas);
}
player->weapon = i;
```

Because `i` is the current weapon itself, this caller is strictly:

```text
requested == current
updateView branch not taken
weapon assignment is identity
```

No real weapon-change gameplay, ammo or inventory ownership is introduced.

### Permanent owner

```text
ESP32/include/esp_post_load_weapon_select_state.h
ESP32/src/esp_post_load_weapon_select_state.c
EspPostLoadWeaponSelectState = 8 B candidate
persistent heap = 0 B
```

The state contains:

```text
weaponBefore
requestedWeapon
weaponAfter
viewInvalidationRequested
targetMapId
gameplayLoadMapId
loadType
active
```

The real CYD will establish the current weapon and state FNV. Reset-time defaults
are not treated as hardware truth.

### Strict input boundary

The candidate requires:

```text
post-load GIVEMAP FNV=448e587d / counts 198/48/15
targetMap=9
gameplayLoadMapId=2
loadType=0
runtimeFNV=bc432a0f
mapStateFNV=8dba0bb4
automapFNV=b699bd75
current weapon in legacy range 0..11
```

Legacy source confirms weapon iteration uses indices through 11 inclusive.

### Hardware probe contract

```text
ESP32/include/native_junction_post_load_weapon_select_probe.h
ESP32/src/native_junction_post_load_weapon_select_probe.c
```

Expected marker:

```text
=== Doom RPG ESP32-native Junction post-load weapon self-select ===
[JUNCTIONWEAPON] READY ...
[JUNCTIONWEAPON] SEMANTIC ...
[JUNCTIONWEAPON] INPUT ...
[JUNCTIONWEAPON] FAILCLOSED ...
[JUNCTIONWEAPON] RESIDENT ...
[JUNCTIONWEAPON] RAM ...
[JUNCTIONWEAPON] LEGACY ...
[JUNCTIONWEAPON] PARK ...
```

Acceptance requires:

```text
stateBytes=8
weaponBefore=requestedWeapon=weaponAfter
viewInvalidationRequested=0
selfSelect=yes
identityAssignment=yes
updateViewBranchTaken=no
legacy weapon unchanged
legacy isUpdateView unchanged
post-load GIVEMAP/HUD-clear/PlayerView/Facing unchanged
snapshotFNV=bb714d80 unchanged
map=8dba0bb4 unchanged
automap=b699bd75 unchanged
runtime/script/line/texture/topology unchanged
PAK closed
heap/largest delta=0
legacy Game/Player/Hud/DoomCanvas/Render/frame unchanged
legacy Player_selectWeapon not called
ST_PLAYING=no
entities=0
monsters=0
```

## Hardware-proven canons through PR #78

```text
Entrance snapshotFNV=b3811f3d
Entrance heap=18008 B
Junction sourceFNV=fefaf5ca
Junction pre-GIVEMAP snapshotFNV=bc9071e9
Junction post-GIVEMAP snapshotFNV=bb714d80
Junction heap=10540 B
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
JunctionPostLoadGiveMapFNV=448e587d
```

Current Junction resident owners before the candidate:

```text
runtime=bc432a0f
map=8dba0bb4
script=bc9b18ff
line=3658710d
texture=537319ad
automap=b699bd75
topology=d6e8df7d
snapshot=bb714d80
```

## Exact caller order

```text
DoomCanvas_finishRotation()                  [hardware-proven]
Hud.msgCount=0                              [hardware-proven]
Hud.statBarMessage=NULL                     [hardware-proven]
Hud.logMessage[0]='\0'                     [hardware-proven]
if MAP_JUNCTION: Game_givemap()             [hardware-proven]
else: DoomCanvas_uncoverAutomap()
Player_selectWeapon(player, player->weapon) [CURRENT CANDIDATE]
if !game->isLoaded: Game_saveState(1,1,1)   [next after PASS/merge]
clear isLoaded/isSaved/activeLoadType       [deferred]
clear queued events / particles             [deferred]
isUpdateView=true                           [deferred]
DoomCanvas_setState(ST_PLAYING)             [deferred]
idleTime=time+8000                          [deferred]
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
 -> direct Junction Game_givemap                 [hardware-proven]
 -> current-weapon self-selection                [CURRENT CANDIDATE]
 -> initial post-load save                       [next after PASS/merge]
 -> remaining caller-side load completion
 -> ST_PLAYING
 -> native gameplay
 -> native renderer
```

Current hardware PARK before candidate:

```text
state=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativeHudClear=yes
nativePostLoadGiveMap=yes
Game_givemapPending=no
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

## Next test

Build and flash normal `esp32-cyd`, then return the complete `[JUNCTIONWEAPON]`
block. Promote only after the real CYD proves the self-selection identity and all
integrity/RAM invariants.
