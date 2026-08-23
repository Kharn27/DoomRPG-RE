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

Older archives remain indexed by Git history. `PORTING_STATUS.md` is the preferred recovery point.

## Latest merged boundary

PR #79 hardware-proved:

```c
Player_selectWeapon(player, player->weapon);
```

Canonical result:

```text
EspPostLoadWeaponSelectState=8 B
stateFNV=699f3cf3
weaponBefore=requestedWeapon=weaponAfter=2
viewInvalidationRequested=0
persistent heap=0 B
ST_PLAYING=no
```

## Current merge-ready milestone

[`MAP1_NATIVE_POST_LOAD_INITIAL_SAVE_INTENT.md`](MAP1_NATIVE_POST_LOAD_INITIAL_SAVE_INTENT.md)
hardware-proves the next conditional caller boundary:

```text
branch = agent/esp32-native-post-load-initial-save-intent
base   = 04e4e2269a6c70db3f3e4027717bdb36f286ce65
hardware-tested firmware = 0da9526775b706606338045babeb89e0d6c72729
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Exact legacy callsite:

```c
if ((doomCanvas->loadMapID != MAP_END_GAME) &&
    (game->isLoaded == false)) {
    Game_saveState(game,
                   doomCanvas->loadMapID,
                   doomCanvas->viewX,
                   doomCanvas->viewY,
                   doomCanvas->viewAngle,
                   false);
}
```

The ESP32 native path represents this as a semantic save intent rather than
reproducing legacy J2ME/desktop persistence files.

### Permanent owner

```text
ESP32/include/esp_post_load_initial_save_intent.h
ESP32/src/esp_post_load_initial_save_intent.c
EspPostLoadInitialSaveIntentState = 24 B
stateFNV = 0bf1a911
persistent heap = 0 B
```

Real-CYD state:

```text
mapId=9
view=992/1888
angle=64
isLoadedBefore=0
saveMode=0
saveRequired=1
componentMask=0f
persistenceDeferred=1
presentationDeferred=1
active=1
```

Component intent:

```text
CONFIG       = requested
PLAYER2      = requested
WORLD        = requested
PLAYER_ROUTE = requested
saveFileWrite=no
savingUi=no
presentation=no
```

### Strict input proof

```text
weaponFNV=699f3cf3
giveMapFNV=448e587d
hudClearFNV=b7383e18
viewFNV=afcdcf74
facingFNV=95aa1108
unchanged=yes
callerOrder=yes
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

Fail-closed/deferred proof:

```text
nullWeapon=1
nullView=1
nullOutput=1
inactiveWeapon=1
weaponMismatch=1
inactiveView=1
viewPending=1
loadedContextDeferred=1
invalidLoaded=1
prepareAtomic=yes
postActivePrepare=1
repeat=1
repeatAtomic=yes
```

RAM / side-effect proof:

```text
heap8=72644->72644
largest8=34804->34804
persistentHeapBytes=0
gameFNV=6960d5bb->6960d5bb
playerFNV=c64e7862->c64e7862
hudFNV=b18611d2->b18611d2
canvasFNV=592d59c9->592d59c9
renderFNV=f9344dec->f9344dec
frameFNV=6a0726c1->6a0726c1
legacyRuntimeClear=yes
legacyGame_saveStateCalled=no
```

Persistence remains deliberately deferred:

```text
legacyNewMapPresent=no
legacyNewDest=0/0
legacyNewAngle=0
routePayloadOwned=no
playerPersistence=no
worldPersistence=no
configPersistence=no
```

The post-PARK normal firmware heartbeat was healthy:

```text
heap=138408
heap8=72644
largest8=34804
SD=ready ZIP=ready VIDEO=ready CORE=ready LAYOUT=ready
PRERENDER=ready RENDER=ready MAPPINGS=ready MENUBSP=ready
```

## SD test incident

An earlier boot of the same candidate stopped during startup with
`SD=unavailable`, which naturally cascaded into ZIP/layout/render/menu skips.
The milestone had not executed and the branch did not modify SD startup code or
wiring. The user confirmed the microSD card itself was the cause. Restoring it
allowed the same firmware to reach the successful hardware block above; no SD
code workaround was added.

## SAVEGAME route ownership note

Opcode-27 route parsing remains hardware-proven, but the old temporary MAP_INTRO
probe resets its sampled `EspMapSaveRouteState` before PARK. A live cross-map
route payload therefore still requires explicit future native ownership.

## Probe completion semantics

Historical probes may set `done=1` on terminal failure, so `*_isDone()` alone is
not a PASS certificate. The current save-intent probe revalidates its exact
predecessor owners/world boundary and itself sets `done=1` only after successful
PARK.

## Hardware-proven canons through current branch

```text
Entrance snapshotFNV=b3811f3d
Junction sourceFNV=fefaf5ca
Junction post-GIVEMAP snapshotFNV=bb714d80
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
```

## Exact caller order

```text
DoomCanvas_finishRotation()                  [hardware-proven]
Hud message-channel clear                    [hardware-proven]
if Junction: Game_givemap()                 [hardware-proven]
Player_selectWeapon(player, player->weapon) [hardware-proven]
conditional Game_saveState(...)             [hardware-proven semantic intent]
Game.isLoaded=false                         [NEXT after merge]
Game.isSaved=false                          [NEXT after merge]
Game.activeLoadType=0                       [NEXT after merge]
numEvents=0 / particles cleared             [deferred]
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
 -> current-weapon self-selection                [hardware-proven]
 -> initial-save semantic intent                 [hardware-proven]
 -> post-load flag cleanup                       [next]
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
nativeHudClear=yes
nativePostLoadGiveMap=yes
nativeWeaponSelfSelect=yes
nativeInitialSaveIntent=yes
initialSaveDecisionPending=no
initialSavePersistencePending=yes
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

## Merge recommendation

```text
MERGE agent/esp32-native-post-load-initial-save-intent
```

Hardware-tested firmware:

```text
0da9526775b706606338045babeb89e0d6c72729
```

## Next bounded milestone after merge

Recover exact new `main`, then own only:

```c
game->isLoaded = false;
game->isSaved = false;
game->activeLoadType = 0;
```

Keep queued-event/particle cleanup, `isUpdateView`, `ST_PLAYING`, rendering and
native durable save storage separate.
