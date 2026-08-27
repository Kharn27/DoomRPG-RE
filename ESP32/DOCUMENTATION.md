# ESP32 documentation map

This file indexes the current classic-CYD Doom RPG port documentation.

## Source of truth

- [`README.md`](README.md): stable build/flash guide.
- [`PORTING_STATUS.md`](PORTING_STATUS.md): authoritative recovery point and current hardware PARK.
- Milestone archives: implementation contracts plus real-CYD evidence.

If chat history and repository state disagree, current GitHub `main` + `PORTING_STATUS.md` + this file + the latest relevant milestone archive win.

## Latest merged boundary

```text
PR   = #100 — native gameplay SELECT front-tile
main = be4a9a666245663da7866a8aa0aa40b98339d076
status = MERGED
```

Merged evidence: [`MAP1_NATIVE_GAMEPLAY_SELECT_FRONT_TILE.md`](MAP1_NATIVE_GAMEPLAY_SELECT_FRONT_TILE.md).

## Current candidate milestone

[`MAP1_NATIVE_ENTRANCE_STARTUP_ROUTE.md`](MAP1_NATIVE_ENTRANCE_STARTUP_ROUTE.md) records the startup-route correction that stops the production boot before the historical automatic Junction handoff.

```text
branch = agent/esp32-native-entrance-startup-route
base = be4a9a666245663da7866a8aa0aa40b98339d076
hardware-tested implementation SHA = 87d7923b42eda0f36a1c7daded33ca0c4f5a4958
status = REAL-CYD HARDWARE PASS
merge-ready = YES
post-test commits = docs-only
```

### Startup semantics recovered

```text
cinematic intro
 -> startupMap=1
 -> MAP_INTRO=1
 -> /intro.bsp
 -> Entrance
```

`/intro.bsp` is the first post-cinematic gameplay BSP. `/level01.bsp` is a later map resource and is not the correct new-game startup target.

### Bug fixed

Historical validation scaffolding continued automatically:

```text
Entrance
 -> target preflight
 -> resident handoff
 -> committed transition
 -> Junction
```

The corrected production route stops after the read-only target preflight:

```text
Entrance resident exact
 -> PARK
```

`ResidentHandoff` and `CommittedTransition` remain historical executable evidence but are no longer serviced automatically during startup.

### Hardware proof

Real classic CYD:

```text
file=/intro.bsp
name=Entrance
startupMap=1
sourceBytes=21823
crc32=623f34e4
runtimeArena=14095
runtimeFNV=c3882516
snapshotFNV=b3811f3d
payload=17891
spawnHeader=904
direction=64
```

Route proof:

```text
entranceResident=yes
targetPreflightOnly=yes
residentHandoff=no
committedTransition=no
junctionResident=no
junctionGameplay=no
spawnDeferred=yes
firstFrameDeferred=yes
shapeData=NULL
mediaTexels=NULL
legacy entities=0
legacy monsters=0
```

Memory/frame witness:

```text
frameFNV=faa62417 exact
heap8=64464 stable
largest8=34804 stable
observer allocation=none
```

## Recent merged gameplay/render milestones

| Archive | Purpose | PR | Merged `main` |
| --- | --- | ---: | --- |
| [`MAP1_NATIVE_PLAYING_SERVICE.md`](MAP1_NATIVE_PLAYING_SERVICE.md) | first permanent native PLAYING service | #86 | `bf1275037fd22504077f6ff2bbf57e14721edf0a` |
| [`MAP1_NATIVE_GRAPHICS_CATALOG.md`](MAP1_NATIVE_GRAPHICS_CATALOG.md) | compact PAK-backed graphics catalog | #87 | `91a17414859fa12a0553e5b011956b6f95165780` |
| [`MAP1_NATIVE_FIRST_JUNCTION_FRAME.md`](MAP1_NATIVE_FIRST_JUNCTION_FRAME.md) | first Junction walls+planes frame | #88 | `d8da51e5a3b9700d1806110f56f553a422d7d182` |
| [`MAP1_NATIVE_JUNCTION_SPRITES.md`](MAP1_NATIVE_JUNCTION_SPRITES.md) | BSP-visible native Junction billboards | #89 | `674b45bbd115cd8f9202f2ce2d7132550c3bb75e` |
| [`MAP1_NATIVE_JUNCTION_GLOWS.md`](MAP1_NATIVE_JUNCTION_GLOWS.md) | additive Junction glow companions | #90 | `30351fd0a867e18dad171962b00d70923b4d173f` |
| [`MAP1_NATIVE_GAMEPLAY_HUD.md`](MAP1_NATIVE_GAMEPLAY_HUD.md) | native gameplay HUD | #91 | `7686f7fb5c93d375f51a34ec0dd0b5cb127017e3` |
| [`MAP1_NATIVE_GAMEPLAY_INPUT.md`](MAP1_NATIVE_GAMEPLAY_INPUT.md) | calibrated touch intent | #92 | `cdda239f1c884a7d6f6707ba1c30a0a0a3603923` |
| [`MAP1_NATIVE_GAMEPLAY_TURN.md`](MAP1_NATIVE_GAMEPLAY_TURN.md) | native cardinal TURN | #93 | `89f9d5f3feaa40f2e2a0c6e9506d1d8efaf5eeb6` |
| [`MAP1_NATIVE_GAMEPLAY_MOVE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_MOVE_COLLISION.md) | native MOVE + initial collision | #94 | `b5a4426eb0df1ef1506893d4bc08b5538543a7b3` |
| [`MAP1_NATIVE_GAMEPLAY_RENDER_HOTPATH.md`](MAP1_NATIVE_GAMEPLAY_RENDER_HOTPATH.md) | viewport-only recomposition | #95 | `f98a0b8e9eb4cbd38bf5678a1ce60c4989766985` |
| [`MAP1_NATIVE_GAMEPLAY_RENDER_RESOURCE_CACHE.md`](MAP1_NATIVE_GAMEPLAY_RENDER_RESOURCE_CACHE.md) | bounded render-resource cache | #96 | `377fce3de5381373750a7fba29d0c83b8142c583` |
| [`MAP1_NATIVE_GAMEPLAY_LARGE_RANGE_CACHE.md`](MAP1_NATIVE_GAMEPLAY_LARGE_RANGE_CACHE.md) | exact 2048 B reuse + wall guard | #97 | `2aae0676528ab00c3494d142d8b35c22b7685dce` |
| [`MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md) | closed line-derived collision | #98 | `3b17a400c35338e434fab16ae0c2a3a63ab47e3e` |
| [`MAP1_NATIVE_GAMEPLAY_DYNAMIC_LINE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_DYNAMIC_LINE_COLLISION.md) | dynamic per-line collision | #99 | `e0a250f0bfd6e5519298f942f4bed65c230c3652` |
| [`MAP1_NATIVE_GAMEPLAY_SELECT_FRONT_TILE.md`](MAP1_NATIVE_GAMEPLAY_SELECT_FRONT_TILE.md) | SELECT front-tile/event/line provenance | #100 | `be4a9a666245663da7866a8aa0aa40b98339d076` |

Older archives remain available in Git history. `PORTING_STATUS.md` is the preferred recovery entry point.

## Stable recovery canons

Entrance current startup resident:

```text
sourceBytes=21823
crc32=623f34e4
runtimeFNV=c3882516
snapshotFNV=b3811f3d
mapFNV=cd99b98e
scriptFNV=f9e3d9df
lineFNV=e5e74861
textureFNV=f1fc1875
automapFNV=669b1aa7
topologyFNV=3f321e43
spawn=904
direction=64
```

Historical Junction canons remain valid for later reuse:

```text
sourceFNV=fefaf5ca
runtimeFNV=bc432a0f
lineState baseline FNV=3658710d
fresh player=992,1888,36
angle=64
tile=943
HUD frame=ba3e5182
HUD viewport=9206eb24
HUD bands=6c2aa46f
HUD stateFNV=4756db9c
```

## Architecture direction

```text
original behavior/data
 -> /DoomRPG-ESP32.pak
 -> compact immutable map                    [hardware-proven]
 -> compact mutable overlays                 [hardware-proven]
 -> native event semantics by bounded family [hardware-proven]
 -> native gameplay/render components        [hardware-proven on Junction]
 -> correct production startup on Entrance   [hardware-proven]
 -> Entrance player/view spawn               [next]
 -> first native Entrance frame              [next]
 -> reattach HUD/input/TURN/MOVE/SELECT      [later bounded milestones]
 -> real Entrance door/computer/password play[later]
```

Permanent invariants remain:

```text
shapeData=NULL
mediaTexels=NULL
runtime ZIP forbidden for migrated paths
/DoomRPG-ESP32.pak is native backing store
legacy Game.entities=0
legacy Game.monsters=0
```

## Recommended next milestone

```text
Entrance resident
 -> BSP header spawn tile 904
 -> direction 64
 -> native player/view placement
 -> first Entrance walls/planes frame
 -> no automatic EV_CHANGEMAP
 -> no Junction resident handoff
```

Reuse map-generic permanent owners already proven on Junction. Do not fork permanent architecture into Entrance-specific duplicates.

## Merge boundary

```text
hardware-tested implementation SHA = 87d7923b42eda0f36a1c7daded33ca0c4f5a4958
REAL-CYD HARDWARE PASS
MERGE-READY = YES
```

All commits after the implementation SHA are documentation-only. After merge, recover the exact new `main` SHA before branching again.
