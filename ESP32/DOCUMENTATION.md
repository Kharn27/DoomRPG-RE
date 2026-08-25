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

Older archives remain available in Git history; `PORTING_STATUS.md` is the preferred recovery entry point.

## Latest merged boundary

PR #91 established the first complete static gameplay presentation including the native compact HUD.

```text
main=7686f7fb5c93d375f51a34ec0dd0b5cb127017e3
JunctionHudFrameFNV=ba3e5182
JunctionGlowViewportFNV=9206eb24
EspNativeGameplayHudState=22 B
HudStateFNV=4756db9c
shapeData=NULL
mediaTexels=NULL
```

Merged evidence: [`MAP1_NATIVE_GAMEPLAY_HUD.md`](MAP1_NATIVE_GAMEPLAY_HUD.md).

## Current candidate milestone

[`MAP1_NATIVE_GAMEPLAY_INPUT.md`](MAP1_NATIVE_GAMEPLAY_INPUT.md) records the gameplay touch-intent boundary currently on:

```text
branch=agent/esp32-native-touch-input-intent
base=7686f7fb5c93d375f51a34ec0dd0b5cb127017e3
implementation HEAD=8c620093650ac4efd5d470343466ce9f5c441e4e
status=REAL-CYD VISUAL PASS / FINAL SERIAL ARCHIVE PENDING
merge-ready=no
```

### Native touch ownership

The permanent input layer owns one compact pointer-free semantic intent and no hidden queue:

```text
EspNativeGameplayInputState=12 B
EspNativeGameplayTouchHit=6 B
one pending intent maximum
busy producer=fail closed
```

Physical CYD touch remains calibrated by `SoftXpt2046` and maps to the logical 160x120 framebuffer coordinate space.

### Final 12-zone layout

```text
top HUD y=0..19:
  MENU      = x0..31
  PASS_TURN = x32..127
  AUTOMAP   = x128..159

world y=20..45:
  STRAFE_LEFT | FORWARD | STRAFE_RIGHT

world y=46..72:
  TURN_LEFT | SELECT | TURN_RIGHT

world y=73..99:
  PREV_WEAPON | BACK | NEXT_WEAPON

bottom HUD y=100..119:
  unbound
```

Recovered semantic action IDs remain the original mobile IDs: `1,2,3,4,5,6,7,9,10,11,12,14`.

### Neon transient feedback

The current feedback exists only to make invisible hitboxes discoverable while preserving the static gameplay framebuffer:

```text
hold=250 ms
style=neon double ring + vector glyph
row palettes=BLUE / GREEN / YELLOW / RED
max edits=512
saved edit=offset + original RGB565 pixel
bounded static storage ~= 2 KiB + metadata
runtime allocations=0
PAK/SD reads=0
restore=reverse-order exact pixel restore
```

Vector glyphs cover movement arrows, bent turn arrows, SELECT reticle, `<<`/`>>` weapon cycling, MENU, AUTOMAP and PASS_TURN.

The user visually confirmed the final colored 12-zone layout as perfect on the physical CYD.

### Hardware evidence status

The earlier real-CYD input run before final polish proved the semantic owner, calibrated touch path and exact restore contract:

```text
baseline=ba3e5182
stateBytes=12
hitBytes=6
heap8=69828->69828
largest8=34804->34804
valid taps => INTENT
feedback restore => ba3e5182 exact=yes
no gameplay dispatch
```

The final implementation subsequently changed the active layout from 11 to 12 zones and replaced the predecessor feedback with row-coded neon double rings and vector glyphs. Therefore exact final-HEAD runtime values are **not** canonical until the Serial block from `8c620093...` is archived.

## Stable recovery canons through merged main

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
 -> native gameplay HUD painting                  [hardware-proven]
 -> native gameplay touch intent                  [candidate: visual PASS, Serial final pending]
 -> first real native gameplay action family      [NEXT after merge]
 -> turn/entity/monster gameplay by bounded family
 -> expanded native renderer/gameplay
```

## Current hardware PARK

Merged main remains:

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
glowPending=no
nativeHud=yes
hudPending=no
gameplayDispatchPending=yes
initialSavePersistencePending=yes
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

Candidate branch arms touch-intent classification after this PARK but still executes no gameplay action.

Mandatory invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
runtime ZIP map/graphics access forbidden
legacy Game.entities == 0
legacy Game.monsters == 0
```

## Classic CYD presentation profile

The logical framebuffer remains raw RGB565, presented by exact nearest-neighbour x2 from 160x120 to 320x240. Permanent hardware-selected panel policy:

```text
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
DO NOT MERGE YET
```

Archive the final Serial block from implementation HEAD:

```text
8c620093650ac4efd5d470343466ce9f5c441e4e
```

The final block should prove 12-zone READY, representative BLUE/GREEN/YELLOW/RED feedback, `PREV_WEAPON`, `NEXT_WEAPON`, `PASS_TURN`, exact `ba3e5182` restores, stable heap/largest and no `FAILED`/`ERROR`.

After that proof, any remaining commits must be documentation-only before merge-ready declaration.

## Next bounded milestone after merge

Recover the exact new `main` SHA and implement only the first small real gameplay action family, preferably `TURN_LEFT` / `TURN_RIGHT`.

All other recognized touch intents remain deferred/fail-closed. Do not enable the broad legacy playing-event loop, movement collision, full turn advancement, monster gameplay or general entity activation in the same milestone.