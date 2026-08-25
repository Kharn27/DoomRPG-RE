# ESP32 documentation map

This file defines the current ESP32 CYD documentation map.

## Source of truth

- [`README.md`](README.md): stable build/flash guide.
- [`PORTING_STATUS.md`](PORTING_STATUS.md): authoritative current recovery point.
- Milestone archives: implementation contracts and real-hardware evidence.

When chat history and repository documentation disagree, current `main` + `PORTING_STATUS.md` + the latest relevant milestone archive win.

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
| [`MAP1_NATIVE_PLAYING_SERVICE.md`](MAP1_NATIVE_PLAYING_SERVICE.md) | first permanent native PLAYING service iteration | #86 | `bf1275037fd22504077f6ff2bbf57e14721edf0a` |
| [`MAP1_NATIVE_GRAPHICS_CATALOG.md`](MAP1_NATIVE_GRAPHICS_CATALOG.md) | compact PAK-backed Junction graphics catalog | #87 | `91a17414859fa12a0553e5b011956b6f95165780` |
| [`MAP1_NATIVE_FIRST_JUNCTION_FRAME.md`](MAP1_NATIVE_FIRST_JUNCTION_FRAME.md) | first native Junction walls+planes frame and permanent CYD panel profile | #88 | `d8da51e5a3b9700d1806110f56f553a422d7d182` |

Older archives remain indexed by Git history. `PORTING_STATUS.md` is the preferred recovery point.

## Latest merged boundary

PR #88 established the first deterministic native Junction gameplay framebuffer, textured planes and the permanent hardware-selected CYD display profile.

```text
main = d8da51e5a3b9700d1806110f56f553a422d7d182
firstFrameFNV=8910c2ed
viewportFNV=032ffaed
catalogFNV=969d5a77
shapeData=NULL
mediaTexels=NULL
```

## Current merge-ready milestone

[`MAP1_NATIVE_JUNCTION_SPRITES.md`](MAP1_NATIVE_JUNCTION_SPRITES.md) hardware-proves the first native Junction billboard sprite pass.

```text
branch = agent/esp32-native-junction-sprites
base   = d8da51e5a3b9700d1806110f56f553a422d7d182
hardware-tested firmware = 3fdb2905b1d49ef1112a9e9df7a5db7e278897bd
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

### Predecessor frame canon

```text
frameBeforeFNV=8910c2ed
viewportBeforeFNV=032ffaed
viewport=160x80 @ 0,20
```

### BSP-visible view-sprite admission

The milestone reproduces legacy `Render_relinkSprite()` leaf ownership and admits sprites only from leaves visited by the same stateful BSP/depth walk used for the native frame.

```text
BSP walk = nodes:39 leaves:12 nodeCull:8 lines:62 backface:20 clip:8
mapSprites=48
bspCandidates=21
bspRejected=27
hidden=0
candidate modes=0:14 / 7:7
hardware census candidateFNV=23ef1895
permanent orderFNV=f16737cb
```

The 27 rejected sprites do not contribute pixels in the canonical Junction pose. Filtering them reduces work/PAK reads while preserving the final image.

### Native billboard raster canon

```text
modes=0:14 / 7:7
mode7Pixels=311
draws=21
nearCull=0
clipCull=0
spanRuns=219
pixels=1828
wallOccludedCols=62
frameLoads=21
uniqueLogical=9
frameBytes=12251
maxFrameBytes=1020
packReads=130
glowDeferred=7
```

Mode 7 uses the legacy additive RGB565 saturation rule. Physical bitshapes/texels are resolved through bounded native PAK reads; no resident legacy graphics pool is installed.

### Stable post-sprite framebuffer

```text
frameAfterFNV=299506eb
viewportAfterFNV=ae2246eb
BMP=/junction-sprite-viewport.bmp
BMP bytes=38454
```

The BMP is a direct logical-framebuffer diagnostic, independent of TFT/panel/camera effects.

### RAM / ownership proof

```text
heapDelta=0
largestDelta=0
legacyRenderStable=yes
topologyFNV=d6e8df7d
catalogFNV=969d5a77
packClosed=yes
shapeData=NULL
mediaTexels=NULL
mediaTexelOffsets=NULL
mediaBitShapeOffsets=NULL
mediaTexturesIds=NULL
mediaSpriteIds=NULL
legacy Game.entities=0
legacy Game.monsters=0
```

The sprite renderer uses one temporary bounded workspace and restores borrowed Render projection scratch exactly before return.

### Still deferred inside sprite rendering

Legacy automatically spawns additive glow companions after some base sprites. These are deliberately outside the current milestone:

```text
135/140 -> companion logical 136, mode 7
131 -> companion logical 144, mode 7 when encountered
current Junction pose: glowDeferred=7
```

The next milestone must extend the sparse catalog dependency closure so implicit companions are explicit native-owned resources before rendering them.

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
JunctionGraphicsCatalogFNV=969d5a77
JunctionGraphicsTextureRecordsFNV=2dd5dfcf
JunctionGraphicsSpriteRecordsFNV=cfd036cf
JunctionFirstFrameFNV=8910c2ed
JunctionViewportFNV=032ffaed
JunctionSpriteFrameFNV=299506eb
JunctionSpriteViewportFNV=ae2246eb
JunctionSpriteCandidateFNV=23ef1895
JunctionSpriteOrderFNV=f16737cb
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
 -> sparse native graphics catalog               [hardware-proven]
 -> native wall + textured-plane gameplay frame  [hardware-proven]
 -> raw CYD presentation                         [hardware-proven]
 -> BSP-visible native billboard rendering       [hardware-proven]
 -> implicit sprite dependency/glow family       [NEXT]
 -> native HUD/input/turn/gameplay
 -> expanded native renderer
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
nativeGraphicsCatalog=yes
nativeFirstFrame=yes
texturedPlanes=yes
nativeBaseBillboards=yes
bspVisibleOnly=yes
intrinsicMode7=yes
glowPending=yes
hudPending=yes
gameplayDispatchPending=yes
initialSavePersistencePending=yes
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

Mandatory invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
mediaTexelOffsets == NULL
mediaBitShapeOffsets == NULL
runtime ZIP map/graphics access forbidden
legacy Game.entities = 0
legacy Game.monsters = 0
```

## Classic CYD presentation profile

The logical framebuffer remains standard raw RGB565 and presentation remains exact nearest-neighbour x2 from 160x120 to 320x240. The hardware-selected panel profile is inversion ON, TFT byte swap ON, no software saturation or R/B swap, with the selected inverted gamma table:

```text
00 15 17 07 11 06 2b 56 3c 05 10 0f 3f 3f 0f
```

## Merge recommendation

```text
MERGE agent/esp32-native-junction-sprites
```

Hardware-tested firmware:

```text
3fdb2905b1d49ef1112a9e9df7a5db7e278897bd
```

All commits after this tested SHA are documentation-only.

## Next bounded milestone after merge

Recover the exact new `main` SHA, then close the sparse native graphics dependency graph for the implicit legacy glow family and render only those companions. Preserve the current BSP-visible candidate set, world/gameplay PARK, bounded temporary scratch, PAK-backed range access and all no-legacy-graphics-pool invariants.
