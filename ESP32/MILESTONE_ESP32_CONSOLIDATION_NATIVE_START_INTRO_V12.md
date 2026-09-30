# ESP32 consolidation milestone — native START + intro escape V12

Date: 2026-09-30

## Boundary

```text
repo = Kharn27/DoomRPG-RE
base main = 4c6071ebe7de01f47925bf7792123e8c8f9d7ff5
branch = agent/esp32-consolidation-main-menu-start-v12
hardware-tested code head = 6b565cd46172e384209a1d93e355c951f4d6c4fa
CI #1185 = SUCCESS
artifact id = 11113402480
artifact digest = sha256:3d8b8c332b780b55a8fdab1874e93759d3e3780f8816f1259da2593170fc9127
firmware.bin = 782368 B
firmware sha256 = 1247ff133519951a616f269395cf5cbfbd05fcd5f0175b0aa0ae9dbaee6ddc84
firmware.elf sha256 = c38e73687d33656eddb106e4b4e3ba23392a71d381299aab79637cde1afc5d2e
static RAM = 45224 B
linked Flash = 782009 B
```

## Goal

Remove the remaining legacy pre-game START and intro-page escape closure while
preserving the already hardware-proven native intro disposal and resident map
bootstrap.

No new source file, public API or generic router is introduced.

## Exact legacy behavior retained

The legacy `Menu_startGame(new)` branch used by MENU_MAIN performed:

```text
menuSystem->imgBG = NULL
Player_reset(player)
player->totalDeaths = 0
DoomCanvas_setState(ST_INTRO)
```

That exact new-game branch is now executed directly by
`native_main_menu_start_action.c`.

The legacy helper also contained unrelated resume and skip-intro map-loading
branches. Those are not part of the native START contract: LOAD already owns
resume, while skip-intro would bypass the native resident bootstrap. V12 keeps
that unsupported START mode fail-closed before destructive cleanup or player
mutation.

## Intro page ownership

Before V12, `native_story_fit.c` still called
`DoomCanvas_changeStoryPage()`. That legacy helper could advance to page 3,
call `DoomCanvas_disposeIntro()`, and immediately enter
`DoomCanvas_loadMap()`.

The permanent ESP32 ownership is now:

```text
page 0 text/input
 -> page 1 animation
 -> native renderer timed 1 -> 2 only
 -> page 2 final text/input
 -> Esp32IntroClock PARK
 -> Esp32IntroDispose_service
 -> native transition/bootstrap
```

The renderer cannot dispose the intro or load a map.

## Final ELF

Direct artifact ELF inspection:

```text
Menu_startGame               ABSENT
DoomCanvas_loadState         ABSENT
DoomCanvas_changeStoryPage   ABSENT
DoomCanvas_disposeIntro      ABSENT
DoomCanvas_loadMap           ABSENT
```

The retained native owners remain linked, including `Player_reset`,
`DoomCanvas_setState` and `Esp32IntroDispose_service`.

## Resource delta

Against merged main:

```text
                 main 4c6071e    V12 6b565cd    delta
static RAM       45224 B         45224 B         0 B
Flash           782265 B        782009 B      -256 B
firmware.bin    782624 B        782368 B      -256 B
```

## Real classic-CYD proof

Native START reaches the normal intro boundary with unchanged memory:

```text
[MAINSTART] Begin ... heap8=21460 largest8=10740 shapeData=0x0 mediaTexels=0x0
[MAINSTART] READY native new-game -> Player_reset -> ST_INTRO
[INTROCLK] ARMED ... heap8=43104 largest8=12276
```

The newly owned automatic transition is explicitly exercised without a touch
during page 1:

```text
[INTROIN] CONTINUE storyPage=0->1 t=1443700 epoch=1443700
[INTROCLK] AUTO-PAGE 1->2 t=1453800 textPage=0 epoch=1453800
[INTROCLK] ... page=2 textPage=0 textDone=0
```

Final text presentation and exit remain native:

```text
[INTROIN] FINAL-TEXT-PRESENTED t=1457300 continueUnlocked=yes
[INTROIN] FINAL-CONTINUE page=2 textPage=0 t=1481600 fullTextPresented=yes
[INTROCLK] PARK reason=intro-exit-ready ... heap8=43104 largest8=12276
```

Bounded disposal frees only intro resources and still forbids map loading:

```text
[INTRODISP] READY state=9 page=3 ... heap8=43104->76876 recovered=33772 ... noMapLoad=yes
[INTRODISP] PARK ... shapeData=0x0 mediaTexels=0x0 nodes=0x0 lines=0x0 mapSprites=0x0
```

The native transition then loads Entrance with the canonical fingerprints:

```text
[BSPREAD] ENTRY /intro.bsp ... size=21823 crc32=623f34e4
[MAPRT] READY arenaBytes=14095 ... arenaFNV=c3882516
[NATIVEBOOT] READY map=1 ... shapeData=0x0 mediaTexels=0x0
```

Resident gameplay comes up normally:

```text
[ENGINESESSION] READY map=1 angle=64 residentCache=yes largeCache=yes touch=invisible-120ms TURN+MOVE=armed shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap=93076 heap8=27368 largest8=18420 ...
```

## Result

PASS on the real classic CYD.

The desktop/J2ME START/load-map/intro-dispose escape closure is no longer linked
into the ESP32 firmware. Post-test changes must remain documentation-only before
merge.
