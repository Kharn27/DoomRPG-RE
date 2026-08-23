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

Older archives remain indexed by Git history. `PORTING_STATUS.md` is the preferred recovery point.

## Latest merged boundary

PR #81 hardware-proved:

```c
game->isLoaded = false;
game->isSaved = false;
game->activeLoadType = 0;
```

Canonical result:

```text
EspPostLoadFlagCleanupState=8 B
stateFNV=46cb2547
isLoaded=0->0
isSaved=0->0
activeLoadType=0->0
targetMap=9
persistentHeapBytes=0
ST_PLAYING=no
```

## Current merge-ready milestone

[`MAP1_NATIVE_POST_LOAD_EVENT_PARTICLE_CLEANUP.md`](MAP1_NATIVE_POST_LOAD_EVENT_PARTICLE_CLEANUP.md)
hardware-proves the next exact caller block:

```text
branch = agent/esp32-native-post-load-event-particle-cleanup
base   = c4a093d9db77a715c355a68c5aae9faaddf22e0b
hardware-tested firmware = 48d47b1c2e6e7276ca555e5811933fd033f496ed
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Exact legacy sequence:

```c
doomCanvas->numEvents = 0;
ParticleSystem_freeAllParticles(doomCanvas->particleSystem);
doomCanvas->numEvents = 0;
```

### Permanent owner

```text
ESP32/include/esp_post_load_event_particle_cleanup_state.h
ESP32/src/esp_post_load_event_particle_cleanup_state.c
EspPostLoadEventParticleCleanupState = 8 B
stateFNV = 8bc79e2b
persistent heap = 0 B
```

Real-CYD state:

```text
numEvents=0->0->0
particleCount=0->0
targetMap=9
active=1
```

So the current fresh-Junction path reaches this cleanup already empty. The
explicit owner preserves caller order while non-empty event/particle contexts
remain fail-closed rather than silently discarded.

### Strict predecessor proof

```text
flagCleanupBytes=8
flagCleanupFNV=46cb2547
unchanged=yes
callerOrder=yes
```

### Particle topology proof

The hardware probe did not trust `particleCount` alone. It bounded-walked both
legacy circular lists and proved:

```text
particleTopologyCanonical=yes
activeList=0
freeList=64
totalPool=64
```

All links were reciprocal, all nodes belonged to `nodeListC[64]`, no duplicates
were seen, and all 64 pool nodes were covered exactly once. This confirms the
active particle list is genuinely empty.

### Fail-closed proof

```text
nullFlag=1
nullOutput=1
inactiveFlag=1
targetMap=1
invalidEvents=1
invalidParticles=1
nonemptyEvents=1
nonemptyParticles=1
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
heap8=72604->72604
largest8=34804->34804
persistentHeapBytes=0
GameMutation=no
PlayerMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
ParticleSystemMutation=no
legacyParticle_freeAllCalled=no
packClosed=yes
```

Same-build equality witnesses only:

```text
gameFNV=6960d5bb->6960d5bb
playerFNV=c64e7862->c64e7862
hudFNV=d2deba0f->d2deba0f
canvasFNV=96abe6d0->96abe6d0
renderFNV=f9344dec->f9344dec
frameFNV=805df09e->805df09e
eventQueueFNV=d985589f->d985589f
particleFNV=f186cf0c->f186cf0c
```

Stable heartbeat:

```text
heap=138368
heap8=72604
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
DoomCanvas.isUpdateView=true                [NEXT after merge]
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
 -> post-load chain through flag cleanup         [hardware-proven]
 -> empty event/particle cleanup                 [hardware-proven]
 -> isUpdateView caller write                    [next]
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
nativePostLoadFlagCleanup=yes
nativeEventParticleCleanup=yes
initialSavePersistencePending=yes
eventParticleCleanupPending=no
isUpdateViewPending=yes
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
MERGE agent/esp32-native-post-load-event-particle-cleanup
```

Hardware-tested firmware:

```text
48d47b1c2e6e7276ca555e5811933fd033f496ed
```

## Next bounded milestone after merge

Recover exact new `main`, then own only:

```c
doomCanvas->isUpdateView = true;
```

Keep `ST_PLAYING`, `idleTime`, rendering and durable native save storage separate.
