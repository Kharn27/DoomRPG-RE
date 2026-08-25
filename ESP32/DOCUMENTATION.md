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

Older archives remain available in Git history; `PORTING_STATUS.md` is the preferred recovery entry point.

## Latest merged boundary

PR #92 merged the permanent gameplay touch semantic owner and invisible 12-zone CYD layout.

```text
main=cdda239f1c884a7d6f6707ba1c30a0a0a3603923
EspNativeGameplayInputState=12 B
EspNativeGameplayTouchHit=6 B
shapeData=NULL
mediaTexels=NULL
```

Merged evidence: [`MAP1_NATIVE_GAMEPLAY_INPUT.md`](MAP1_NATIVE_GAMEPLAY_INPUT.md).

## Current candidate milestone

[`MAP1_NATIVE_GAMEPLAY_TURN.md`](MAP1_NATIVE_GAMEPLAY_TURN.md) records the first real native gameplay action family:

```text
branch=agent/esp32-native-turn-dispatch
base=cdda239f1c884a7d6f6707ba1c30a0a0a3603923
hardware-tested implementation SHA=66ba643e7650f51d0022cd56e007242902d76c77
status=REAL-CYD HARDWARE PASS
merge-ready=yes after docs-only audit
```

### TURN ownership

Permanent native dispatcher:

```text
EspNativeGameplayTurnState=24 B
EspNativeGameplayDispatchResult=12 B
EspPlayerViewState=44 B
TURN_LEFT=+64
TURN_RIGHT=-64
other recognized actions=DEFERRED
```

The runtime owns cardinal vectors independently from the older fresh-map finishRotation owner. Prepare/commit is stale-checked and rollback-capable.

### Input/render scheduling

The proven input probe remains the sole touch callback owner. TURN execution is deliberately deferred out of the callback:

```text
tap
 -> semantic intent
 -> 120 ms yellow neon feedback
 -> exact dynamic frame restore
 -> queue TURN
 -> return lifecycle
 -> commit orientation
 -> render on later service
```

No renderer is entered from the touch callback.

### Single-present TURN compositor

The current bridge reuses the native walls/planes + sprite/glow renderer, suppresses the historical world-only intermediate physical present, restores the existing HUD bands from one bounded temporary 12.8 KiB buffer, repaints only the true compass dirty rectangle, then presents one final complete frame.

```text
EspNativeGameplayFrameStats=84 B
temporaryHudBytes=12800
intermediatePresentSuppressed=1
finalPresent=1
```

The fixed-North sprite milestone required visible mode7/glow witnesses. Runtime cardinal views instead use complete candidate accounting, so a view with zero actually drawn sprites/glows is valid when all admitted candidates are culled/accounted and no unsupported/deferred dependency exists.

### Real-CYD 360-degree proof

The hardware-tested SHA executed four consecutive `TURN_RIGHT` actions:

```text
N / 64  frame=ba3e5182 viewport=9206eb24 HUD=6c2aa46f
E / 0   frame=8cfdfe34 viewport=17c48c15 HUD=1d908304
S / 192 frame=da1c4297 viewport=582c2ad8 HUD=a78d0f96
W / 128 frame=23ee0954 viewport=de06a408 HUD=9281a6d1
N / 64  frame=ba3e5182 viewport=9206eb24 HUD=6c2aa46f roundTrip=exact
```

Final North world phase also recovered the established walls/planes viewport `032ffaed` before sprites/glows restored the complete viewport `9206eb24`.

Hardware resource/side-effect witnesses:

```text
heap8=67284->67284
largest8=34804->34804
stackHighWater=172
probe execScratch=464 B
legacyStable=yes
residentStable=yes
Game_advanceTurn=no
Game_executeTile=no
facingRefresh=deferred
```

The real CYD visibly rotated through all four orientations.

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
 -> native TURN_LEFT/TURN_RIGHT                   [hardware-proven candidate]
 -> movement/collision by bounded family          [NEXT candidate]
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
TURN_LEFT=yes
TURN_RIGHT=yes
360-degree roundTrip=exact
initialSavePersistencePending=yes
legacy Game.entities=0
legacy Game.monsters=0
Game_advanceTurn=no
Game_executeTile=no
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

Do not prematurely optimize `PlatformVideo_present()`.

## Merge recommendation

```text
MERGE-READY after docs-only audit
```

Hardware-tested code SHA:

```text
66ba643e7650f51d0022cd56e007242902d76c77
```

Any commit after that SHA must be documentation-only or the hardware PASS is invalidated.

## Next bounded milestone after merge

After merge, recover the exact new `main` SHA and reread `PORTING_STATUS.md`, this file and [`MAP1_NATIVE_GAMEPLAY_TURN.md`](MAP1_NATIVE_GAMEPLAY_TURN.md).

Preferred next gameplay milestone: cardinal translation (`FORWARD/BACK/STRAFE`) with collision semantics, while keeping actual turn advancement, tile-event execution and entities/monsters fail-closed.

A smaller renderer-only alternative is the first-person pistol overlay (fresh Junction weapon logical sprite 242). Do not combine both in one milestone.