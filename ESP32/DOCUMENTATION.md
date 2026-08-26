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

Older archives remain available in Git history; `PORTING_STATUS.md` is the preferred recovery entry point.

## Latest merged boundary

PR #95 merged the gameplay renderer hot path:

```text
main=f98a0b8e9eb4cbd38bf5678a1ce60c4989766985
world viewport=160x80 @0,20
EspNativeGameplayFrameStats=104 B
temporary HUD save=0 B
world intermediate present=none
final present=exactly one
canonical frame=ba3e5182
canonical viewport=9206eb24
canonical HUD=6c2aa46f
shapeData=NULL
mediaTexels=NULL
```

Merged evidence: [`MAP1_NATIVE_GAMEPLAY_RENDER_HOTPATH.md`](MAP1_NATIVE_GAMEPLAY_RENDER_HOTPATH.md).

## Current candidate milestone

[`MAP1_NATIVE_GAMEPLAY_RENDER_RESOURCE_CACHE.md`](MAP1_NATIVE_GAMEPLAY_RENDER_RESOURCE_CACHE.md) records the persistent bounded PAK/render-resource owner:

```text
branch=agent/esp32-native-gameplay-render-resource-cache
base=f98a0b8e9eb4cbd38bf5678a1ce60c4989766985
hardware-tested implementation SHA=1e8c6a5f8fd1e6d01588b1c74dd4fc4e3b961e95
status=REAL-CYD HARDWARE PASS
merge-ready=yes after docs-only audit
```

### Permanent storage/cache ownership

```text
physical /DoomRPG-ESP32.pak owner = resident during native gameplay
world/sprite/HUD open-close calls = logical leases
full disk-index validation = once at resident begin
entry descriptor cache = 24 slots
small exact-range payload = 16384 B
small exact-range keys = 256
cacheable exact range <=1024 B
large world reads remain PAK-backed
runtime ZIP forbidden
shapeData=NULL
mediaTexels=NULL
```

The pack's historical non-resident lifecycle remains valid outside this explicit resident mode.

### Real-CYD RAM witness

```text
owner struct=21160 B
heap8=66372->40832
observed heap cost=25540 B
largest8 after owner=13812 B
cache used=9225/16384 B after canonical cold frame
range entries=195/256
```

This is a substantial permanent cost on the no-PSRAM CYD, so future cache growth must be justified by measured hardware gain.

### Canonical cold/warm proof

Immediate predecessor, before enabling the resident owner in the same firmware:

```text
world=1295232 us
sprites=1610031 us
HUD=400296 us
present=34935 us
total=3350141 us
frame=ba3e5182 viewport=9206eb24 HUD=6c2aa46f exact=yes
```

First resident COLD frame:

```text
physicalOpen=0 validate=0
sdReads=280 sdBytes=55541
entry=6H/9M
range=155H/195M/195S/22B
world=1044890 us
sprites=505972 us
HUD=378835 us
present=34925 us
total=1974252 us
frame=ba3e5182 viewport=9206eb24 HUD=6c2aa46f exact=yes
```

Second resident WARM frame:

```text
physicalOpen=0 validate=0
sdReads=22 sdBytes=45056
entry=15H/0M
range=350H/0M/0S/22B
world=209454 us
sprites=9604 us
HUD=1317 us
present=34908 us
total=264828 us
frame=ba3e5182 viewport=9206eb24 HUD=6c2aa46f exact=yes
heapStable=yes
```

Physical reads fell from `280` to `22`, saving `258` reads (~92.1%). The user reports the result is now clearly more playable.

The same strict repeated pose improved from `3350141 us` immediately before owner activation to `264828 us` warm, approximately `12.65x` lower total recomposition time.

### Interactive MOVE/TURN proof

The owner remained resident after the strict probe and real calibrated touch actions stayed stable at arbitrary moved positions:

```text
FORWARD 943->911 total=255050 us
FORWARD 911->879 total=272550 us
TURN_RIGHT 64->0 @ tile879 total=160497 us
TURN_LEFT 0->64 @ tile879 total=272608 us
TURN_LEFT 64->128 @ tile879 total=260054 us
TURN_RIGHT 128->64 @ tile879 total=272618 us
FORWARD 879->847 total=235073 us
FORWARD 847->815 total=290621 us
TURN_RIGHT 64->0 @ tile815 total=203124 us
```

Across the interactive sequence:

```text
heap8=40832->40832
largest8=13812->13812
tempHud=0
routeNoPresent=1
finalPresent=1
legacyStable=yes
residentStable=yes
Game_advanceTurn=no
Game_executeTile=no
facingRefresh=deferred
```

One first West-direction compass repaint measured `110734 us`; already-warm compass paths measured about `1.3 ms`. This remains a recorded hardware witness.

## Current performance truth

Warm canonical North is now:

```text
world   = 209454 us  ~79.1%
sprites =   9604 us  ~3.6%
HUD     =   1317 us  ~0.5%
present =  34908 us  ~13.2%
total   = 264828 us
```

The broad repeated PAK metadata/sprite/HUD problem is therefore no longer the current bottleneck.

The remaining warm storage traffic is exactly:

```text
22 physical reads
45056 B
22 x 2048 B
```

Those reads are the deliberate >1024 B cache-bypass class, corresponding to the remaining wall/plane texture path. This is the preferred next renderer frontier.

Because the resident owner already reserves 16 KiB payload but only 9225 B was occupied by the canonical cold frame, first investigate whether part of that already-paid capacity can service a tiny bounded 2048-byte wall/plane cache before increasing permanent RAM.

`PlatformVideo_present()` is now a visible fraction of the optimized frame, but should not be optimized before the remaining 2048-byte PAK reads are bounded and measured.

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
 -> bounded persistent render resource owner      [hardware-proven candidate]
 -> bounded remaining 2048-byte texture reuse     [preferred NEXT]
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

## Merge recommendation

```text
MERGE-READY after docs-only audit
```

Hardware-tested code SHA:

```text
1e8c6a5f8fd1e6d01588b1c74dd4fc4e3b961e95
```

Any code commit after that SHA invalidates the hardware PASS. Closeout commits after it must be documentation-only.

## Next bounded milestone after merge

After merge, recover the exact new `main` SHA and reread `PORTING_STATUS.md`, this file and [`MAP1_NATIVE_GAMEPLAY_RENDER_RESOURCE_CACHE.md`](MAP1_NATIVE_GAMEPLAY_RENDER_RESOURCE_CACHE.md).

Preferred renderer milestone: bound the remaining `22 x 2048 B` warm PAK reads with the smallest possible cache/ownership change, preferably by repartitioning already allocated resident payload rather than increasing the permanent heap footprint.

Do not mix this with new gameplay behavior.

If behavior is prioritized instead, recover post-move `Game_eventFlagsForMovement`, tile-event and turn-advance semantics from legacy as a separate fail-closed family.