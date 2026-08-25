# MAP1 native gameplay HUD

## Status

```text
branch = agent/esp32-native-gameplay-hud
base   = 30351fd0a867e18dad171962b00d70923b4d173f
hardware-tested firmware = fa6b0d2ab4c1ec2598b92dfe635a84ff50a74867
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Base `30351fd0a867e18dad171962b00d70923b4d173f` is merged PR #90 (`agent/esp32-native-junction-glows`).

## Objective

Close the first visible native gameplay UI boundary without opening gameplay dispatch.

The current Junction PARK already owns the player/view state, post-load HUD clear semantics and one pending 8-byte HUD dirty intent. This milestone consumes that dirty intent only after successfully painting the original current-pose gameplay HUD directly into the existing logical framebuffer.

The scope is deliberately limited to the exact current fresh-Junction pose:

```text
logical framebuffer = 160x120 RGB565
HUD top band         = y 0..19
world viewport       = y 20..99 / 160x80
HUD bottom band      = y 100..119
health               = 30 / 30
armor                = 0 / 20
weapon               = 2 / pistol
ammo type            = 1
ammo                  = 8
face                  = 0 / normal
direction             = N
```

No input, turn advancement, gameplay dispatch, entity/monster activation or legacy gameplay/render loop is part of this milestone.

## Permanent native implementation

New permanent modules:

```text
ESP32/include/esp_native_indexed_bmp.h
ESP32/src/esp_native_indexed_bmp.c
ESP32/include/esp_native_gameplay_hud.h
ESP32/src/esp_native_gameplay_hud.c
```

The painter reproduces the relevant legacy `Hud_drawTopBar()` / `Hud_drawBottomBar()` result for the current hardware-proven pose while keeping the ESP32 ownership model native.

HUD assets are read only from:

```text
/DoomRPG-ESP32.pak
```

Current required source assets:

```text
a.bmp  Doom bitmap font
k.bmp  compact status-bar tile
l.bmp  compact HUD faces
m.bmp  compact HUD icon sheet
o.bmp  orientation arrow
```

The indexed-BMP reader validates and streams bounded source rows. It does not create SDL textures and does not retain decoded images or map-wide texel arrays.

Permanent invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
runtime ZIP graphics access = forbidden
legacy Game.entities = 0
legacy Game.monsters = 0
```

## HUD dirty ownership

The previously hardware-proven `EspHudRefreshState` remains the semantic dirty owner.

Before paint:

```text
stateBytes=8
stateFNV=6965ee06
refreshPending=1
```

The native painter may consume that request only after the complete HUD paint succeeds. The consumed state is:

```text
stateFNV=40c66f99
refreshPending=0
```

No legacy `Hud_t` field is used as the permanent dirty owner and the legacy HUD object remains bit-stable across the route.

## Real-CYD evidence

The user visually confirmed that the HUD is correctly visible on the classic CYD.

Hardware log from firmware `fa6b0d2ab4c1ec2598b92dfe635a84ff50a74867`:

```text
=== Doom RPG ESP32-native Junction initial gameplay HUD ===
[JUNCTIONHUDPAINTPROBE] CONTRACT reproduce current-pose legacy Hud_drawTopBar/Hud_drawBottomBar as one native 160x120 painter: empty tiled top bar plus compact bottom bar 30HP/0 armor/pistol ammo8/normal face/N; assets a/k/l/m/o come only from DoomRPG-ESP32.pak via bounded indexed row reads; consume the existing 8B HUD dirty intent only after paint; preserve the 160x80 gameplay viewport, all native world owners and every legacy Game/Player/Hud/DoomCanvas/Render field; no input, turn, gameplay dispatch, legacy renderer or runtime ZIP
[VIDEO] Present 160x120 -> 320x240 exact 2x raw RGB565: 34418 us
[JUNCTIONHUDPAINT] READY stateBytes=22 stateFNV=4756db9c frame=b5218f24->ba3e5182 viewport=9206eb24 preserved=yes bands=9cf0c5c5->6c2aa46f hp=30/30 armor=0/20 weapon=2 ammoType=1 ammo=8 face=0 dir=N
[JUNCTIONHUDPAINT] ASSETS validated=5 bar=20x20 icon=13x13 face=18x20 reads=184 bytes=6344 rows=164 pixels=7538 packClosed=yes
[JUNCTIONHUDPAINT] MEMORY heap=70196->70196 largest=34804->34804 delta=0 dirty=6965ee06->40c66f99 consumed=yes legacyHudStable=yes playerStable=yes gameStable=yes canvasStable=yes renderStable=yes residentStable=yes topology=d6e8df7d catalog=257444a5
[JUNCTIONHUDPAINT] PARK nativeHud=yes hudPending=no worldViewportPreserved=yes glowCompanions=yes gameplayDispatchPending=yes legacyState=9 entities=0 monsters=0 noGameplay=yes presented=1
```

## Stable framebuffer canons

Predecessor complete world + sprite + glow frame:

```text
frameBeforeFNV=b5218f24
worldViewportFNV=9206eb24
viewport=160x80 @ 0,20
```

After native HUD paint:

```text
frameAfterFNV=ba3e5182
worldViewportFNV=9206eb24
viewportPreserved=yes
HUD bands FNV=9cf0c5c5->6c2aa46f
```

The full-frame hash changes only because the top and bottom HUD bands are painted. The gameplay viewport remains byte-identical.

## Painter / asset canon

```text
EspNativeGameplayHudState=22 B
stateFNV=4756db9c
assetsValidated=5
bar=20x20
icon=13x13
face=18x20
PAK reads=184
PAK bytes=6344
source rows=164
painted pixels=7538
packClosed=yes
```

## RAM / side-effect proof

```text
heap8=70196->70196
largest8=34804->34804
heapDelta=0
largestDelta=0
legacyHudStable=yes
playerStable=yes
gameStable=yes
canvasStable=yes
renderStable=yes
residentStable=yes
topology=d6e8df7d
closedGraphicsCatalog=257444a5
shapeData=NULL
mediaTexels=NULL
legacy Game.entities=0
legacy Game.monsters=0
```

The route does not consume input, advance a turn, execute native gameplay dispatch or call legacy `DoomCanvas_playingState()` / `Render_render()`.

## Hardware PARK after this milestone

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

## Architectural consequence

The first complete static gameplay presentation is now native-owned on the ESP32:

```text
compact Junction runtime
 -> native walls + textured planes
 -> BSP-visible sprites + additive glows
 -> native gameplay HUD
 -> raw 160x120 RGB565 framebuffer
 -> exact CYD x2 presentation
```

The desktop/J2ME HUD remains a behavioral/layout reference, not the permanent ESP32 ownership model.

## Next bounded milestone after merge

Recover the exact merged `main` SHA before branching again. The next coherent frontier is native gameplay input/dispatch, but it must be split into a small action family after rereading the exact legacy semantics.

Do not combine the first input milestone with broad turn advancement, monster gameplay or full entity activation. Preserve the current native renderer/HUD PARK and keep all unsupported actions fail-closed until their dedicated milestone.