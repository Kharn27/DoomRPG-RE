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

[`MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md) records the diagnosis and correction of the Junction arrival-door bug.

```text
branch=agent/esp32-native-gameplay-door-view-probe
base=2aae0676528ab00c3494d142d8b35c22b7685dce
hardware-tested implementation SHA=5c01d91f9c6320460b2ecaf033f68a88bde80dfd
status=REAL-CYD HARDWARE PASS
merge-ready=yes after docs-only closeout audit
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

Closed arrival-door line `35` is a real legacy collision entity:

```text
texture=7
entity-def lookup=305+7=312
entity type=0
trace mask 0xf287 includes type 0
line midpoint/link tile=975
```

The player therefore ended up exactly on the door line. The gameplay camera's 16-unit offset then placed the wall extremely close to the camera, producing the huge door image. Renderer semantics were not wrong; collision had supplied an illegal pose.

### Permanent correction

Bounded owner:

```text
EspEntityDefTypeCatalog
817 B BSS
source=/entities.db from /DoomRPG-ESP32.pak
heap=0
runtime ZIP=no
stores eType only
```

Native cardinal collision now traces closed line-derived entities using recovered legacy midpoint/nudge placement plus trace mask `0xf287`. It does not instantiate `Entity_t`, legacy `entityDb`, or other pointer-heavy world state.

Dynamic opened-line relinking remains fail-closed.

### Final real-CYD proof

Cleaned production firmware `5c01d91f9c6320460b2ecaf033f68a88bde80dfd` produced:

```text
[MOVEPROBE] NEIGHBOR action=FORWARD ... tile=943->911 ... status=CLEAR
[LINECOLLISION] BLOCK source=943 dest=975 line=35 texture=7 flags=00000505 type=0 defTile=312
[MOVEPROBE] NEIGHBOR action=BACK ... tile=943->975 ... status=ENTITY ... type=0
[MOVEPROBE] NEIGHBOR action=STRAFE_LEFT ... status=WALL
[MOVEPROBE] NEIGHBOR action=STRAFE_RIGHT ... status=WALL
```

Interactive BACK remained exact:

```text
[MOVE] BLOCKED ... tile=943->975 collision=ENTITY blocker=65535 type=0 frame=ba3e5182 exact=yes heap=38928->38928 largest=29684->29684
```

The user confirms the physical CYD behaves correctly.

### PR #98 production cleanup also proven

Code review correctly identified that the temporary door-view witness still ran after every successful world render. The branch removed:

```text
EspNativeDoorViewProbe_log()
second diagnostic EspNativeBspVisibility_build()
DOORVIEW success logging
TURNFRAME SPRITES success logging
esp_native_door_view_probe.c/.h
```

Failure-only TURNFRAME diagnostics remain.

The cleanup retest proves:

```text
no [DOORVIEW] output
no [TURNFRAME] ... fail=
no Guru Meditation / reboot
heap8=38928 stable
largest8=29684 stable
```

Four successful TURN renders exercised the cleaned shared gameplay renderer and returned exactly to canonical North:

```text
N ba3e5182 / 9206eb24 / 6c2aa46f
W 23ee0954 / de06a408 / 9281a6d1
S da1c4297 / 582c2ad8 / a78d0f96
E 8cfdfe34 / 17c48c15 / 1d908304
N round-trip exact
```

The final North render retained:

```text
tempHud=0 B
routeNoPresent=1
finalPresent=1
legacyStable=yes
residentStable=yes
stackHighWater=860
```

The supplied retest excerpt contains the intentional blocked-door MOVE rather than a successful CLEAR MOVE; do not invent a missing CLEAR-MOVE fingerprint.

## Historical collision erratum

[`MAP1_NATIVE_GAMEPLAY_MOVE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_MOVE_COLLISION.md) records the genuine earlier hardware result `BACK 943->975 CLEAR`. That is historical evidence of the missing line-derived entity family, not the current collision contract.

Current truth is defined by `PORTING_STATUS.md` and [`MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md).

Likewise, the “start-door renderer/view anomaly” section in [`MAP1_NATIVE_GAMEPLAY_LARGE_RANGE_CACHE.md`](MAP1_NATIVE_GAMEPLAY_LARGE_RANGE_CACHE.md) is resolved by this milestone as a collision-model defect. The large-cache/legacy-wall-guard evidence remains valid.

## Superseded branch

The older unmerged branch:

```text
agent/esp32-native-door-view-witness
tip=e04195e60a0499a4da3dc189eef98446d074fd92
```

is two commits above the same base and changes only:

```text
ESP32/platformio.ini                  +6 lines
ESP32/src/native_door_view_witness.c  +230 lines
```

It is an obsolete wrapper-based diagnostic and contains no permanent collision correction. No cherry-pick is required.

Disposition: **safe to abandon/delete**.

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
 -> closed line-entity collision                  [hardware-proven]
 -> production renderer diagnostic cleanup        [hardware-proven]
 -> persistent bounded render caches              [merged]
 -> dynamic opened-line relinking                 [later]
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
persistentRenderResourceOwner=yes
smallExactRangeCache=yes
largeExact2048RangeCache=yes
legacyWallGuard=yes
static wall collision=yes
compact linked sprite-entity collision=yes
closed line-entity collision=yes
spawn BACK blocked=yes
dynamic opened-line collision=fail-closed
TURN canonical round-trip=exact
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

Final hardware-tested firmware SHA:

```text
5c01d91f9c6320460b2ecaf033f68a88bde80dfd
```

Status:

```text
REAL-CYD HARDWARE PASS
MERGE-READY after docs-only closeout audit
```

All later commits on the candidate branch must remain documentation-only. After merge, recover the exact new `main` SHA and reread `PORTING_STATUS.md`, this file, and [`MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md) before branching again.
