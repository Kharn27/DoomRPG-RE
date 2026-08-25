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

Older archives remain indexed by Git history. `PORTING_STATUS.md` is the preferred recovery point.

## Latest merged boundary

PR #87 established the compact native sparse graphics catalog.

```text
main = 91a17414859fa12a0553e5b011956b6f95165780
JunctionGraphicsCatalogFNV=969d5a77
textureRecordsFNV=2dd5dfcf
spriteRecordsFNV=cfd036cf
shapeData=NULL
mediaTexels=NULL
firstFramePending=yes
```

## Current merge-ready milestone

[`MAP1_NATIVE_FIRST_JUNCTION_FRAME.md`](MAP1_NATIVE_FIRST_JUNCTION_FRAME.md) hardware-proves the first real native Junction gameplay framebuffer and the permanent classic-CYD panel profile.

```text
branch = agent/esp32-native-first-junction-frame
base   = 91a17414859fa12a0553e5b011956b6f95165780
hardware-tested firmware = 09f670a2f11e1cfce065c55aef8a4d3a5711a9a3
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

### Native frame canon

```text
EspNativeFirstFrameState = 48 B
frameAfterFNV = 8910c2ed
viewport = 160x80 @ 0,20
viewportFNV = 032ffaed
BMP = /junction-viewport.bmp
BMP bytes = 38454
```

`frameBeforeFNV` and the complete first-frame state FNV are deliberately **not** cross-boot canons because the previously displayed framebuffer differs between runs.

### Native walls/BSP

```text
nodes=39
leaves=12
nodeCull=8
lineCandidates=62
backfaceCull=20
clipCull=8
occluderOnly=0
spriteSpanDeferred=0
wallRequests=34
wallDraws=34
spanCalls=166
wallPixels=4341
wallCache=17H/17M/14E
resolvedTextures=30
animationTime=0
```

### Native textured planes

```text
rows=80
pixels=12800
uniqueLogicalTextures=6
cache=12795H/5M/0E
texelReads=10240 B
```

### Resident / RAM / side-effect proof

```text
residentSnapshot=bb714d80->bb714d80
runtimeFNV=bc432a0f
mapFNV=8dba0bb4
scriptFNV=bc9b18ff
lineFNV=3658710d
textureStateFNV=537319ad
automapFNV=b699bd75
topologyFNV=d6e8df7d
playerViewFNV=afcdcf74
playingServiceFNV=4c50b853
catalogFNV=969d5a77
heapDelta=0
largestDelta=0
legacyRenderStable=yes
packClosed=yes
```

The strict probe additionally verifies unchanged Game, Player, Hud, DoomCanvas, Render, `Render.columnScale`, legacy palette, graphics catalog and resident state; wrong-map/null inputs fail closed and repeat is atomic.

### Classic CYD presentation profile

The logical framebuffer is standard raw RGB565 and remains untouched by display calibration.

```text
logical framebuffer = 160x120 RGB565 / 38400 B
physical output      = 320x240
resampling           = exact nearest-neighbour x2
software saturation = none
software R/B swap   = none
TFT byte swap        = ON
panel inversion      = ON
TFT_RGB_ORDER        = TFT_BGR
ILI9341 driver       = ILI9341_2_DRIVER
SPI frequency        = 55 MHz
```

Real-CYD primary testing proved raw RGB565 channel order correct. A later controller-only A-F comparison of the exact canonical Junction frame isolated the remaining poor colour/contrast reproduction to the ILI9341 gamma profile.

Hardware selected **B / INV-GAMMA-WA** without contest. The permanent `PlatformVideo_begin()` keeps the stock `ILI9341_2_DRIVER` power/VCOM/frame-rate setup and writes this table into both gamma registers `0xE0` and `0xE1`:

```text
00 15 17 07 11 06 2b 56 3c 05 10 0f 3f 3f 0f
```

`PlatformVideo_present()` remains a direct raw RGB565 nearest-neighbour x2 path. The final tested firmware contains no temporary `VIDEOCAL`, `VIDEOBOUNDARY`, `VIDEOPRIMARY` or `PANELCAL` carousel.

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
 -> native sprite gameplay rendering             [NEXT]
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
graphicsCatalogPending=no
firstFramePending=no
spritesPending=yes
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

## Merge recommendation

```text
MERGE agent/esp32-native-first-junction-frame
```

Hardware-tested firmware:

```text
09f670a2f11e1cfce065c55aef8a4d3a5711a9a3
```

All commits after that tested SHA are documentation-only.

## Next bounded milestone after merge

Recover exact new `main`, then render the Junction **native sprite pass** inside the same deterministic gameplay-frame boundary using the 16 hardware-proven sparse sprite catalog records.

Keep it bounded:

```text
same Junction pose/frame boundary
input consumed = no
turn advanced = no
gameplay dispatch = no
legacy Game.entities = 0
legacy Game.monsters = 0
legacy DoomCanvas.state = ST_INTRO
legacy DoomCanvas_playingState = not called
legacy Render_render = not called
shapeData = NULL
mediaTexels = NULL
runtime ZIP = forbidden
bounded sprite cache only
HUD painting deferred unless separately scoped
```
