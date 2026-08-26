# ESP32 documentation map

This file indexes the current classic-CYD port documentation.

## Source of truth

- [`README.md`](README.md): stable build/flash guide.
- [`PORTING_STATUS.md`](PORTING_STATUS.md): authoritative recovery point and current hardware PARK.
- Milestone archives: implementation contracts plus real-CYD evidence.

If chat history and repository state disagree, current GitHub `main` + `PORTING_STATUS.md` + this file + the latest relevant milestone archive win.

## Recent merged milestones

| Archive | Purpose | PR | Merged `main` |
| --- | --- | ---: | --- |
| [`MAP1_NATIVE_PLAYING_SERVICE.md`](MAP1_NATIVE_PLAYING_SERVICE.md) | first permanent native PLAYING service | #86 | `bf1275037fd22504077f6ff2bbf57e14721edf0a` |
| [`MAP1_NATIVE_GRAPHICS_CATALOG.md`](MAP1_NATIVE_GRAPHICS_CATALOG.md) | compact PAK-backed graphics catalog | #87 | `91a17414859fa12a0553e5b011956b6f95165780` |
| [`MAP1_NATIVE_FIRST_JUNCTION_FRAME.md`](MAP1_NATIVE_FIRST_JUNCTION_FRAME.md) | first Junction walls+planes frame + permanent CYD panel profile | #88 | `d8da51e5a3b9700d1806110f56f553a422d7d182` |
| [`MAP1_NATIVE_JUNCTION_SPRITES.md`](MAP1_NATIVE_JUNCTION_SPRITES.md) | BSP-visible native Junction billboard rendering | #89 | `674b45bbd115cd8f9202f2ce2d7132550c3bb75e` |
| [`MAP1_NATIVE_JUNCTION_GLOWS.md`](MAP1_NATIVE_JUNCTION_GLOWS.md) | dependency-closed additive Junction glow companions | #90 | `30351fd0a867e18dad171962b00d70923b4d173f` |
| [`MAP1_NATIVE_GAMEPLAY_HUD.md`](MAP1_NATIVE_GAMEPLAY_HUD.md) | native fresh-Junction gameplay HUD painter | #91 | `7686f7fb5c93d375f51a34ec0dd0b5cb127017e3` |
| [`MAP1_NATIVE_GAMEPLAY_INPUT.md`](MAP1_NATIVE_GAMEPLAY_INPUT.md) | calibrated invisible 12-zone gameplay touch intent owner | #92 | `cdda239f1c884a7d6f6707ba1c30a0a0a3603923` |
| [`MAP1_NATIVE_GAMEPLAY_TURN.md`](MAP1_NATIVE_GAMEPLAY_TURN.md) | native TURN_LEFT/TURN_RIGHT + cardinal render round-trip | #93 | `89f9d5f3feaa40f2e2a0c6e9506d1d8efaf5eeb6` |
| [`MAP1_NATIVE_GAMEPLAY_MOVE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_MOVE_COLLISION.md) | native cardinal movement + compact wall/entity collision | #94 | `b5a4426eb0df1ef1506893d4bc08b5538543a7b3` |
| [`MAP1_NATIVE_GAMEPLAY_RENDER_HOTPATH.md`](MAP1_NATIVE_GAMEPLAY_RENDER_HOTPATH.md) | viewport-only gameplay recomposition, no temporary HUD save/intermediate present | #95 | `f98a0b8e9eb4cbd38bf5678a1ce60c4989766985` |
| [`MAP1_NATIVE_GAMEPLAY_RENDER_RESOURCE_CACHE.md`](MAP1_NATIVE_GAMEPLAY_RENDER_RESOURCE_CACHE.md) | persistent bounded PAK/render-resource owner + exact small-range reuse | #96 | `377fce3de5381373750a7fba29d0c83b8142c583` |

Older archives remain available in Git history; `PORTING_STATUS.md` is the preferred recovery entry point.

## Latest merged boundary

PR #96 is the current merged hardware baseline:

```text
main=377fce3de5381373750a7fba29d0c83b8142c583
physical /DoomRPG-ESP32.pak owner = resident during native gameplay
entry descriptor cache = 24 slots
exact resident payload = 16384 B
exact range key capacity = 256
small exact range <=1024 B
owner struct=21160 B
canonical warm North reads=22 x 2048 B
shapeData=NULL
mediaTexels=NULL
```

Merged evidence: [`MAP1_NATIVE_GAMEPLAY_RENDER_RESOURCE_CACHE.md`](MAP1_NATIVE_GAMEPLAY_RENDER_RESOURCE_CACHE.md).

## Current candidate milestone

[`MAP1_NATIVE_GAMEPLAY_LARGE_RANGE_CACHE.md`](MAP1_NATIVE_GAMEPLAY_LARGE_RANGE_CACHE.md) records the shared-payload exact `2048 B` cache plus the bounded legacy wall-block guard required by unrestricted gameplay roaming.

```text
branch=agent/esp32-native-gameplay-large-range-cache
base=377fce3de5381373750a7fba29d0c83b8142c583
hardware-tested implementation SHA=1273f0205c0ba060972500bedd76effc974077bf
status=REAL-CYD HARDWARE PASS
```

### Shared-payload large range cache

The existing `16 KiB` resident payload is split logically from both ends:

```text
small <=1024 B ranges grow upward
exact 2048 B ranges grow downward
small ranges retain priority
large activation allocates nothing
same 256 range-record table
no second permanent heap owner
```

Canonical hardware proof:

```text
BASE  cache=9225/16384 B entries=195/256 large=off
LEARN cache=15369/16384 B entries=198/256 large=3
WARM  cache=15369/16384 B entries=198/256 large=3
savedReads=3
savedBytes=6144
ownerDelta=0
frame=ba3e5182
viewport=9206eb24
HUD=6c2aa46f
exact=yes
```

The warm strict frame used `19` physical `2048 B` reads instead of `22`, with total time roughly `232.9 ms`. The gain is real but modest; the user reported the game was already playable and did not perceive a dramatic change from these three retained ranges alone.

### Renderer boundary discovered by real roaming

The first unrestricted North traversal exposed a fail-closed wall sampler boundary at:

```text
view=992,1568,36
angle=64
line=90
logical=66
actual=140
flags=00002000
source=98304
packedIndex=2048
```

This disproved the earlier speculative double-height hypothesis: the line flags do not contain the `0x00010000` double-height bit.

The recovered desktop behavior explains the exact `2048` access: legacy `Render_loadTexels()` admitted required wall blocks, sorted them by source offset, then packed them contiguously into map-wide `mediaTexels`. The native cache keeps each block bounded, so the one-byte cross-block legacy read must be modeled explicitly rather than recreating `mediaTexels`.

### 16-byte BSS legacy guard

Final recovery is deliberately outside the deep renderer stack:

```text
LegacyWallGuard=16 B BSS
no new 2048 B buffer
no FirstFrameWork growth
no PAK read inside sampleWallSpan()
exact packedIndex=2048 only
all unrelated OOB remains fail-closed
```

Flow:

```text
SPAN_OOB=2048
 -> renderer unwinds completely
 -> resolve the next legacy compact wall block from mappings.bin
 -> read one packed byte from wtexels.bin
 -> store 16 B guard
 -> retry frame
```

Explicit real-CYD recovery witness:

```text
logical=15 actual=40 source=20480
successorActual=108 successorSource=61440 byte=aa
guard owner=BSS bytes=16
retry line=33
recovered=yes
```

The next MOVE committed normally and the user then roamed throughout the level without renderer failure, Guru Meditation, reboot, heap drift, or resident-state loss.

Final long-run memory/stack witness:

```text
heap=105424 stable
heap8=39756 stable
largest8=14836 stable
stackHighWater=924
MOVE execScratch=540 B
TURN execScratch=484 B
legacyStable=yes
residentStable=yes
```

The previous pre-guard stable heap witness was `39772 B`; the exact `16 B` reduction matches the BSS guard owner.

## Known renderer/view anomaly kept outside this milestone

The user still sees a confusing start/arrival-door view in some combinations of backing away / approaching while facing particular directions. Serial semantic orientation remains coherent:

```text
0=East
64=North
128=West
192=South
```

MOVE preserves orientation and TURN changes it by exactly `+/-64`, including canonical round trips. Do not alter gameplay orientation to hide this visual symptom.

The next bounded renderer investigation should instrument the door view / geometry / culling relation and prove whether the divergence is in camera transform, BSP visibility, wall orientation, or another render-only path.

## Stable recovery canons

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
JunctionHudBandsFNV=6c2aa46f
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
 -> native gameplay HUD                           [hardware-proven]
 -> native gameplay touch intent                  [merged]
 -> native TURN_LEFT/TURN_RIGHT                   [merged]
 -> native cardinal movement + collision          [merged]
 -> native viewport-only gameplay recomposition   [merged]
 -> bounded persistent render resource owner      [merged]
 -> shared-payload exact 2048 B reuse             [hardware-proven candidate]
 -> bounded legacy wall-block guard               [hardware-proven candidate]
 -> door/view renderer investigation              [preferred NEXT]
 -> turn advancement / tile events                [later]
 -> entity/monster gameplay                       [later]
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
nativeHud=yes
nativeInput=yes
nativeTurnDispatch=yes
nativeMovementDispatch=yes
nativeGameplayViewportHotPath=yes
persistentRenderResourceOwner=yes
smallExactRangeCache=yes
largeExact2048RangeCache=yes
legacyWallGuard=yes
TURN_LEFT=yes
TURN_RIGHT=yes
FORWARD/BACK/STRAFE native semantics=yes
static wall collision=yes
compact linked entity collision=yes
dynamic opened-line collision=fail-closed
initialSavePersistencePending=yes
legacy Game.entities=0
legacy Game.monsters=0
Game_advanceTurn=no
Game_executeTile=no
facingRefresh=deferred
```

Mandatory invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
runtime ZIP map/graphics access forbidden
/DoomRPG-ESP32.pak = native backing store
```

## Classic CYD presentation profile

```text
logical framebuffer=160x120 RGB565
physical=320x240
present=exact nearest-neighbour x2
inversion=ON
TFT byte swap=ON
software saturation/gamma transform=none
software R/B swap=none
TFT_RGB_ORDER=TFT_BGR
driver=ILI9341_2_DRIVER
SPI=55 MHz
gamma=00 15 17 07 11 06 2b 56 3c 05 10 0f 3f 3f 0f
```

## Merge boundary

Hardware-tested code SHA:

```text
1273f0205c0ba060972500bedd76effc974077bf
```

Any commit after that SHA must be documentation-only for the current hardware PASS to remain valid.

After merge, recover the exact new `main` SHA and reread `PORTING_STATUS.md`, this file, and [`MAP1_NATIVE_GAMEPLAY_LARGE_RANGE_CACHE.md`](MAP1_NATIVE_GAMEPLAY_LARGE_RANGE_CACHE.md) before branching.

Preferred next milestone: a minimal fail-only/view-only witness around the start-door anomaly. Keep semantic orientation unchanged unless hardware proves the owner itself is wrong.
