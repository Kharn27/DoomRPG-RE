# Milestone — Retire DoomCanvas intro state and dead shell

Status: **REAL-CYD PASS**

Hardware-tested code boundary:
`d1b052b6d2934b6a5e6aad68aa84b9e52afd85f4`

Branch:
`agent/esp32-intro-state-owner`

## Goal

Continue shrinking the permanent ESP32 `DoomCanvas_t` compatibility object by
moving the complete prologue-only state to a transient native owner, then
retiring the next source-closed group of desktop shell fields and assets without
reviving desktop gameplay/render ownership.

## Part 1 — transient intro owner

ESP32 now owns this exact `ST_INTRO` group in
`EspNativeIntroState_t`:

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

The state is compile-time pinned to 96 bytes. It is allocated only when a fresh
START enters the prologue and is destroyed by the bounded intro disposer before
native MAP_INTRO loading.

`DoomCanvas_loadPrologueText` is no longer an ESP32 source-ABI export. Its
remaining loading/music transition is internal to the bridge and hands ownership
to `EspNativeIntroState`.

## Part 2 — dead shell closure

A second source-closure audit identified 56 bytes that no longer have an ESP32
consumer:

```text
field                    bytes  disposition
memory                       4  desktop allocation metric retired by generator
imgMapCursor                 16  native automap uses compact vector cursor
imgLegals                    16  legal-screen asset already retired
vibrateEnabled                4  config byte consumed but no ESP32 behavior
mouseSensitivity              4  config int consumed but no ESP32 behavior
mouseYMove                    4  config byte consumed but no ESP32 behavior
sndPriority                   4  config byte consumed but no ESP32 behavior
restoreSoftKeys               4  write-only in the retained bridge
                            ----
                              56
```

The configuration parser preserves file-format compatibility by continuing to
consume the retired values. It simply no longer mirrors them into
`DoomCanvas_t`.

The old desktop automap cursor `b.bmp` is no longer created during
`DoomCanvas_startup()`. The native automap already draws its player marker as
a bounded vector primitive.

The legal image is no longer initialized/freed through Canvas or during
main-menu cleanup. The menu cleanup log explicitly reports `legals=retired`.

`renderFloorCeilingTextures` is **not** retired: generated `Render.c` still
reads it in the current closure. Fonts, geometry, state/time, softkey strings,
`doomRpg` and `render` remain live.

## Layout and ABI

```text
DoomCanvas_t                 368 B -> 272 B -> 216 B
intro-state cut                         96 B
dead-shell cut                          56 B
branch-total permanent cut             152 B
desktop layout reclaimed              3524 B / 3740 B (~94.2%)
source ABI exports                      11 -> 10
retired export                          DoomCanvas_loadPrologueText
```

The bridge pins 216 bytes with a `_Static_assert`; future accidental reuse of
the retired fields fails at compile time.

## CI

The first dead-shell expansion commit
`8f701388e733af23d61d9fbcedf3a94aeacd02bb` intentionally did not survive the
layout guard: `vibrateEnabled` was still physically present, so the
`_Static_assert` rejected the expected 216-byte layout.

The corrective commit
`d1b052b6d2934b6a5e6aad68aa84b9e52afd85f4` removes that final field and is the
exact tested boundary.

Normal `esp32-cyd` CI #1662: **SUCCESS**.

```text
[ESP32] Desktop DoomCanvas.c retired; esp_legacy_doomcanvas_bridge.c owns 10 source ABI exports
RAM:   45056 B
Flash: 773713 B
esp32-cyd SUCCESS
```

Static RAM remains 45056 B from the intro-owner milestone. Relative to merged
main (45432 B / 772925 B), the full branch is -376 B static RAM and +788 B
linked Flash while removing 152 permanent Canvas bytes and the obsolete
automap-cursor runtime allocation.

## Real-CYD boot acceptance

```text
Engine structs: Render=5040 Game=4 Canvas=216 Total=6020 bytes
[DOOMCANVASBRIDGE] INIT exports=10 desktopTU=no bytes=216 retiredDialogStores=2560 retiredZeroRefLayout=240 retiredDormantText=428, retiredStateLayout=116 retiredGraphMirrors=28 retiredIntroState=96 retiredDeadShell=56 clip=160x120
[CORE] DoomCanvas     used=232 heap=187024 largest=110580
[CORE] READY objects=5 heap used=6200 remaining=181436 largest=110580 clip=160x120
```

The source-ABI count, physical layout and hardware allocator all agree.

## Permanent + runtime memory gain

Relative to the previous 272-byte REAL-CYD boundary:

```text
checkpoint                  previous       current        gain
CORE READY                   181380         181436        +56 B
LAYOUT READY                 163796         164120       +324 B
mappings resident            144636         144960       +324 B
Exit->Menu heap8             149480         149804       +324 B
```

CORE isolates the 56-byte Canvas shrink exactly.

The additional 268-byte gain beginning at layout is the persistent cost avoided
by no longer loading the legacy `b.bmp` automap cursor. The hardware layout
trace contains only the two live font decodes:

```text
[BMP] decode 144x72 4-bpp ... out=5184
[BMP] decode 208x102 4-bpp ... out=10608
[LAYOUT] heap8 used=17316 remaining=164120 largest=110580
```

There is no 6x48 `b.bmp` decode.

## Intro lifecycle remains exact

The complete intro remains functional. Final Continue parks the intro clock and
the bounded disposer frees the story hand and transient owner without loading a
map early:

```text
[INTROFIT] HAND-RELEASE asset=p.bmp owner=native-story
[INTRODISP] FREE image=p.bmp/storyHand heap8=119328->119504 gain=176 owner=native-story
[INTROSTATE] RELEASE owner=native-transient state=NULL
[INTRODISP] FREE owner=native-intro-state stateBytes=96 heap8=119504->153400 gain=33896 state=NULL
[INTRODISP] READY ... frameFNV=68160500->68160500 heap8=119328->153400 recovered=34072 ... owner=NULL assets=NULL texts=NULL clip=off noMapLoad=yes
```

The framebuffer hash is unchanged across disposal. The four prologue images,
three copied strings and 96-byte state all leave ownership before MAP_INTRO.

## Native map/session invariants

Native loading then rebuilds MAP_INTRO from the PAK and preserves the canonical
first visible world frame:

```text
[NATIVEBOOT] READY map=1 gameplayLoadMapId=1 spawnTile=904 ... shapeData=0x0 mediaTexels=0x0
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY map=1 angle=64 residentCache=yes largeCache=yes ... shapeData=0x0 mediaTexels=0x0
```

The acceptance run exercises movement and turning, crate transform/combat,
multiple armor pickups, regular/deferred door animation and HUB/System.

## Exit / menu / LOAD regression

Confirmed Exit To Menu releases the resident session:

```text
[RESIDENTRESET] heap8=131796->149804 released=18008 before=1/1/1/1/1/1/1 after=0/0/0/0/0/0/0 empty=1
[MAINMENU] Runtime cleanup legals=retired heap8=149804->149804 gained=0 largest8=110580->110580 ... shapeData=0x0 mediaTexels=0x0
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty saveWrite=no checkpoint=unchanged
```

The zero-byte legal cleanup is the expected witness: there is no hidden Canvas
legal image left to release.

MENU_MAIN -> LOAD restores a version-11 checkpoint exactly and resumes native
gameplay:

```text
[NATIVESAVE] LOAD ... version=11 ... world=resources+script+lines+action-removals+crate-transforms+automap+monster-state+topology+position+activation+monster-drops-restored-exact session=reprime-pending
[ENGINESESSION] READY map=1 angle=0 ... shapeData=0x0 mediaTexels=0x0
[MONSTERATKVIS] COMPLETE probe=1 ... resolution=unblocked-after-animation
[MONSTERRETAL] COMMIT probe=1 ... playerHP=34->32 armor=19->17 ... rollback=closed
```

The resumed session also accepts further movement/pickups and visibility-based
monster activation.

## Known unrelated diagnostics

The compact renderer's established
`LEGACY_GUARD -> RETRY -> RECOVERED` path fires and recovers normally in both
fresh and resumed gameplay. It is not introduced by this Canvas cut.

The resumed monster sequencer may still fail closed with
`cause=active-order-not-owned` when no movement candidate owns the current
active-order boundary; that is existing gameplay policy, not a Canvas ownership
regression.

## Cosmetic log closure

The previously concatenated
`retiredDormantText=428retiredStateLayout=116` boot witness is fixed in this
tested code boundary. Hardware now prints the fields separately.

## Closure

Hardware-tested code ends at
`d1b052b6d2934b6a5e6aad68aa84b9e52afd85f4`.

No code/runtime commit follows that SHA. The closure commit changes
documentation only.
