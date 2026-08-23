# ESP32 native Junction post-load HUD clear milestone

Branch: `agent/esp32-native-post-load-hud-clear`

Base merged `main`:

```text
PR   = #76 — native durable facing
main = 3ab143110a1f44ebb44bc130d12d1844f3ae73ca
```

Hardware-tested firmware:

```text
469abe119fbc401d812c21f96d94fd8aaae06ff3
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

Own only the first caller-side side effect immediately after the now
hardware-proven `DoomCanvas_finishRotation()`:

```c
DoomCanvas_finishRotation(doomCanvas);

doomCanvas->hud->msgCount = 0;
doomCanvas->hud->statBarMessage = NULL;
doomCanvas->hud->logMessage[0] = '\0';
```

The milestone does not execute the following caller operations:

```text
Junction Game_givemap()
Player_selectWeapon()
initial Game_saveState()
Game.isLoaded/isSaved/activeLoadType cleanup
numEvents / particle cleanup
isUpdateView=true
DoomCanvas_setState(ST_PLAYING)
idleTime update
```

## Recovered caller order

```text
1. clear HUD message channels                 [HARDWARE-PROVEN]
2. Junction Game_givemap()                    [next]
   else DoomCanvas_uncoverAutomap()
3. Player_selectWeapon(current weapon)        [deferred]
4. initial Game_saveState when !isLoaded      [deferred]
5. clear isLoaded/isSaved/activeLoadType      [deferred]
6. clear queued events / particles            [deferred]
7. isUpdateView=true                          [deferred]
8. DoomCanvas_setState(ST_PLAYING)            [deferred]
9. idleTime=time+8000                         [deferred]
```

This boundary is intentionally separate from the existing post-spawn
`EspHudRefreshState`: that earlier owner represents `Hud.isUpdate=true`, while
this owner represents the later message-channel reset.

## Hardware-proven input boundary

The real CYD entered this milestone only after PR #76 durable facing was complete:

```text
PlayerView FNV=afcdcf74
hudRefreshPending=0
facingRefreshPending=0
playerSetupPending=0
tileEnterPending=0

EspPlayerFacingState=32 B
facing FNV=95aa1108
kind=none
finishRotationComplete=yes

Junction resident snapshot FNV=bc9071e9
automap FNV=0b2ae445
ST_PLAYING=no
legacy Game.entities=0
legacy Game.monsters=0
```

## Permanent owner

```text
ESP32/include/esp_hud_post_load_clear_state.h
ESP32/src/esp_hud_post_load_clear_state.c
```

Hardware-proven ABI:

```text
EspHudPostLoadClearState = 8 B
persistent heap = 0 B
stateFNV = b7383e18
```

Hardware-proven state:

```text
messageCount=0
statBarMessagePresent=0
logMessageLength=0
cleared=1
active=1
targetMap=9
gameplayLoadMapId=2
loadType=0
```

API:

```text
EspHudPostLoadClear_reset()
EspHudPostLoadClear_isReady()
EspHudPostLoadClear_view()
EspHudPostLoadClear_prepare()
EspHudPostLoadClear_route()
```

`prepare()` is pure. It requires an active fresh-map PlayerView whose prior
spawn/setup/tile/facing responsibilities are all complete and a matching durable
facing owner. Saved-world load remains fail-closed until separately recovered.

`route()` parks the owner once. It does not add or consume another PlayerView
pending bit. No legacy `Hud_t*` pointer is retained or written.

## Real-CYD evidence

The tested normal `esp32-cyd` firmware produced:

```text
[JUNCTIONHUDCLEAR] READY stateBytes=8 stateFNV=b7383e18 messageCount=0 statBarMessagePresent=0 logMessageLength=0 cleared=1 active=1 targetMap=9 gameplayLoadMapId=2 loadType=0
[JUNCTIONHUDCLEAR] INPUT viewFNV=afcdcf74 facingFNV=95aa1108 unchanged=yes finishRotationComplete=yes
```

The semantic witness proves the three exact legacy writes are represented while
the real legacy HUD remains untouched:

```text
legacyMsgCount=0->0
legacyStatBarPresent=0->0
legacyLogFirst=0->0
legacyHudUntouched=yes
```

Fail-closed hardware proof:

```text
nullView=1
nullFacing=1
nullOutput=1
inactiveView=1
inactiveFacing=1
facingMismatch=1
loadType=1
order=1
prepareAtomic=yes
repeat=1
repeatAtomic=yes
```

Resident and automap integrity:

```text
snapshotFNV=bc9071e9->bc9071e9
unchanged=yes
automapFNV=0b2ae445->0b2ae445
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes
Game_givemapDeferred=yes
```

RAM proof on the normal environment:

```text
heap8=72732->72732
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Same-build equality witnesses:

```text
gameFNV=d073b2d5->d073b2d5
playerFNV=c64e7862->c64e7862
hudFNV=b18611d2->b18611d2
canvasFNV=70a8ad15->70a8ad15
renderFNV=f9344dec->f9344dec
frameFNV=9eb7ce0f->9eb7ce0f
legacyRuntimeClear=yes
GameMutation=no
PlayerMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
```

These equality FNVs are same-build witnesses, not cross-build canons.

Hardware PARK:

```text
state=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativeFacing=yes
nativeHudClear=yes
finishRotationComplete=yes
Game_givemapPending=yes
weaponReselectPending=yes
initialSavePending=yes
postLoadCleanupPending=yes
ST_PLAYING=no
entities=0
monsters=0
noGameplay=yes
```

Stable heartbeat immediately after the milestone:

```text
heap=138496
heap8=72732
largest8=34804
SD=ready
VIDEO=ready
CORE=ready
```

## Mandatory invariants

```text
shapeData == NULL
mediaTexels == NULL
runtime ZIP access forbidden
legacy Game.entities = 0
legacy Game.monsters = 0
ST_PLAYING not reached
```

## Promotion result

The milestone satisfies the promotion rule on real hardware. The tested firmware
SHA is `469abe119fbc401d812c21f96d94fd8aaae06ff3`.

Every later commit on this branch must remain documentation-only unless another
firmware is flashed.

## Next boundary after merge

The next exact caller operation on Junction is:

```c
Game_givemap(doomCanvas->game);
```

Recovered semantics:

```text
all non-hidden lines: flags |= 0x80
all map sprites: info |= 0x10000000
all BIT_AM_ENTRANCE tiles: add BIT_AM_VISITED
```

That direct caller-side reveal must remain a separate milestone from this HUD
clear and from later weapon/save/ST_PLAYING progression.

## Merge recommendation

```text
MERGE agent/esp32-native-post-load-hud-clear
```
