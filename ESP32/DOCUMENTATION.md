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

Older archives remain indexed by Git history. `PORTING_STATUS.md` is the preferred recovery point.

## Latest merged boundary

PR #82 hardware-proved the exact caller block:

```c
doomCanvas->numEvents = 0;
ParticleSystem_freeAllParticles(doomCanvas->particleSystem);
doomCanvas->numEvents = 0;
```

Canonical result:

```text
EspPostLoadEventParticleCleanupState=8 B
stateFNV=8bc79e2b
numEvents=0->0->0
particleCount=0->0
particleTopologyCanonical=yes
activeList=0
freeList=64
totalPool=64
persistentHeapBytes=0
ST_PLAYING=no
```

## Current merge-ready milestone

[`MAP1_NATIVE_POST_LOAD_VIEW_INVALIDATION.md`](MAP1_NATIVE_POST_LOAD_VIEW_INVALIDATION.md) hardware-proves the next exact caller write:

```text
branch = agent/esp32-native-post-load-view-invalidation
base   = c9d0a3fdc705acdbb613beccb17de4d98af218c3
hardware-tested firmware = 25976e82976bf7ed78b0506640db62bd0779ec5f
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Exact legacy boundary:

```c
doomCanvas->isUpdateView = true;
```

### Permanent owner

```text
ESP32/include/esp_post_load_view_invalidation_state.h
ESP32/src/esp_post_load_view_invalidation_state.c
EspPostLoadViewInvalidationState = 4 B
stateFNV = 4561c3c1
persistent heap = 0 B
```

Real-CYD state:

```text
isUpdateView=1->1
targetMap=9
active=1
```

The write is an identity assignment on the fresh-Junction path but remains an explicit caller-order boundary.

### Semantic proof

```text
redrawRequested=yes
identityAssignment=yes
edgeTransition=no
legacyIsUpdateView=1->1
legacyMutation=no
renderTriggered=no
presentation=no
```

### Strict predecessor proof

```text
eventParticleBytes=8
eventParticleFNV=8bc79e2b
unchanged=yes
callerOrder=yes
particleTopologyCanonical=yes
activeList=0
freeList=64
totalPool=64
```

Fail-closed proof:

```text
nullCleanup=1
nullOutput=1
inactiveCleanup=1
targetMap=1
invalidValue=1
prepareAtomic=yes
postActivePrepare=1
repeat=1
repeatAtomic=yes
```

Resident stayed exact:

```text
snapshotFNV=bb714d80->bb714d80
runtimeFNV=bc432a0f
mapFNV=8dba0bb4
scriptFNV=bc9b18ff
lineFNV=3658710d
textureFNV=537319ad
automapFNV=b699bd75
topologyFNV=d6e8df7d
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes
```

RAM / side-effect proof:

```text
heap8=72588->72588
largest8=34804->34804
persistentHeapBytes=0
GameMutation=no
PlayerMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
ParticleSystemMutation=no
legacyDoomCanvas_updateViewTrueCalled=no
```

Same-build equality witnesses only:

```text
gameFNV=6960d5bb->6960d5bb
playerFNV=c64e7862->c64e7862
hudFNV=d2deba0f->d2deba0f
canvasFNV=d140bc71->d140bc71
renderFNV=f9344dec->f9344dec
frameFNV=faa62417->faa62417
eventQueueFNV=d985589f->d985589f
particleFNV=f186cf0c->f186cf0c
```

Stable heartbeat:

```text
heap=138352
heap8=72588
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
DoomCanvas_setState(ST_PLAYING)             [NEXT after merge]
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
 -> post-load chain through event/particle       [hardware-proven]
 -> post-load view invalidation                  [hardware-proven]
 -> ST_PLAYING transition                        [NEXT]
 -> native gameplay
 -> native renderer
```

Current hardware PARK:

```text
state=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativeEventParticleCleanup=yes
nativeViewInvalidation=yes
initialSavePersistencePending=yes
eventParticleCleanupPending=no
isUpdateViewPending=no
ST_PLAYINGPending=yes
idleTimePending=yes
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

## Merge recommendation

```text
MERGE agent/esp32-native-post-load-view-invalidation
```

Hardware-tested firmware:

```text
25976e82976bf7ed78b0506640db62bd0779ec5f
```

## Next bounded milestone after merge

Recover exact new `main`, then recover the full legacy semantics of:

```c
DoomCanvas_setState(doomCanvas, ST_PLAYING);
```

Do not treat this as a simple scalar write. Audit all `ST_INTRO -> ST_PLAYING` behavior inside `DoomCanvas_setState()` before designing the native owner. Keep `idleTime = time + 8000`, durable save storage, entity gameplay and renderer migration separate unless the function itself proves a side effect inseparable from the state transition.
