# ESP32 documentation map

This file indexes the current classic-CYD port documentation.

## Source of truth

- [`README.md`](README.md): stable build/flash guide.
- [`PORTING_STATUS.md`](PORTING_STATUS.md): authoritative recovery point and current hardware PARK.
- Milestone archives: implementation contracts plus real-CYD evidence.

If chat history and repository state disagree, current GitHub `main` + `PORTING_STATUS.md` + the latest relevant milestone archive win.

## Recent merged milestones

| Archive | Purpose | PR | Merged `main` |
| --- | --- | ---: | --- |
| [`MAP1_NATIVE_PLAYING_SERVICE.md`](MAP1_NATIVE_PLAYING_SERVICE.md) | first permanent native PLAYING service | #86 | `bf1275037fd22504077f6ff2bbf57e14721edf0a` |
| [`MAP1_NATIVE_GRAPHICS_CATALOG.md`](MAP1_NATIVE_GRAPHICS_CATALOG.md) | compact PAK-backed graphics catalog | #87 | `91a17414859fa12a0553e5b011956b6f95165780` |
| [`MAP1_NATIVE_FIRST_JUNCTION_FRAME.md`](MAP1_NATIVE_FIRST_JUNCTION_FRAME.md) | first Junction walls+planes frame + permanent CYD panel profile | #88 | `d8da51e5a3b9700d1806110f56f553a422d7d182` |
| [`MAP1_NATIVE_JUNCTION_SPRITES.md`](MAP1_NATIVE_JUNCTION_SPRITES.md) | BSP-visible native Junction billboard rendering | #89 | `674b45bbd115cd8f9202f2ce2d7132550c3bb75e` |

Older archives remain available in Git history; `PORTING_STATUS.md` is the preferred recovery entry point.

## Latest merged boundary

PR #89 established the first hardware-proven BSP-visible native billboard pass on Junction.

```text
main=674b45bbd115cd8f9202f2ce2d7132550c3bb75e
base world frame=8910c2ed
base sprite frame=299506eb
base sprite viewport=ae2246eb
mapSprites=48
BSP-visible candidates=21
BSP-rejected=27
modes=0:14 / 7:7
shapeData=NULL
mediaTexels=NULL
```

Merged evidence: [`MAP1_NATIVE_JUNCTION_SPRITES.md`](MAP1_NATIVE_JUNCTION_SPRITES.md).

## Current merge-ready milestone

[`MAP1_NATIVE_JUNCTION_GLOWS.md`](MAP1_NATIVE_JUNCTION_GLOWS.md) records the current real-CYD PASS.

```text
branch=agent/esp32-native-junction-glows
base=674b45bbd115cd8f9202f2ce2d7132550c3bb75e
hardware-tested firmware=338388ee4166115585e2c964aa95e79d5b0313eb
status=REAL-CYD HARDWARE PASS / MERGE-READY
```

### Direct catalog predecessor

```text
stateFNV=969d5a77
textures=30
sprites=16
storage=1840 B
textureFNV=2dd5dfcf
spriteFNV=cfd036cf
```

### Native dependency closure

The current Junction view requires the legacy implicit family:

```text
135/140 -> companion logical sprite 136 / mode 7
```

The native catalog now closes that dependency atomically instead of bypassing ownership with ad-hoc PAK reads.

Hardware canon:

```text
closedCatalogFNV=257444a5
textures=30
sprites=17
storage=1880 B
persistentIncrement=40 B
dependency=136
largest8=34804->34804
repeatAtomic=yes
packClosed=yes
```

Generic dependency semantics retain `131 -> 144` for future maps/views; current Junction requires only 136.

### Shared BSP visibility/depth primitive

This milestone extracts the stateful compact BSP visibility/depth walk into a reusable native primitive. It publishes visited leaves plus the 160-column depth boundary and restores borrowed legacy projection scratch exactly.

Hardware parity remains:

```text
nodes=39
leaves=12
nodeCull=8
lines=62
backface=20
clip=8
occluder=0
spriteSpan=0
orderFNV=f16737cb
```

### Base billboard pass remains preserved

```text
objects=48
bspCandidates=21
bspRejected=27
modes=0:14 / 7:7
mode7Pixels=311
draws=21
nearCull=0
clipCull=0
spans=219
pixels=1828
wallOccludedCols=62
frameLoads=21
uniqueLogical=9
frameBytes=12251
maxFrameBytes=1020
preserved=yes
```

### Hardware-proven glow companions

Legacy ordering is preserved: each additive companion is rendered immediately after its parent.

```text
companions=7
draws=7
nearCull=0
clipCull=0
spans=59
pixels=1917
wallOccludedCols=32
frameLoads=7
frameBytes=5572
maxFrameBytes=796
packReads=172
renderMode=7 additive RGB565
```

The additive lamp glow is visibly present on the real CYD.

### Stable complete framebuffer

```text
predecessor world frame=8910c2ed
pre-glow sprite frame=299506eb
complete sprite+glow frame=b5218f24
complete viewportFNV=9206eb24
BMP=/junction-sprite-viewport.bmp
BMP bytes=38454
viewport=160x80 @ 0,20
```

### RAM / ownership proof

```text
catalog persistent increment=40 B
renderer heapDelta=0
renderer largestDelta=0
topologyFNV=d6e8df7d
closedCatalogFNV=257444a5
packClosed=yes
shapeData=NULL
mediaTexels=NULL
mediaTexelOffsets=NULL
mediaBitShapeOffsets=NULL
mediaTexturesIds=NULL
mediaSpriteIds=NULL
legacy Game.entities=0
legacy Game.monsters=0
no world/entity mutation
no input consumption
no turn advancement
no gameplay dispatch
```

## Stable recovery canons through current branch

```text
Entrance snapshotFNV=b3811f3d
Junction sourceFNV=fefaf5ca
Junction snapshotFNV=bb714d80
runtimeFNV=bc432a0f
mapFNV=8dba0bb4
scriptFNV=bc9b18ff
lineFNV=3658710d
textureStateFNV=537319ad
automapFNV=b699bd75
topologyFNV=d6e8df7d
JunctionNativeSTPlayingFNV=73bc9acd
JunctionNativePlayingServiceFNV=4c50b853
DirectGraphicsCatalogFNV=969d5a77
DirectTextureRecordsFNV=2dd5dfcf
DirectSpriteRecordsFNV=cfd036cf
ClosedGlowCatalogFNV=257444a5
JunctionFirstFrameFNV=8910c2ed
JunctionFirstViewportFNV=032ffaed
JunctionBaseSpriteFrameFNV=299506eb
JunctionBaseSpriteViewportFNV=ae2246eb
JunctionSpriteCandidateFNV=23ef1895
JunctionSpriteOrderFNV=f16737cb
JunctionGlowFrameFNV=b5218f24
JunctionGlowViewportFNV=9206eb24
```

## Architecture direction

```text
original behavior/data
 -> /DoomRPG-ESP32.pak
 -> compact immutable map                         [hardware-proven]
 -> compact mutable overlays                      [hardware-proven]
 -> native event semantics                        [hardware-proven by family]
 -> native transition/residency                   [hardware-proven]
 -> native fresh-map player/post-load chain       [hardware-proven]
 -> first native PLAYING service                  [hardware-proven]
 -> sparse native graphics catalog                [hardware-proven]
 -> native walls + textured planes                [hardware-proven]
 -> raw CYD presentation                          [hardware-proven]
 -> BSP-visible native billboards                 [hardware-proven]
 -> implicit sprite dependency closure + glows    [hardware-proven]
 -> native gameplay HUD painting                  [NEXT]
 -> native input/turn/gameplay dispatch
 -> expanded native renderer/gameplay
```

## Current hardware PARK

```text
legacyState=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativeST_PLAYING=yes
nativePlayingService=yes
nativeGraphicsCatalog=yes
nativeFirstFrame=yes
texturedPlanes=yes
nativeBaseBillboards=yes
bspVisibleOnly=yes
intrinsicMode7=yes
glowCompanions=yes
glowPending=no
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
runtime ZIP map/graphics access forbidden
legacy Game.entities == 0
legacy Game.monsters == 0
```

## Classic CYD presentation profile

The logical framebuffer remains raw RGB565, presented by exact nearest-neighbour x2 from 160x120 to 320x240. Permanent hardware-selected panel policy:

```text
inversion=ON
TFT byte swap=ON
software saturation/gamma transform=none
software R/B swap=none
TFT_RGB_ORDER=TFT_BGR
driver=ILI9341_2_DRIVER
SPI=55 MHz
gamma=00 15 17 07 11 06 2b 56 3c 05 10 0f 3f 3f 0f
```

## Merge recommendation

```text
MERGE agent/esp32-native-junction-glows
```

Hardware-tested firmware:

```text
338388ee4166115585e2c964aa95e79d5b0313eb
```

All commits after that tested SHA must be documentation-only before merge-ready declaration.

## Next bounded milestone after merge

Recover the exact new `main` SHA, then implement **native gameplay HUD painting** as a separate visible boundary. Consume the already-owned native HUD/player/view intent only; do not combine it with input, turn advancement or gameplay dispatch.

Keep the existing renderer/PARK constraints: same 160x120 framebuffer, native PAK only, bounded caches/scratch, no legacy gameplay/render loop, `shapeData=NULL`, `mediaTexels=NULL`.