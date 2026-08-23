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

Older archives remain indexed by Git history. `PORTING_STATUS.md` is the preferred recovery point.

## Latest merged boundary

PR #83 hardware-proved:

```c
doomCanvas->isUpdateView = true;
```

Canonical result:

```text
EspPostLoadViewInvalidationState=4 B
stateFNV=4561c3c1
isUpdateView=1->1
targetMap=9
persistentHeapBytes=0
legacy ST_PLAYING=no
```

## Current merge-ready milestone

[`MAP1_NATIVE_POST_LOAD_PLAYING_TRANSITION.md`](MAP1_NATIVE_POST_LOAD_PLAYING_TRANSITION.md) hardware-proves the full relevant fresh-Junction semantics of:

```c
DoomCanvas_setState(doomCanvas, ST_PLAYING);
```

Candidate/result:

```text
branch = agent/esp32-native-post-load-playing-transition
base   = 4b5a9a368fbe4ee7938b2e3d11218b312d631f47
hardware-tested firmware = afda93f0a28af5c34620fef2ac3354a24b3f91f5
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

### Permanent owner

```text
ESP32/include/esp_post_load_playing_transition_state.h
ESP32/src/esp_post_load_playing_transition_state.c
EspPostLoadPlayingTransitionState = 12 B
stateFNV = 73bc9acd
persistent heap = 0 B
```

Real-CYD state:

```text
state=9->3
monstersTurn=0
displaySoftKeys=0
restoreSoftKeys=0->0
skipCheckState=0->1
softKeyIntent=1
softKeyPresentationDeferred=0
targetMap=9
active=1
```

The permanent owner now establishes native `ST_PLAYING`. Legacy `DoomCanvas.state` deliberately remains `ST_INTRO` because the legacy loop would otherwise enter `DoomCanvas_playingState()` and the removed pointer-heavy renderer/runtime.

### Soft-key semantics

Legacy `DoomCanvas_setState(ST_PLAYING)` requests `DoomCanvas_drawSoftKeys("Menu", "Map")` when `!game->monstersTurn`.

Real CYD:

```text
monstersTurn=0
softKeyCallRequested=yes
softKeyIntent=Menu/Map
displaySoftKeys=0
softKeyVisible=no
restoreSoftKeys=0->0
```

`DoomCanvas_drawSoftKeys()` only enters its drawing/state-mutation body when `displaySoftKeys` is enabled. Therefore the Menu/Map intent is preserved permanently without a presentation debt on this classic CYD path.

### Semantic proof

```text
nativeST_PLAYING=yes
legacyST_PLAYING=no
stateTransition=yes
softKeyCallRequested=yes
softKeyVisible=no
softKeyLabels=Menu/Map
restoreSoftKeysResult=0
skipCheckStateResult=1
legacyDoomCanvas_setStateCalled=no
legacyDoomCanvas_drawSoftKeysCalled=no
rendering=no
presentation=no
```

### Strict predecessor / fail-closed proof

```text
viewInvalidationBytes=4
viewInvalidationFNV=4561c3c1
unchanged=yes
callerOrder=yes
isUpdateView=1->1
particleTopologyCanonical=yes
activeList=0
freeList=64
totalPool=64

nullView=1
nullOutput=1
inactiveView=1
targetMap=1
invalidMonstersTurn=1
invalidDisplaySoftKeys=1
invalidRestoreSoftKeys=1
invalidSkipCheckState=1
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
heap8=72552->72552
largest8=34804->34804
persistentHeapBytes=0
legacyState=9->9
GameMutation=no
PlayerMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
ParticleSystemMutation=no
legacyDoomCanvas_setStateCalled=no
legacyDoomCanvas_drawSoftKeysCalled=no
packClosed=yes
```

Same-build equality witnesses only:

```text
gameFNV=002b366b->002b366b
playerFNV=c64e7862->c64e7862
hudFNV=d2deba0f->d2deba0f
canvasFNV=4331fadc->4331fadc
renderFNV=f9344dec->f9344dec
frameFNV=b8924a47->b8924a47
eventQueueFNV=d985589f->d985589f
particleFNV=f186cf0c->f186cf0c
```

Stable heartbeat:

```text
heap=138316
heap8=72552
largest8=34804
SD=ready ZIP=ready VIDEO=ready CORE=ready LAYOUT=ready
PRERENDER=ready RENDER=ready MAPPINGS=ready MENUBSP=ready
```

## Hardware-proven canons through current branch

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

## Exact caller order

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
idleTime=time+8000                          [NEXT after merge]
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
 -> post-load caller chain                       [hardware-proven through ST_PLAYING]
 -> idleTime caller write                        [next]
 -> native PLAYING loop/input dispatch
 -> native gameplay
 -> native renderer/presentation
```

Current hardware PARK:

```text
legacyState=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativeViewInvalidation=yes
nativeST_PLAYING=yes
legacyST_PLAYING=no
initialSavePersistencePending=yes
ST_PLAYINGPending=no
idleTimePending=yes
rendererPending=yes
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
MERGE agent/esp32-native-post-load-playing-transition
```

Hardware-tested firmware:

```text
afda93f0a28af5c34620fef2ac3354a24b3f91f5
```

All commits after that tested SHA must remain documentation-only.

## Next bounded milestone after merge

Recover exact new `main`, then own only:

```c
doomCanvas->idleTime = doomCanvas->time + 8000;
```

Keep native gameplay dispatch and renderer activation separate. Once this final post-load caller write is hardware-proven, the fresh-Junction load tail is semantically complete; the following architectural milestone should consume native ST_PLAYING in a native loop/render path rather than changing legacy `DoomCanvas.state` to 3.
