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
| [`MAP1_NATIVE_GAMEPLAY_HUD.md`](MAP1_NATIVE_GAMEPLAY_HUD.md) | native fresh-Junction gameplay HUD painter | #91 | `7686f7fb5c93d375f51a34ec0dd0b5cb127017e3` |
| [`MAP1_NATIVE_GAMEPLAY_INPUT.md`](MAP1_NATIVE_GAMEPLAY_INPUT.md) | calibrated invisible 12-zone gameplay touch intent owner | #92 | `cdda239f1c884a7d6f6707ba1c30a0a0a3603923` |
| [`MAP1_NATIVE_GAMEPLAY_TURN.md`](MAP1_NATIVE_GAMEPLAY_TURN.md) | native TURN_LEFT/TURN_RIGHT + cardinal render round-trip | #93 | `89f9d5f3feaa40f2e2a0c6e9506d1d8efaf5eeb6` |

Older archives remain available in Git history; `PORTING_STATUS.md` is the preferred recovery entry point.

## Latest merged boundary

PR #93 merged the first real native gameplay action family: `TURN_LEFT` / `TURN_RIGHT`.

```text
main=89f9d5f3feaa40f2e2a0c6e9506d1d8efaf5eeb6
EspNativeGameplayTurnState=24 B
EspNativeGameplayDispatchResult=12 B
TURN_LEFT=+64
TURN_RIGHT=-64
shapeData=NULL
mediaTexels=NULL
```

Merged evidence: [`MAP1_NATIVE_GAMEPLAY_TURN.md`](MAP1_NATIVE_GAMEPLAY_TURN.md).

## Current candidate milestone

[`MAP1_NATIVE_GAMEPLAY_MOVE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_MOVE_COLLISION.md) records native cardinal translation plus collision:

```text
branch=agent/esp32-native-move-collision
base=89f9d5f3feaa40f2e2a0c6e9506d1d8efaf5eeb6
hardware-tested implementation SHA=becaa1ec5bdd68311fa2e1d626fc238d1a706779
status=REAL-CYD HARDWARE PASS
merge-ready=yes after docs-only audit
```

### Movement ownership

Permanent native collision / dispatch boundary:

```text
EspNativeGameplayCollisionResult=16 B
EspNativeGameplayMoveResult=24 B
EspPlayerViewState=44 B
FORWARD/BACK/STRAFE_LEFT/STRAFE_RIGHT = one 64-unit cardinal step
```

Movement derives its vector from the live gameplay orientation owner, not from hard-coded Junction North. A successful move mutates only settled native X/Y and renders on a later service.

### Collision semantics

Fresh Junction neighbor probe:

```text
FORWARD      tile 943->911 CLEAR
BACK         tile 943->975 CLEAR
STRAFE_LEFT  tile 943->942 WALL
STRAFE_RIGHT tile 943->944 WALL
```

Collision uses compact native tile flags and linked sprite/entity topology. Dynamic opened lines remain deliberately fail-closed until their relinking semantics have a dedicated milestone.

The final hardware run also proved a true entity blocker:

```text
FORWARD tile 911->910
collision=ENTITY
blocker=24
type=7
frame exact / no movement
```

### TURN + MOVE composition

The final real-CYD run composed movement and rotation at non-spawn tile centers:

```text
FORWARD 943->911
TURN_LEFT at pos 992,1824
blocked FORWARD by ENTITY
STRAFE_RIGHT 911->879
TURN_RIGHT at pos 992,1760
FORWARD 879->847
TURN_RIGHT at pos 992,1696
FORWARD 847->848
```

No probe failure followed. `SELECT` taps in the same run remained feedback-only/deferred.

Hardware side-effect witnesses:

```text
heap8=66708->66708
largest8=29684->29684
stackHighWater=172
legacyStable=yes
residentStable=yes
Game_advanceTurn=no
Game_executeTile=no
facingRefresh=deferred
```

### Native wall-cache fix

The final tested SHA includes a renderer correction required by moved-position views. Native wall cache slots contain one bounded 2048-byte packed texture, so wall sampling now uses local texel coordinates rather than reconstructing the legacy map-wide `mediaTexels` coordinate and subtracting it again.

This removes a 32-bit overflow path for newly visible textures and aligns the renderer with the permanent architecture:

```text
sourceTexelOffset -> PAK range + cache identity only
raster texel coordinate -> local 0..4095
mediaTexels -> NULL permanently
```

## Current performance status

Movement and TURN are functionally correct on the real CYD but visibly slow. This is now an explicit renderer hot-path debt, not a gameplay correctness issue.

A successful action currently performs roughly:

```text
feedback full present
 -> feedback restore full present
 -> transient bounded cache rebuild / repeated PAK reads
 -> complete world + sprite/glow recomposition
 -> temporary 12.8 KiB HUD save/restore bridge
 -> final full present
```

Observed full physical presents remain about 34 ms; neon feedback hold is 120 ms. Do not start by micro-optimizing `PlatformVideo_present()`.

The preferred bounded performance frontier is:

```text
persistent bounded wall/plane caches
+ viewport-only gameplay world redraw
+ remove temporary HUD save bridge
+ preserve exact frame canons
+ keep redraw on demand
```

This work should remain separate from new gameplay semantics.

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
 -> native cardinal movement + collision          [hardware-proven candidate]
 -> native gameplay render hot path               [preferred NEXT]
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
legacy Game.entities == 0
legacy Game.monsters == 0
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
becaa1ec5bdd68311fa2e1d626fc238d1a706779
```

Any commit after that SHA must be documentation-only or the hardware PASS is invalidated.

## Next bounded milestone after merge

After merge, recover the exact new `main` SHA and reread `PORTING_STATUS.md`, this file and [`MAP1_NATIVE_GAMEPLAY_MOVE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_MOVE_COLLISION.md).

Preferred next milestone: native gameplay renderer hot-path ownership. Preserve current MOVE/TURN behavior and frame canons while making caches/buffers permanent and bounded and making world redraw viewport-only. Do not mix this with `PlatformVideo_present()` micro-optimization or new gameplay behavior.

If behavior is prioritized instead, recover post-move `Game_eventFlagsForMovement`, tile-event and turn-advance semantics from legacy and introduce them as a separate fail-closed family.