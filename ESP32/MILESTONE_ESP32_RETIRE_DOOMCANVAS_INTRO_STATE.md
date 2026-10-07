# Milestone — Retire DoomCanvas intro state into a transient native owner

Status: **REAL-CYD PASS**

Hardware-tested code boundary:
`4635f711b5d2d1128a893a8d266920626df8654d`

Branch:
`agent/esp32-intro-state-owner`

## Goal

Remove the complete ST_INTRO-only ownership group from permanent
`DoomCanvas_t` without reviving desktop intro/state machinery and without
touching world/entity/render legacy mutation.

## Ownership boundary

ESP32 now owns this exact transient group in `EspNativeIntroState_t`:

```text
imgSpaceBG
imgLinesLayer
imgPlanetLayer
imgSpaceship
storyTextTime
storyAnimTime
storyText1[2]
storyText2
storyPage
storyTextPage
showTextDone
```

The native state is compile-time pinned to 96 bytes. It is allocated only when
a fresh START enters `ST_INTRO`, then destroyed by the bounded intro disposer
before native MAP_INTRO loading.

Shared Canvas state such as layout rectangles, fonts/softkeys, time, `doomRpg`
and `render` remains outside this cut.

## Layout and ABI

```text
DoomCanvas_t                 368 B -> 272 B
permanent Canvas reclaimed    96 B
total reclaimed             3468 B / 3740 B (~92.7%)
source ABI exports            11 -> 10
retired export                DoomCanvas_loadPrologueText
```

The old source export is replaced by an internal bridge begin-intro helper that
preserves the loading/music transition and enters the native transient owner.
Unsupported ownership still fails closed.

## CI

Normal `esp32-cyd` CI #1659 passed on functional commit
`d6a9f8b4426d20cbbde7fbdb289363690a447e12`:

```text
[ESP32] Desktop DoomCanvas.c retired; esp_legacy_doomcanvas_bridge.c owns 10 source ABI exports
RAM:   45056 B
Flash: 773901 B
esp32-cyd SUCCESS
```

Merged-main comparison at the start of this milestone was 45432 B RAM /
772925 B Flash. The functional change therefore releases 376 B static RAM and
adds 976 B linked Flash.

Hardware-tested head `4635f711b5d2d1128a893a8d266920626df8654d` differs only by a one-line const-correct
`EspNativeIntroState_view()` check in startup. GitHub did not emit a new CI
run for that follow-up, so CI PASS is attributed exactly to the functional
commit above while REAL-CYD PASS is attributed to `4635f711b5d2d1128a893a8d266920626df8654d`.

## Real-CYD acceptance

Boot proves the compact Canvas and 10-export bridge:

```text
[DOOMCANVASBRIDGE] INIT exports=10 desktopTU=no bytes=272 retiredDialogStores=2560 retiredZeroRefLayout=240 retiredDormantText=428retiredStateLayout=116 retiredGraphMirrors=28 retiredIntroState=96 clip=160x120
[CORE] DoomCanvas     used=288 heap=186968 largest=110580
[CORE] READY objects=5 heap used=6256 remaining=181380 largest=110580 clip=160x120
[LAYOUT] heap8 used=17584 remaining=163796 largest=110580
[CONFIGMAP] mappingPayload=8376 heap8=144636 largest8=110580
```

Relative to the preceding 368-byte hardware boundary:

```text
CORE READY   180908 -> 181380  (+472)
LAYOUT       163324 -> 163796  (+472)
mappings     144164 -> 144636  (+472)
```

This equals the 96-byte Canvas heap-object reduction plus the 376-byte static
RAM reduction seen by CI.

Fresh START creates the bounded native intro state:

```text
[INTROSTATE] READY bytes=96 owner=native-transient images=4 texts=3 page=0 textPage=0
[INTRO1] READY one deterministic ST_INTRO frame presented once FNV=ade0195d
```

Final Continue parks the clock without loading a map, then the bounded disposer
releases only intro-owned resources:

```text
[INTROFIT] HAND-RELEASE asset=p.bmp owner=native-story
[INTRODISP] FREE image=p.bmp/storyHand heap8=119016->119192 gain=176 owner=native-story
[INTROSTATE] RELEASE owner=native-transient state=NULL
[INTRODISP] FREE owner=native-intro-state stateBytes=96 heap8=119192->153076 gain=33884 state=NULL
[INTRODISP] READY ... frameFNV=68160500->68160500 heap8=119016->153076 recovered=34060 ... owner=NULL assets=NULL texts=NULL ... noMapLoad=yes
```

The native owner recovery includes its images, copied strings and 96-byte state
object. With the separate 176-byte hand, total intro disposal recovers 34060
bytes exactly and leaves the framebuffer unchanged.

Native MAP_INTRO startup then succeeds and the canonical world frame is
unchanged:

```text
[NATIVEBOOT] READY map=1 gameplayLoadMapId=1 spawnTile=904 ... shapeData=0x0 mediaTexels=0x0
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY map=1 angle=64 residentCache=yes largeCache=yes ... shapeData=0x0 mediaTexels=0x0
```

Fresh gameplay covers movement/turning, crate combat/removal, pickups, regular
door animation and HUB/System. Exit To Menu tears down the resident session:

```text
[RESIDENTRESET] heap8=131468->149480 released=18012 before=1/1/1/1/1/1/1 after=0/0/0/0/0/0/0 empty=1
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty saveWrite=no checkpoint=unchanged
```

The same firmware then performs MENU_MAIN -> LOAD on a version-11 checkpoint,
restores resources/script/lines/action removals/crate transforms/automap/
monster state/topology/position/activation/drops, reaches
`ENGINESESSION READY` again with the two forbidden map-wide stores still
NULL, commits further movement and pickups, activates a monster and completes a
real ordered attack plus retaliation.

Representative resumed witness:

```text
[NATIVESAVE] LOAD ... version=11 ... world=...monster-drops-restored-exact session=reprime-pending
[ENGINESESSION] READY map=1 angle=0 ... shapeData=0x0 mediaTexels=0x0
[MONSTERATKVIS] COMPLETE probe=1 ... resolution=unblocked-after-animation
[MONSTERRETAL] COMMIT probe=1 ... playerHP=34->32 armor=23->21 ... rollback=closed
```

## Known unrelated diagnostics

The compact renderer's known
`LEGACY_GUARD -> RETRY -> RECOVERED` recovery fires and succeeds in both
fresh and resumed gameplay. It is not introduced by this Canvas ownership cut.

The bridge boot line still lacks a separator between
`retiredDormantText=428` and `retiredStateLayout=116`. The defect is
cosmetic only. It is intentionally not changed after REAL-CYD PASS because that
would create a new untested code boundary.

## Closure

Hardware-tested code ends at
`4635f711b5d2d1128a893a8d266920626df8654d`.

This closure commit changes documentation only.
