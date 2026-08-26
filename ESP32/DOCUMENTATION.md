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

Older archives remain available in Git history; `PORTING_STATUS.md` is the preferred recovery entry point.

## Latest merged boundary

PR #94 merged cardinal native translation and compact collision:

```text
main=b5a4426eb0df1ef1506893d4bc08b5538543a7b3
EspNativeGameplayCollisionResult=16 B
EspNativeGameplayMoveResult=24 B
EspPlayerViewState=44 B
FORWARD/BACK/STRAFE_LEFT/STRAFE_RIGHT = one cardinal 64-unit step
shapeData=NULL
mediaTexels=NULL
```

Merged evidence: [`MAP1_NATIVE_GAMEPLAY_MOVE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_MOVE_COLLISION.md).

## Current candidate milestone

[`MAP1_NATIVE_GAMEPLAY_RENDER_HOTPATH.md`](MAP1_NATIVE_GAMEPLAY_RENDER_HOTPATH.md) records the gameplay-only renderer viewport path:

```text
branch=agent/esp32-native-gameplay-render-hotpath
base=b5a4426eb0df1ef1506893d4bc08b5538543a7b3
hardware-tested implementation SHA=a07455e34eadbacca7d23fb068ba4308f0b7f80a
status=REAL-CYD HARDWARE PASS
merge-ready=yes after docs-only audit
```

### Hot-path ownership

Permanent renderer boundary:

```text
world viewport=160x80 @0,20
EspNativeGameplayFrameStats=104 B
world route whole-frame clear=no
world route physical present=no
temporary HUD save=0 B
HUD bands preserved in place=yes
final complete-frame present=exactly one
```

The historical first-frame renderer now exposes `EspNativeFirstFrame_renderGameplayViewport()`, which reuses the compact BSP/wall/plane path without clearing pixels outside the gameplay viewport, presenting, or mutating the historical first-frame owner.

### Final wrapper correction

The real-CYD run first exposed a regression where walls remained visible but sprites disappeared. The gameplay-only route left the historical first-frame owner ready, so a wrapper guard based on `!EspNativeFirstFrame_isReady()` stopped injecting native planes and the compositor failed closed before sprites.

Final tested correction:

```text
a07455e34eadbacca7d23fb068ba4308f0b7f80a
fix(esp32): activate native render wrappers for gameplay viewport
```

The wrappers now identify the actual compact native world context instead of using first-frame lifecycle state. Plane injection and `tmpLine` preservation therefore work for both boot and gameplay recomposition.

### Canonical real-CYD proof

```text
frame=ba3e5182
viewport=9206eb24
HUD=6c2aa46f
tempHud=0
routeNoPresent=1
finalPresent=1
heap8=66452->66452
largest8=29684->29684
exact=yes
```

North gameplay recomposition remained bit-exact after removing the 12.8 KiB HUD bridge and intermediate world presentation.

MOVE and TURN also remained live on the same route. The hardware run included:

```text
TURN_RIGHT angle64->0 frame ba3e5182->8cfdfe34
TURN_LEFT  angle0->64 frame 8cfdfe34->ba3e5182 exact round-trip
FORWARD tile943->911 frame ba3e5182->66da9d16
FORWARD tile911->879 frame 66da9d16->fc7a5142
FORWARD tile879->847 frame fc7a5142->3625f7a7
```

No new gameplay side effects were enabled:

```text
legacyStable=yes
residentStable=yes
Game_advanceTurn=no
Game_executeTile=no
facingRefresh=deferred
```

## Current performance truth

The user reports **no notable fluidity improvement versus `main`**. This is consistent with the new hardware timings.

Canonical North hot-path:

```text
world   = 1261184 us
sprites = 1572941 us
HUD     =  387161 us
present =   34930 us
total   = 3265801 us
```

Approximate share:

```text
world   ~38.6%
sprites ~48.2%
HUD     ~11.9%
present  ~1.1%
```

The milestone successfully removed one redundant ~34 ms world presentation and a 12.8 KiB HUD save/restore, but the full heavy frame still costs ~3.27 s. `PlatformVideo_present()` is therefore not the next meaningful optimization target.

Other hardware samples:

```text
TURN N->E total=1835575 us
TURN E->N total=3265560 us
MOVE 943->911 total=3038743 us
MOVE 911->879 total=2952445 us
MOVE 879->847 total=2916327 us
```

### Measured remaining debt

The current world phase still opens `/DoomRPG-ESP32.pak`, validates/scans its complete disk index, performs disk-backed entry searches, rebuilds all resolved wall texture descriptors and closes the pack every recomposition.

The sprite phase then opens/validates the PAK again and reloads sprite frames one at a time. Canonical North performs:

```text
21 base frame loads
7 glow frame loads
172 PAK reads
9 unique base logical sprite IDs
```

The compass phase independently reopens the PAK and `k.bmp` / `o.bmp` / `a.bmp`, performs 63 PAK reads and takes about 387 ms.

This repeated bounded storage work is legal but is now the dominant measured performance debt.

## Preferred next performance frontier

After merge, use a separate bounded milestone for **persistent native render-resource/cache ownership**:

```text
1. retain validated render-source / PAK-entry metadata
2. small bounded sprite-frame cache keyed by logical/actual frame
3. bounded resident compass/HUD render resources
4. evaluate bounded persistent wall/plane texel cache with explicit RAM budget
```

Keep `/DoomRPG-ESP32.pak` as backing store. Do not create a map-wide texel pool or revive runtime ZIP access.

Every cache milestone must preserve exact framebuffer canons, stable heap, explicit bounded ownership/eviction and fail-closed behavior.

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
 -> native viewport-only gameplay recomposition   [hardware-proven candidate]
 -> bounded persistent render caches              [preferred NEXT]
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
a07455e34eadbacca7d23fb068ba4308f0b7f80a
```

Any code commit after that SHA invalidates the hardware PASS. Closeout commits after it must be documentation-only.

## Next bounded milestone after merge

After merge, recover the exact new `main` SHA and reread `PORTING_STATUS.md`, this file and [`MAP1_NATIVE_GAMEPLAY_RENDER_HOTPATH.md`](MAP1_NATIVE_GAMEPLAY_RENDER_HOTPATH.md).

Preferred next milestone: bounded persistent native render-resource/cache ownership, starting with repeated PAK metadata/source setup and duplicate sprite/HUD reads. Do not mix this with new gameplay behavior or `PlatformVideo_present()` micro-optimization.

If behavior is prioritized instead, recover post-move `Game_eventFlagsForMovement`, tile-event and turn-advance semantics from legacy as a separate fail-closed family.
