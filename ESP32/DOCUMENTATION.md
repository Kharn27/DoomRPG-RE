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
| [`MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md) | recover closed line-derived gameplay collision | #98 | `3b17a400c35338e434fab16ae0c2a3a63ab47e3e` |

Older archives remain available in Git history; `PORTING_STATUS.md` is the preferred recovery entry point.

## Latest merged boundary

PR #98 is the current merged hardware baseline:

```text
main=3b17a400c35338e434fab16ae0c2a3a63ab47e3e
closed line-derived collision=yes
spawn BACK 943->975 blocked by line 35
line entity type=0 / defTile=312
shapeData=NULL
mediaTexels=NULL
```

Merged evidence: [`MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md).

## Current candidate milestone

[`MAP1_NATIVE_GAMEPLAY_DYNAMIC_LINE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_DYNAMIC_LINE_COLLISION.md) records the permanent per-line dynamic collision topology.

```text
branch=agent/esp32-native-gameplay-dynamic-line-collision
base=3b17a400c35338e434fab16ae0c2a3a63ab47e3e
hardware-tested implementation SHA=52ddbf979e33f99be27c8344eb4e0572ac4d0547
status=REAL-CYD HARDWARE PASS
merge-ready=yes after docs-only closeout audit
```

### Behavior recovered

Legacy door collision is not a separate geometry pass: opening a line unlinks its line-derived entity; closing it links that entity again. Native collision now consumes `EspMapLineState` directly to reproduce that topology:

```text
closed -> participates in collision
open   -> skipped
closed -> participates again
```

No legacy entity world is created.

### Strict activation witness

Canonical Junction line `35` is temporarily toggled only during activation:

```text
CLOSED -> BLOCKED_ENTITY
OPEN   -> CLEAR
CLOSE  -> exact original BLOCKED_ENTITY result
```

The witness then restores the exact baseline before interactive gameplay begins.

### First-hardware probe correction

Implementation `429a86d...` incorrectly required the resident snapshot to remain byte-identical while intentionally changing `lineStateFNV1a`. The first real-CYD run correctly failed the witness and blocked native gameplay activation.

Fix `52ddbf9...` permits exactly that one resident field to change while OPEN and requires full exactness again after CLOSE.

Post-fix physical-CYD logs show repeated successful MOVE and TURN execution with stable memory and side-effect guards, proving the activation witness completed and gameplay remained live:

```text
heap8=38928 stable
largest8=29684 stable
stackHighWater=860
legacyStable=yes
residentStable=yes
turnAdvance=no
tileDispatch=no
```

The supplied post-fix excerpt begins after activation, so the exact OPEN-state line FNV is intentionally not recorded here.

## Why there is still no door the user can open

This milestone is collision plumbing, not SELECT gameplay.

Current touch SELECT remains:

```text
action=SELECT
consumedBy=probe
dispatch=observer
gameplay=no
```

The line-35 witness is invisible and fully rolled back before gameplay. Also, canonical line 35 has flags `0x505`, including the `LOCKED` bit `0x400`.

Legacy `Game_performDoorEvent()` refuses a locked line before toggling the open bit. Therefore a future native SELECT path must preserve key/lock semantics instead of directly toggling colored doors.

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
 -> closed line-derived collision                 [hardware-proven]
 -> dynamic per-line open/close collision         [hardware-proven]
 -> SELECT front-tile/interaction routing         [next]
 -> lock/key-aware native interaction             [later]
 -> real EV_OPENLINE/CLOSELINE gameplay trigger   [later]
 -> door visual animation consumer                [later]
 -> post-move turn/tile events                    [later]
 -> entity/monster gameplay                       [later]
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
glowCompanions=yes
nativeHud=yes
nativeInput=yes
nativeTurnDispatch=yes
nativeMovementDispatch=yes
nativeGameplayViewportHotPath=yes
static wall collision=yes
compact linked sprite-entity collision=yes
closed line-derived collision=yes
dynamic opened-line collision=yes
player-operated door=no
SELECT gameplay=no
Game_advanceTurn=no
Game_executeTile=no
facingRefresh=deferred
shapeData=NULL
mediaTexels=NULL
```

## Next bounded milestone

Preferred next boundary:

```text
SELECT intent
 -> current cardinal facing
 -> identify front tile / front line / event descriptor
 -> recover legacy interaction flag 1280 routing
 -> emit compact native interaction result
 -> LOCKED remains fail-closed
 -> no invented keys
 -> no sound/animation yet
 -> no broad legacy Game_executeEvent
```

This gives a hardware-testable SELECT path without pretending that Junction's colored locked doors are already openable.

## Merge boundary

Final hardware-tested implementation SHA:

```text
52ddbf979e33f99be27c8344eb4e0572ac4d0547
```

Status:

```text
REAL-CYD HARDWARE PASS
MERGE-READY after docs-only closeout audit
```

All later commits on the candidate branch must remain documentation-only. After merge, recover the exact new `main` SHA and reread `PORTING_STATUS.md`, this file, and [`MAP1_NATIVE_GAMEPLAY_DYNAMIC_LINE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_DYNAMIC_LINE_COLLISION.md) before branching again.
