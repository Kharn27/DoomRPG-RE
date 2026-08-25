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
| [`MAP1_NATIVE_JUNCTION_GLOWS.md`](MAP1_NATIVE_JUNCTION_GLOWS.md) | dependency-closed additive Junction glow companions | #90 | `30351fd0a867e18dad171962b00d70923b4d173f` |

Older archives remain available in Git history; `PORTING_STATUS.md` is the preferred recovery entry point.

## Latest merged boundary

PR #90 established the dependency-closed current Junction sprite set and seven native additive glow companions.

```text
main=30351fd0a867e18dad171962b00d70923b4d173f
closedCatalogFNV=257444a5
complete sprite+glow frame=b5218f24
complete viewportFNV=9206eb24
shapeData=NULL
mediaTexels=NULL
```

Merged evidence: [`MAP1_NATIVE_JUNCTION_GLOWS.md`](MAP1_NATIVE_JUNCTION_GLOWS.md).

## Current merge-ready milestone

[`MAP1_NATIVE_GAMEPLAY_HUD.md`](MAP1_NATIVE_GAMEPLAY_HUD.md) records the current real-CYD PASS.

```text
branch=agent/esp32-native-gameplay-hud
base=30351fd0a867e18dad171962b00d70923b4d173f
hardware-tested firmware=fa6b0d2ab4c1ec2598b92dfe635a84ff50a74867
status=REAL-CYD HARDWARE PASS / MERGE-READY
```

### Native HUD boundary

The current fresh-Junction pose now paints the original compact gameplay HUD natively:

```text
top band=0..19
world viewport=20..99 / 160x80
bottom band=100..119
health=30/30
armor=0/20
weapon=2 / pistol
ammoType=1
ammo=8
face=0 / normal
direction=N
```

The user visually confirmed the HUD on the physical classic CYD.

### PAK-backed HUD resources

The native indexed-BMP path reads only bounded rows from `/DoomRPG-ESP32.pak` and keeps no decoded image resident:

```text
assets=a.bmp,k.bmp,l.bmp,m.bmp,o.bmp
assetsValidated=5
bar=20x20
icon=13x13
face=18x20
PAK reads=184
PAK bytes=6344
source rows=164
painted pixels=7538
packClosed=yes
```

### Stable framebuffer canons

```text
pre-HUD frame=b5218f24
post-HUD frame=ba3e5182
world viewport=9206eb24
world viewport preserved=yes
HUD bands=9cf0c5c5->6c2aa46f
EspNativeGameplayHudState=22 B
HUD stateFNV=4756db9c
```

The complete-frame hash changes only in the top/bottom HUD bands. The 160x80 gameplay viewport remains byte-identical.

### HUD dirty ownership

```text
before dirty FNV=6965ee06 refreshPending=1
after dirty FNV=40c66f99 refreshPending=0
consume only after successful paint=yes
```

The legacy `Hud_t` object remains stable; native dirty ownership is not reflected back into legacy state.

### RAM / ownership proof

```text
heap8=70196->70196
largest8=34804->34804
heapDelta=0
largestDelta=0
legacyHudStable=yes
playerStable=yes
gameStable=yes
canvasStable=yes
renderStable=yes
residentStable=yes
topologyFNV=d6e8df7d
closedCatalogFNV=257444a5
packClosed=yes
shapeData=NULL
mediaTexels=NULL
legacy Game.entities=0
legacy Game.monsters=0
no input consumed
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
JunctionHudFrameFNV=ba3e5182
JunctionHudStateFNV=4756db9c
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
 -> native gameplay HUD painting                  [hardware-proven]
 -> native gameplay input/action dispatch         [NEXT]
 -> turn/entity/monster gameplay by bounded family
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
nativeHud=yes
hudPending=no
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
MERGE agent/esp32-native-gameplay-hud
```

Hardware-tested firmware:

```text
fa6b0d2ab4c1ec2598b92dfe635a84ff50a74867
```

All commits after that tested SHA must be documentation-only before merge-ready declaration.

## Next bounded milestone after merge

Recover the exact new `main` SHA, reread the legacy playing-event semantics and choose one small **native gameplay input/action family**. Unsupported actions must remain fail-closed.

Do not activate the broad legacy `DoomCanvas_handlePlayingEvents()` / gameplay loop, full turn advancement, monster gameplay or general entity activation in the first input milestone. Preserve the current renderer + HUD PARK, native PAK backing and no-PSRAM invariants.