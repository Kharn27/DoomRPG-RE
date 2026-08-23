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
| [`MAP1_NATIVE_POST_LOAD_HUD_CLEAR.md`](MAP1_NATIVE_POST_LOAD_HUD_CLEAR.md) | post-load HUD message clear | #77 | `56c4211a91e6a95763dd4cc215ef40de6c10a98b` |

Older archives remain indexed by Git history. `PORTING_STATUS.md` is the preferred recovery point.

## Latest merged boundary

PR #77 hardware-proved the three HUD message-channel writes immediately after
recovered `DoomCanvas_finishRotation()`:

```text
EspHudPostLoadClearState=8 B
stateFNV=b7383e18
PlayerView=afcdcf74 unchanged
Facing=95aa1108 unchanged
resident snapshot=bc9071e9 unchanged
automap=0b2ae445 unchanged
persistent heap=0 B
ST_PLAYING=no
```

## Current real-CYD branch

[`MAP1_NATIVE_POST_LOAD_GIVEMAP.md`](MAP1_NATIVE_POST_LOAD_GIVEMAP.md) hardware-
proves the next exact Junction caller operation:

```text
branch = agent/esp32-native-post-load-givemap
base   = 56c4211a91e6a95763dd4cc215ef40de6c10a98b
hardware-tested firmware = 511156120bd877367d13ffa4b98ed6815005bc3c
status = REAL-CYD HARDWARE PASS for direct Junction GIVEMAP
```

The supplied excerpt did not include the older same-firmware MAP_INTRO
`[MAPGIVEMAPPROBE]` regression block, so that historical regression witness is
not claimed yet.

### Exact direct GIVEMAP semantics

```text
for each line without flag 0x20: set reveal flag 0x80
for every map sprite: set reveal bit 0x10000000
for every BIT_AM_ENTRANCE tile: set BIT_AM_VISITED
```

The operation maps directly onto existing compact owners:

```text
EspMapAutomapState -> line/sprite reveal bits
EspMapState        -> tile BIT_AM_VISITED
```

No legacy `Game_givemap()` call occurs.

### Shared permanent primitive

```text
EspMapGiveMapDirectResult = 12 B
EspMapAutomapState_planGiveMapDirect()
EspMapAutomapState_applyGiveMapDirect()
```

The historical event wrapper remains ABI-compatible:

```text
EspMapGiveMapResult = 20 B
EspMapAutomapState_applyGiveMapCommand()
```

### Caller-order owner

```text
EspPostLoadGiveMapState = 16 B
stateFNV = 448e587d
persistent heap = 0 B
```

Real-CYD state:

```text
lineTargets=198
spriteTargets=48
entranceTargets=15
linesMutated=198
spritesMutated=48
tilesMutated=15
targetMap=9
gameplayLoadMapId=2
loadType=0
active=1
```

### World fingerprints

```text
mapStateFNV  c5cdfc04 -> 8dba0bb4
automapFNV   0b2ae445 -> b699bd75
snapshotFNV  bc9071e9 -> bb714d80
```

Unchanged non-target owners:

```text
runtime=bc432a0f
script=bc9b18ff
line=3658710d
texture=537319ad
topology=d6e8df7d
```

Semantic proof:

```text
allTargetsRevealed=yes
idempotentPlan=yes
nonTargetOwnersUnchanged=yes
```

### Fail-closed / RAM / legacy integrity

```text
nullHud=1
nullOutput=1
inactiveHud=1
uncleared=1
targetMap=1
gameplayMap=1
loadType=1
plannerNull=1
prepareAtomic=yes
postActivePrepare=1
repeat=1
repeatAtomic=yes

heap8=72700->72700
largest8=34804->34804
persistentHeapBytes=0
packClosed=yes
```

Same-build equality witnesses:

```text
gameFNV=d073b2d5->d073b2d5
playerFNV=c64e7862->c64e7862
hudFNV=b18611d2->b18611d2
canvasFNV=702a1a9d->702a1a9d
renderFNV=f9344dec->f9344dec
frameFNV=2cb60336->2cb60336
legacyRuntimeClear=yes
legacyGame_givemapCalled=no
```

## Hardware-proven canons through current branch

```text
Entrance snapshotFNV=b3811f3d
Entrance heap=18008 B
Junction pre-GIVEMAP snapshotFNV=bc9071e9
Junction post-GIVEMAP snapshotFNV=bb714d80
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
JunctionPostLoadGiveMapFNV=448e587d
```

## Exact caller order

```text
DoomCanvas_finishRotation()                  [hardware-proven]
Hud.msgCount=0                              [hardware-proven]
Hud.statBarMessage=NULL                     [hardware-proven]
Hud.logMessage[0]='\0'                     [hardware-proven]
if MAP_JUNCTION: Game_givemap()             [hardware-proven]
else: DoomCanvas_uncoverAutomap()
Player_selectWeapon(current weapon)         [NEXT after merge]
initial Game_saveState when !isLoaded       [deferred]
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
 -> weapon reselection                           [next after merge]
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
nativeFacing=yes
nativeHudClear=yes
nativePostLoadGiveMap=yes
finishRotationComplete=yes
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

## Remaining witness before full branch merge-ready

The direct Junction GIVEMAP boundary itself is a real-CYD PASS on firmware
`511156120bd877367d13ffa4b98ed6815005bc3c`.

Because the branch refactored the shared opcode-9 mutation primitive, the
candidate contract also requested the historical MAP_INTRO `[MAPGIVEMAPPROBE]`
PASS from that same firmware. It was not included in the supplied Serial excerpt.
No code change is needed; only the actual regression log may close that final
documentation witness.

## Next bounded milestone after merge

Recover exact `main`, then port only:

```c
Player_selectWeapon(player, player->weapon);
```

Keep save, cleanup, `ST_PLAYING`, gameplay entities and rendering separate.