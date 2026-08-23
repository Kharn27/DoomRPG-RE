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
| [`MAP1_NATIVE_POST_LOAD_IDLE_TIME.md`](MAP1_NATIVE_POST_LOAD_IDLE_TIME.md) | final fresh-map idle deadline / load-tail completion | #85 | `cdd7f3c7bdd7f1ea472faaccf64d055e7a00a4a2` |

Older archives remain indexed by Git history. `PORTING_STATUS.md` is the preferred recovery point.

## Latest merged boundary

PR #85 completed the successful fresh-Junction `DoomCanvas_loadMedia()` caller tail.

Stable result:

```text
nativeST_PLAYING=yes
nativeIdleTime=yes
postLoadTailComplete=yes
legacyST_PLAYING=no
```

The idle-time owner is 16 B and preserves `idleTimeAfter-timeBefore=8000`; its FNV is uptime-dependent and therefore not a cross-boot canon.

## Current merge-ready milestone

[`MAP1_NATIVE_PLAYING_SERVICE.md`](MAP1_NATIVE_PLAYING_SERVICE.md) hardware-proves the first permanent native PLAYING service iteration.

```text
branch = agent/esp32-native-playing-service
base   = cdd7f3c7bdd7f1ea472faaccf64d055e7a00a4a2
hardware-tested firmware = e9c10c8759588e48478d3d702292628411c5939e
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

### Permanent owner

```text
ESP32/include/esp_native_playing_service_state.h
ESP32/src/esp_native_playing_service_state.c
EspNativePlayingServiceState = 12 B
stateFNV = 4c50b853
persistent heap = 0 B
```

Hardware-proven state:

```text
nativeState=3
serviceOrdinal=1
inputCountBefore=0
inputConsumed=0
gameplayDispatched=0
renderIntent=1
renderDeferred=1
presentationDeferred=1
hudIntent=1
targetMapId=9
active=1
```

### First-service gates

```text
renderOnly=0
health=30
waitTime=0
activeSprites=0
monstersTurn=0
openDoors=0
animDoors=0
viewSettled=yes
isUpdateView=1
particleCount=0
idleDeadlinePending=yes
currentTime=4200
nativeIdleDeadline=12200
activeMonstersPresent=no
```

This proves that the first native PLAYING iteration is safe and idle. `monstersTurn=0` also proves that the legacy path would request view/render/HUD work, so `renderIntent=1` / `hudIntent=1` reflect recovered behavior while actual rendering remains deferred.

### Strict predecessor / fail-closed proof

```text
playingBytes=12
playingFNV=73bc9acd
unchanged=yes
idleBytes=16
idleFNV=1e6f9a0e
unchanged=yes
idleDelta=8000
callerOrder=yes
particleTopologyCanonical=yes
activeList=0
freeList=64
totalPool=64

nullPlaying=1
nullIdle=1
nullOutput=1
inactivePlaying=1
badIdle=1
inputPending=1
prepareAtomic=yes
postActivePrepare=1
repeat=1
repeatAtomic=yes
```

`idleFNV=1e6f9a0e` is a run-specific boot witness only.

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
heap8=72516->72516
largest8=34804->34804
persistentHeapBytes=0
legacyState=9->9
frameFNV=ee9d9dbc->ee9d9dbc
GameMutation=no
PlayerMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
ParticleSystemMutation=no
frameMutation=no
packClosed=yes
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
JunctionNativePlayingServiceFNV=4c50b853
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
 -> native PLAYING service/dispatch              [hardware-proven first iteration]
 -> first native gameplay framebuffer frame      [NEXT]
 -> native input/turn/gameplay
 -> expanded native renderer/presentation
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
nativePlayingService=yes
playingServicePending=no
firstFramePending=yes
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
MERGE agent/esp32-native-playing-service
```

Hardware-tested firmware:

```text
e9c10c8759588e48478d3d702292628411c5939e
```

All commits after that tested SHA must remain documentation-only.

## Next bounded milestone after merge

Recover exact new `main`, then consume `EspNativePlayingServiceState` with stable `stateFNV=4c50b853` to produce the first native Junction gameplay framebuffer frame.

That milestone may finally mutate/present the 160x120 RGB565 framebuffer, but must keep input, turn advancement, entities/monsters and legacy `DoomCanvas_playingState()` / `Render_render()` outside.
