# ESP32 documentation map

This file defines the current ESP32 CYD documentation map.

## Source of truth

- [`README.md`](README.md): stable build/flash guide.
- [`PORTING_STATUS.md`](PORTING_STATUS.md): authoritative current recovery point.
- Milestone archives: implementation contracts and hardware evidence.

## Recent merged milestones

| Archive | Purpose | PR | Merged `main` |
| --- | --- | ---: | --- |
| [`MAP1_NATIVE_COMMITTED_TRANSITION.md`](MAP1_NATIVE_COMMITTED_TRANSITION.md) | committed native map swap | #68 | `00268a100c6662cb883f9a02d979b4f29eecbf12` |
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
| [`MAP1_NATIVE_POST_LOAD_WEAPON_SELF_SELECT.md`](MAP1_NATIVE_POST_LOAD_WEAPON_SELF_SELECT.md) | current-weapon identity self-select | #79 | `04e4e2269a6c70db3f3e4027717bdb36f286ce65` |
| [`MAP1_NATIVE_POST_LOAD_INITIAL_SAVE_INTENT.md`](MAP1_NATIVE_POST_LOAD_INITIAL_SAVE_INTENT.md) | initial-save semantic intent | #80 | `b669488c6f577d1004ac5a1dc742392698d66095` |
| [`MAP1_NATIVE_POST_LOAD_FLAG_CLEANUP.md`](MAP1_NATIVE_POST_LOAD_FLAG_CLEANUP.md) | isLoaded/isSaved/activeLoadType cleanup | #81 | `c4a093d9db77a715c355a68c5aae9faaddf22e0b` |
| [`MAP1_NATIVE_POST_LOAD_EVENT_PARTICLE_CLEANUP.md`](MAP1_NATIVE_POST_LOAD_EVENT_PARTICLE_CLEANUP.md) | empty event/particle cleanup | #82 | `c9d0a3fdc705acdbb613beccb17de4d98af218c3` |
| [`MAP1_NATIVE_POST_LOAD_VIEW_INVALIDATION.md`](MAP1_NATIVE_POST_LOAD_VIEW_INVALIDATION.md) | redraw-request caller write | #83 | `4b5a9a368fbe4ee7938b2e3d11218b312d631f47` |
| [`MAP1_NATIVE_POST_LOAD_PLAYING_TRANSITION.md`](MAP1_NATIVE_POST_LOAD_PLAYING_TRANSITION.md) | native ST_PLAYING transition semantics | #84 | `0a2cf860e074b19240f50fc65822710ab8d505bb` |

Older archives remain indexed by Git history. `PORTING_STATUS.md` is the preferred recovery point.

## Latest merged boundary

PR #84 hardware-proved the full relevant fresh-Junction semantics of:

```c
DoomCanvas_setState(doomCanvas, ST_PLAYING);
```

Canonical result:

```text
EspPostLoadPlayingTransitionState=12 B
stateFNV=73bc9acd
state=9->3
monstersTurn=0
displaySoftKeys=0
restoreSoftKeys=0->0
skipCheckState=0->1
softKeyIntent=Menu/Map
nativeST_PLAYING=yes
legacyST_PLAYING=no
persistentHeapBytes=0
```

## Current merge-ready milestone

[`MAP1_NATIVE_POST_LOAD_IDLE_TIME.md`](MAP1_NATIVE_POST_LOAD_IDLE_TIME.md) hardware-proves the final successful fresh-map caller write:

```c
doomCanvas->idleTime = doomCanvas->time + 8000;
```

Candidate/result:

```text
branch = agent/esp32-native-post-load-idle-time
base   = 0a2cf860e074b19240f50fc65822710ab8d505bb
hardware-tested firmware = 1349ed314487bcade159ce92c6ad9c27b75735d5
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

### Permanent owner

```text
ESP32/include/esp_post_load_idle_time_state.h
ESP32/src/esp_post_load_idle_time_state.c
EspPostLoadIdleTimeState = 16 B
persistent heap = 0 B
```

Stable semantic contract:

```text
idleTimeAfter = timeBefore + 8000
targetMap=9
active=1
```

Real-CYD run:

```text
timeBefore=4600
idleTimeBefore=0
idleTimeAfter=12600
delta=8000
stateFNV=d6e95f57
```

The idle-time owner FNV is a **run-specific witness**, not a cross-boot canon, because the owner contains the live uptime. Future valid boots may produce another FNV while preserving `delta=8000` and the permanent contract.

### Semantic proof

```text
nativeST_PLAYING=yes
loadTailComplete=yes
idleDeadlineOwned=yes
legacyTime=4600->4600
legacyIdleTime=0->0
legacyMutation=no
gameplayDispatch=no
rendering=no
presentation=no
```

### Strict predecessor / fail-closed proof

```text
playingBytes=12
playingFNV=73bc9acd
unchanged=yes
callerOrder=yes
nativeState=3
particleTopologyCanonical=yes
activeList=0
freeList=64
totalPool=64

nullPlaying=1
nullOutput=1
inactivePlaying=1
targetMap=1
negativeTime=1
overflowTime=1
prepareAtomic=yes
postActivePrepare=1
repeat=1
repeatAtomic=yes
```

### Resident / RAM / side-effect proof

```text
snapshotFNV=bb714d80->bb714d80
runtimeFNV=bc432a0f
mapFNV=8dba0bb4
scriptFNV=bc9b18ff
lineFNV=3658710d
textureFNV=537319ad
automapFNV=b699bd75
topologyFNV=d6e8df7d
heap8=72540->72540
largest8=34804->34804
persistentHeapBytes=0
legacyState=9->9
time=4600->4600
idleTime=0->0
GameMutation=no
PlayerMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
ParticleSystemMutation=no
packClosed=yes
```

Same-build equality witnesses only:

```text
gameFNV=3982324b->3982324b
playerFNV=c64e7862->c64e7862
hudFNV=d2deba0f->d2deba0f
canvasFNV=afd3b96c->afd3b96c
renderFNV=f9344dec->f9344dec
frameFNV=10f53ffb->10f53ffb
eventQueueFNV=d985589f->d985589f
particleFNV=f186cf0c->f186cf0c
```

Stable heartbeat:

```text
heap=138304
heap8=72540
largest8=34804
SD=ready ZIP=ready VIDEO=ready CORE=ready LAYOUT=ready
PRERENDER=ready RENDER=ready MAPPINGS=ready MENUBSP=ready
```

## Stable canons through current branch

```text
Entrance snapshotFNV=b3811f3d
Junction sourceFNV=fefaf5ca
Junction snapshotFNV=bb714d80
runtimeFNV=bc432a0f
mapFNV=8dba0bb4
scriptFNV=bc9b18ff
lineFNV=3658710d
textureFNV=537319ad
automapFNV=b699bd75
topologyFNV=d6e8df7d
JunctionDurableFacingFNV=95aa1108
postFacingPlayerViewFNV=afcdcf74
JunctionPostLoadHudClearFNV=b7383e18
JunctionPostLoadGiveMapFNV=448e587d
JunctionWeaponSelfSelectFNV=699f3cf3
JunctionInitialSaveIntentFNV=0bf1a911
JunctionPostLoadFlagCleanupFNV=46cb2547
JunctionEventParticleCleanupFNV=8bc79e2b
JunctionViewInvalidationFNV=4561c3c1
JunctionNativeSTPlayingFNV=73bc9acd
```

Idle-time `d6e95f57` is deliberately omitted from this stable list because it is uptime-dependent.

## Fresh-Junction load tail: complete

```text
DoomCanvas_finishRotation()                  [hardware-proven]
Hud message-channel clear                    [hardware-proven]
if Junction: Game_givemap()                 [hardware-proven]
Player_selectWeapon(player, player->weapon) [hardware-proven]
conditional Game_saveState(...)             [hardware-proven semantic intent]
Game.isLoaded=false                         [hardware-proven]
Game.isSaved=false                          [hardware-proven]
Game.activeLoadType=0                       [hardware-proven]
DoomCanvas.numEvents=0                      [hardware-proven semantic cleanup]
ParticleSystem_freeAllParticles(...)        [hardware-proven semantic cleanup]
DoomCanvas.numEvents=0                      [hardware-proven semantic cleanup]
DoomCanvas.isUpdateView=true                [hardware-proven semantic owner]
DoomCanvas_setState(ST_PLAYING)             [hardware-proven native semantic owner]
DoomCanvas.idleTime=DoomCanvas.time+8000    [hardware-proven native semantic owner]
return true                                 [tail complete]
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
 -> post-load caller chain                       [hardware-proven complete]
 -> native PLAYING loop/input dispatch           [NEXT]
 -> native gameplay
 -> native renderer/presentation
```

Current hardware PARK:

```text
legacyState=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativeST_PLAYING=yes
nativeIdleTime=yes
postLoadTailComplete=yes
ST_PLAYINGPending=no
idleTimePending=no
gameplayDispatchPending=yes
rendererPending=yes
initialSavePersistencePending=yes
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
```

## Merge recommendation

```text
MERGE agent/esp32-native-post-load-idle-time
```

Hardware-tested firmware:

```text
1349ed314487bcade159ce92c6ad9c27b75735d5
```

All commits after that tested SHA must remain documentation-only.

## Next bounded milestone after merge

Recover exact new `main`, then introduce the smallest permanent native PLAYING-loop/dispatch boundary that consumes the native ST_PLAYING and idle-time owners without changing legacy `DoomCanvas.state` to 3.

The first milestone should remain no-input/no-gameplay and renderer-free: prove one native PLAYING service iteration, ownership/gating, zero heap allocation, resident stability and dormant legacy runtime. Do not bundle a full renderer or entity gameplay into that first loop milestone.
