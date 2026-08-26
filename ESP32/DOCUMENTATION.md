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
| [`MAP1_NATIVE_GAMEPLAY_INPUT.md`](MAP1_NATIVE_GAMEPLAY_INPUT.md) | calibrated invisible gameplay touch intent owner | #92 | `cdda239f1c884a7d6f6707ba1c30a0a0a3603923` |
| [`MAP1_NATIVE_GAMEPLAY_TURN.md`](MAP1_NATIVE_GAMEPLAY_TURN.md) | native TURN_LEFT/TURN_RIGHT + cardinal render round-trip | #93 | `89f9d5f3feaa40f2e2a0c6e9506d1d8efaf5eeb6` |
| [`MAP1_NATIVE_GAMEPLAY_MOVE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_MOVE_COLLISION.md) | native cardinal movement + initial wall/sprite-entity collision | #94 | `b5a4426eb0df1ef1506893d4bc08b5538543a7b3` |
| [`MAP1_NATIVE_GAMEPLAY_RENDER_HOTPATH.md`](MAP1_NATIVE_GAMEPLAY_RENDER_HOTPATH.md) | viewport-only gameplay recomposition | #95 | `f98a0b8e9eb4cbd38bf5678a1ce60c4989766985` |
| [`MAP1_NATIVE_GAMEPLAY_RENDER_RESOURCE_CACHE.md`](MAP1_NATIVE_GAMEPLAY_RENDER_RESOURCE_CACHE.md) | persistent bounded PAK/render-resource owner | #96 | `377fce3de5381373750a7fba29d0c83b8142c583` |
| [`MAP1_NATIVE_GAMEPLAY_LARGE_RANGE_CACHE.md`](MAP1_NATIVE_GAMEPLAY_LARGE_RANGE_CACHE.md) | shared exact 2048 B reuse + bounded legacy wall-block guard | #97 | `2aae0676528ab00c3494d142d8b35c22b7685dce` |

Older archives remain available in Git history; `PORTING_STATUS.md` is the preferred recovery entry point.

## Latest merged boundary

PR #97 is the current merged hardware baseline:

```text
main=2aae0676528ab00c3494d142d8b35c22b7685dce
small exact resident ranges <=1024 B
shared exact 2048 B tail cache
three retained 2048 B slots on canonical Junction North
canonical warm physical reads=19
bounded cross-block legacy wall guard=16 B BSS
shapeData=NULL
mediaTexels=NULL
```

Merged evidence: [`MAP1_NATIVE_GAMEPLAY_LARGE_RANGE_CACHE.md`](MAP1_NATIVE_GAMEPLAY_LARGE_RANGE_CACHE.md).

## Current candidate milestone

[`MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md) records the diagnosis and correction of the apparent Junction arrival-door rendering bug.

```text
branch=agent/esp32-native-gameplay-door-view-probe
base=2aae0676528ab00c3494d142d8b35c22b7685dce
hardware-proven semantic-fix SHA=efea93977d20ba94e2fd5d6981ebce2e7916bc5b
post-review firmware SHA=5c01d91f9c6320460b2ecaf033f68a88bde80dfd
status=REAL-CYD RETEST REQUIRED
merge-ready=no
```

### Root cause

The original native MOVE collision family modeled:

```text
static WALL cells
+ compact map-sprite entity topology
```

but omitted legacy line-derived entities. At fresh Junction spawn that incorrectly allowed:

```text
BACK tile 943 -> 975
player 992,1888 -> 992,1952
```

Closed arrival-door line `35` is itself a real legacy collision entity:

```text
texture=7
entity-def lookup=305+7=312
entity type=0
trace mask 0xf287 includes type 0
line midpoint/link tile=975
```

The player therefore ended up exactly on the door line. The gameplay camera's 16-unit offset then placed the wall extremely close to the camera, producing the huge door/wall image. The renderer was not wrong; collision had supplied an illegal pose.

### Permanent correction

New bounded owner:

```text
EspEntityDefTypeCatalog
817 B BSS
source=/entities.db from /DoomRPG-ESP32.pak
heap=0
runtime ZIP=no
stores eType only
```

Native cardinal collision traces closed line-derived entities using the recovered legacy midpoint/nudge placement and trace mask. It does not instantiate `Entity_t`, a pointer database, or legacy Render line ownership.

Dynamic opened-line relinking remains fail-closed and is not silently inferred.

### Decisive real-CYD semantic proof

Hardware-tested SHA `efea939...` produced:

```text
[LINECOLLISION] BLOCK source=943 dest=975 line=35 texture=7 flags=00000505 type=0 defTile=312
[MOVE] BLOCKED ... tile=943->975 collision=ENTITY blocker=65535 type=0 frame=ba3e5182 exact=yes heap=38216->38216 largest=21492->21492
```

Correct immediate Junction movement truth:

```text
FORWARD      943->911 : CLEAR
BACK         943->975 : ENTITY / closed line 35
STRAFE_LEFT  943->942 : WALL
STRAFE_RIGHT 943->944 : WALL
```

The user confirmed on the physical classic CYD that the visual door bug is corrected.

### Renderer/compositor exonerated

Temporary diagnostics at the tested SHA established:

```text
world render succeeds
HUD bands remain exact
sprite accounting complete
unsupported sprites=0
Render scratch stable
final present succeeds
```

Do not alter cardinal orientation, BSP side tests, wall projection, or video presentation to address this now-closed symptom.

The hardware run also revalidated exact spawn TURN canons:

```text
N ba3e5182 / 9206eb24 / 6c2aa46f
E 8cfdfe34 / 17c48c15 / 1d908304
S da1c4297 / 582c2ad8 / a78d0f96
W 23ee0954 / de06a408 / 9281a6d1
N round-trip exact
```

### PR #98 production cleanup

Code review correctly identified that the completed door witness was still being executed after every successful gameplay world render. The branch now removes that temporary diagnostic from the normal hot path:

```text
c8b39ab1dde922045391f160ab447b6f974ccfbb
  remove second door/BSP traversal from EspNativeGameplayFrame_renderTurn()
  remove DOORVIEW success logs
  remove TURNFRAME SPRITES success log
  keep failure-only TURNFRAME diagnostics

1d84f58770087237020a5b3ecfbfc2bfe8fe7bde
  remove esp_native_door_view_probe.c

5c01d91f9c6320460b2ecaf033f68a88bde80dfd
  remove esp_native_door_view_probe.h
```

This is a production-path improvement, but it happened after the last real-CYD run. Therefore the current candidate must be reflashed before merge. Do not inherit the `efea939...` PASS onto `5c01d91...` by assumption.

Required final retest:

```text
normal esp32-cyd firmware
spawn BACK still blocked by line 35/type 0
one successful MOVE render
one successful TURN render
no DOORVIEW success spam
no TURNFRAME failure
no reboot/Guru Meditation
heap/resident state stable
```

## Historical collision erratum

[`MAP1_NATIVE_GAMEPLAY_MOVE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_MOVE_COLLISION.md) records the genuine earlier hardware result `BACK 943->975 CLEAR`. That record is historical evidence of the missing line-entity family, not the current collision contract.

The current truth is defined by `PORTING_STATUS.md` and [`MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md).

Likewise, the “start-door renderer/view anomaly” section in [`MAP1_NATIVE_GAMEPLAY_LARGE_RANGE_CACHE.md`](MAP1_NATIVE_GAMEPLAY_LARGE_RANGE_CACHE.md) is resolved by this milestone as a collision-model defect. The large-cache/legacy-wall-guard evidence itself remains valid and unchanged.

## Superseded branch

The older unmerged branch:

```text
agent/esp32-native-door-view-witness
tip=e04195e60a0499a4da3dc189eef98446d074fd92
```

is two commits ahead of the same `2aae067...` base. Its complete diff is only:

```text
ESP32/platformio.ini                  +6 lines
ESP32/src/native_door_view_witness.c  +230 lines
```

It is an older wrapper-based witness and contains no permanent collision correction. The later direct witness on the current candidate has also been retired after finding the root cause.

Disposition: **safe to abandon/delete; no cherry-pick required**.

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
ClosedGlowCatalogFNV=257444a5
JunctionFirstFrameFNV=8910c2ed
JunctionFirstViewportFNV=032ffaed
JunctionGlowFrameFNV=b5218f24
JunctionGlowViewportFNV=9206eb24
JunctionHudFrameFNV=ba3e5182
JunctionHudBandsFNV=6c2aa46f
JunctionHudStateFNV=4756db9c
```

## Current architecture direction

```text
original behavior/data
 -> /DoomRPG-ESP32.pak
 -> compact immutable map                         [hardware-proven]
 -> compact mutable overlays                      [hardware-proven]
 -> native event semantics by bounded family      [hardware-proven]
 -> native transition/residency                   [hardware-proven]
 -> native PLAYING service                        [hardware-proven]
 -> sparse native graphics catalog                [hardware-proven]
 -> native walls + planes                         [hardware-proven]
 -> billboards + glows                            [hardware-proven]
 -> native HUD + touch intent                     [hardware-proven]
 -> native TURN                                   [hardware-proven]
 -> native cardinal MOVE                          [hardware-proven]
 -> static WALL + sprite-entity collision         [hardware-proven]
 -> closed line-entity collision                  [hardware-proven semantics]
 -> production render hot-path cleanup            [retest pending]
 -> persistent bounded render caches              [merged]
 -> dynamic opened-line relinking                 [later]
 -> post-move turn/tile events                    [later]
 -> entity/monster gameplay                       [later]
```

## Current hardware PARK

The following state is hardware-proven at `efea939...`; the cleaned firmware must revalidate it before merge:

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
static wall collision=yes
compact linked sprite-entity collision=yes
closed line-entity collision=yes
spawn BACK blocked=yes
dynamic opened-line collision=fail-closed
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

## Merge boundary

Last hardware-tested firmware SHA:

```text
efea93977d20ba94e2fd5d6981ebce2e7916bc5b
```

Current post-review firmware candidate:

```text
5c01d91f9c6320460b2ecaf033f68a88bde80dfd
```

Because firmware changed after the tested SHA:

```text
REAL-CYD RETEST REQUIRED
MERGE-READY = NO
```

After the current firmware candidate passes, only documentation may change before merge. Then recover the exact merged `main` SHA before starting the next `agent/*` branch.
