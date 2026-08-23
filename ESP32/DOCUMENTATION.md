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

PR #79 hardware-proved the exact caller operation:

```c
Player_selectWeapon(player, player->weapon);
```

Canonical result:

```text
EspPostLoadWeaponSelectState=8 B
stateFNV=699f3cf3
weaponBefore=requestedWeapon=weaponAfter=2
viewInvalidationRequested=0
selfSelect=yes
identityAssignment=yes
updateViewBranchTaken=no
persistent heap=0 B
ST_PLAYING=no
```

## Current hardware candidate

[`MAP1_NATIVE_POST_LOAD_INITIAL_SAVE_INTENT.md`](MAP1_NATIVE_POST_LOAD_INITIAL_SAVE_INTENT.md)
recovers only the next conditional caller boundary:

```text
branch = agent/esp32-native-post-load-initial-save-intent
base   = 04e4e2269a6c70db3f3e4027717bdb36f286ce65
status = HARDWARE CANDIDATE — NOT YET CYD-PROVEN
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

The current native caller arguments already come from the hardware-proven
`EspPlayerView`:

```text
mapId=9
viewX=992
viewY=1888
viewAngle=64
```

### Why this is an intent boundary

Current `Game_saveState()` bundles unrelated legacy concerns:

```text
Saving... UI/presentation
Config serialization
Player2 current checkpoint
World serialization through pointer-heavy Entity_t state
Player future-route checkpoint
```

The ESP32 candidate does not reproduce those file formats. It parks the semantic
request and records persistence/presentation as deferred. This preserves caller
order without reintroducing desktop/J2ME architecture.

### Permanent owner

```text
ESP32/include/esp_post_load_initial_save_intent.h
ESP32/src/esp_post_load_initial_save_intent.c
EspPostLoadInitialSaveIntentState = 24 B candidate
persistent heap = 0 B
```

Component mask for the exact `z=false` caller:

```text
CONFIG       0x01
PLAYER2      0x02
WORLD        0x04
PLAYER_ROUTE 0x08
ALL          0x0f
```

Expected hardware state semantics:

```text
stateBytes=24
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

The real CYD establishes the state FNV.

### Strict input boundary

The probe revalidates rather than trusting predecessor `isDone()` alone:

```text
weaponFNV=699f3cf3
giveMapFNV=448e587d
hudClearFNV=b7383e18
viewFNV=afcdcf74
facingFNV=95aa1108
snapshotFNV=bb714d80
runtimeFNV=bc432a0f
mapFNV=8dba0bb4
automapFNV=b699bd75
```

`Game.isLoaded` is sampled legacy read-only only because a permanent native load-
state owner does not exist yet. `isLoaded=1` is explicitly deferred/refused by
this fresh-load milestone.

### Required side-effect proof

```text
saveFileWrite=no
savingUi=no
presentation=no
legacy Game_saveState not called
heap/largest delta=0
frame unchanged
legacy Game/Player/Hud/DoomCanvas/Render unchanged
PAK closed
ST_PLAYING=no
entities=0
monsters=0
```

## SAVEGAME route ownership note

Opcode-27 route parsing remains hardware-proven, but the old temporary MAP_INTRO
probe resets its probe-local sampled `EspMapSaveRouteState` before PARK. The old
`routeLifetimeCrossMap=yes` wording therefore does not mean a live route owner
currently survives the resident handoff.

The new save intent requests the `PLAYER_ROUTE` persistence component but does
not fabricate a missing route payload. Durable cross-map route persistence stays
an explicit future save-system responsibility.

## Probe completion semantics

Historical probes may set `done=1` on terminal failure, so `*_isDone()` alone is
not a PASS certificate. New downstream probes must revalidate exact preceding
owners/world state.

The new initial-save-intent probe itself sets `done=1` only after successful
final PARK.

## Hardware-proven canons through PR #79

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
```

## Exact caller order

```text
DoomCanvas_finishRotation()                  [hardware-proven]
Hud message-channel clear                    [hardware-proven]
if Junction: Game_givemap()                 [hardware-proven]
Player_selectWeapon(player, player->weapon) [hardware-proven]
conditional Game_saveState(...)             [CURRENT INTENT CANDIDATE]
Game.isLoaded=false                         [next after PASS/merge]
Game.isSaved=false                          [next after PASS/merge]
Game.activeLoadType=0                       [next after PASS/merge]
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
 -> initial-save semantic intent                 [CURRENT CANDIDATE]
 -> post-load flag cleanup                       [next]
 -> remaining caller-side load completion
 -> ST_PLAYING
 -> native gameplay
 -> native renderer
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

Build and flash normal `esp32-cyd` from the current candidate branch. Return the
complete `[JUNCTIONSAVEINTENT]` block. Promote only after real-CYD PASS.
