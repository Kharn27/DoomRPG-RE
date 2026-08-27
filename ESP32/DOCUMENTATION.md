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
| [`MAP1_NATIVE_GAMEPLAY_DYNAMIC_LINE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_DYNAMIC_LINE_COLLISION.md) | per-line dynamic open/closed collision topology | #99 | `e0a250f0bfd6e5519298f942f4bed65c230c3652` |

Older archives remain available in Git history; `PORTING_STATUS.md` is the preferred recovery entry point.

## Latest merged boundary

PR #99 is the current merged hardware baseline:

```text
main=e0a250f0bfd6e5519298f942f4bed65c230c3652
closed line collision=yes
open line skipped by collision=yes
close restores collision=yes
shapeData=NULL
mediaTexels=NULL
```

Merged evidence: [`MAP1_NATIVE_GAMEPLAY_DYNAMIC_LINE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_DYNAMIC_LINE_COLLISION.md).

## Current candidate milestone

[`MAP1_NATIVE_GAMEPLAY_SELECT_FRONT_TILE.md`](MAP1_NATIVE_GAMEPLAY_SELECT_FRONT_TILE.md) records the first live SELECT front-tile observer on Junction.

```text
branch=agent/esp32-native-gameplay-select-front-tile
base=e0a250f0bfd6e5519298f942f4bed65c230c3652
hardware-tested implementation SHA=ca5560c0eb849c8a11b21eb8c117e7a8fc4c60ff
status=REAL-CYD HARDWARE PASS
merge-ready=yes after docs-only closeout audit
```

### Behavior recovered

The first legacy SELECT step is now mirrored read-only on the live native gameplay path:

```text
SELECT
 -> current dest + viewStep
 -> front tile
 -> run flags 1280 / 0x500
 -> event lookup + descriptor + mutable current state + filter provenance
 -> optional line-derived entity witness on same front tile
```

No bytecode executes and no world state changes.

Permanent ABI:

```text
EspNativeGameplaySelectResult=28 B
EspMapLineTopologyRef=16 B
persistent heap=0 B
```

### Fresh spawn hardware proof

North-facing SELECT at canonical spawn:

```text
front=992,1824
tile=911
event=59
opcode=4 / UNOWNED
decision=FLAGS_MISMATCH
frame=ba3e5182 exact=yes
heap8=38924 stable
largest8=29684 stable
legacyExact=yes
residentExact=yes
```

### Arrival door hardware proof

After two right turns, SELECT South at the arrival door proved:

```text
front=992,1952
tile=975
event=63
line=35
texture=7
flags=00000505
open=0
locked=1
linked=1
type=0
defTile=312
```

The one eligible event command is:

```text
opcode=15 / EV_OPENLINE
arg1=35
arg2=00000100
decision=ELIGIBLE
```

This establishes the exact interaction chain:

```text
SELECT -> Junction tile975 -> event63 -> EV_OPENLINE(35) -> line35 LOCKED
```

The observer correctly performs no mutation. Legacy locked-door refusal remains a separate later execution boundary.

### Important corpus correction

Do not reuse `/intro.bsp` event bounds as Junction runtime facts. A pre-test prediction that tile975 had no event was invalid because the active resident runtime is `/junction.bsp`.

Real CYD truth:

```text
Junction tile975 has event63
```

### Live interaction corpus

Physical SELECTs while walking around Junction exposed real multi-state script families:

```text
tile878 event56 commands=18: EV_DIALOG / EV_CHANGESTATE / EV_NEXTSTATE
tile845 event49 commands=14: EV_DIALOG / EV_CHANGESTATE + linked line98 type7 unlocked
tile816 event45 commands=12: EV_DIALOG / EV_CHANGESTATE
```

The user reached visible scientist/computer/soldier interaction targets while these logs were captured. The observer does not yet map those physical labels to permanent entity identities; it only records the proven front-tile/script provenance.

### Regression proof

Normal MOVE/TURN remained live throughout the same physical session:

```text
canonical North round-trip frame=ba3e5182 exact
MOVE 943->911 OK
MOVE 911->879 OK
MOVE 847->846 OK
MOVE 846->847 OK
MOVE 847->815 OK
heap8=38924 stable
largest8=29684 stable
stackHighWater=860
legacyStable=yes
residentStable=yes
```

No SELECT probe failure, Guru Meditation, reboot or memory drift was observed.

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
 -> SELECT front-tile/event/line provenance       [hardware-proven]
 -> lock-aware EV_OPENLINE/CLOSELINE execution    [next]
 -> door visual animation consumer                [later]
 -> live dialogue/UI SELECT execution             [later]
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
SELECT front-tile observer=yes
SELECT runFlags=0x500
event/filter provenance=yes
front line provenance=yes
arrival door event63/EV_OPENLINE35/LOCKED=yes
player-operated door=no
Game_advanceTurn=no
postMoveTileDispatch=no
facingRefresh=deferred
shapeData=NULL
mediaTexels=NULL
```

## Next bounded milestone

Preferred next boundary:

```text
SELECT eligible command
 -> support only opcode 15 EV_OPENLINE and opcode 16 EV_CLOSELINE
 -> resolve line target
 -> apply recovered LOCKED guard before mutation
 -> locked line: refuse, exact no-op
 -> unlocked line: mutate compact EspMapLineState only
 -> existing native collision immediately consumes new open state
 -> unrelated opcodes remain fail-closed
```

Keep sound, visual animation, broad entity/combat fallback, unrelated script opcodes and fake key ownership out of that milestone.

For the positive open/close witness, discover and use a real unlocked Junction line/event; do not bypass the lock on arrival line35.

## Merge boundary

Final hardware-tested implementation SHA:

```text
ca5560c0eb849c8a11b21eb8c117e7a8fc4c60ff
```

Status:

```text
REAL-CYD HARDWARE PASS
MERGE-READY after docs-only closeout audit
```

All later commits on the candidate branch must remain documentation-only. After merge, recover the exact new `main` SHA and reread `PORTING_STATUS.md`, this file, and [`MAP1_NATIVE_GAMEPLAY_SELECT_FRONT_TILE.md`](MAP1_NATIVE_GAMEPLAY_SELECT_FRONT_TILE.md) before branching again.
