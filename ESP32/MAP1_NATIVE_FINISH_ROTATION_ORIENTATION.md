# ESP32 native Junction finishRotation orientation milestone

Branch: `agent/esp32-native-finish-rotation-orientation`

Base merged `main`:

```text
PR   = #73 — native initial tile-enter
main = 0bc171affad8416ed1a7918a4a67fd4d53d61efe
```

Hardware-tested firmware:

```text
850f1651db7ca1943d5647a00099dfb48c9de284
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

Own only the first four writes in recovered `DoomCanvas_finishRotation()` after the hardware-proven first fresh-map tile dispatch:

```c
doomCanvas->viewSin = doomCanvas->render->sinTable[doomCanvas->destAngle & 255];
doomCanvas->viewCos = doomCanvas->render->sinTable[(doomCanvas->destAngle + 64) & 255];
doomCanvas->viewStepX = (doomCanvas->viewCos * 64) >> 16;
doomCanvas->viewStepY = ((-doomCanvas->viewSin) * 64) >> 16;
```

The second `Game_executeTile()`, final durable `DoomCanvas_checkFacingEntity()`, `ST_PLAYING`, entity gameplay and rendering remain deferred.

## Permanent owner

Files:

```text
ESP32/include/esp_player_orientation_state.h
ESP32/src/esp_player_orientation_state.c
```

Hardware-proven ABI:

```text
EspPlayerOrientationState = 24 B
persistent heap = 0 B
```

API:

```text
EspPlayerOrientation_reset()
EspPlayerOrientation_isReady()
EspPlayerOrientation_view()
EspPlayerOrientation_prepare()
EspPlayerOrientation_route()
```

The API is map-generic but this milestone enables only `destAngle == 64`. Other angles remain fail-closed. `prepare()` is pure and zeroes caller output on refusal. `route()` parks only the new owner and never mutates `EspPlayerViewState` or `EspPlayerInitialTileState`.

## Hardware-proven orientation canon

Real CYD proved exact native-vs-legacy 16.16 fixed-point equality:

```text
stateBytes=24
stateFNV=acc754a6
destAngle=64
viewSin=65536
viewCos=0
viewStepX=0
viewStepY=-64
legacySin=65536
legacyCos=0
legacyStepX=0
legacyStepY=-64
exact=yes
prepared=1
active=1
targetMap=9
gameplayLoadMapId=2
loadType=0
```

`acc754a6` is now the canonical hardware fingerprint for the 24-byte Junction orientation owner at this boundary.

## Input owner canons remain unchanged

The orientation milestone begins from the hardware-proven post-initial-tile state:

```text
EspPlayerViewState=44 B
PlayerView FNV=1bd0f09b
hudRefreshPending=0
facingRefreshPending=1
playerSetupPending=0
tileEnterPending=0

EspPlayerInitialTileState=24 B
InitialTile FNV=f73e28b2
tile=943
eventIndex=61
eligible=0
executed=0
```

Hardware proved PlayerView remained byte-for-byte unchanged:

```text
beforeFNV=1bd0f09b
afterFNV=1bd0f09b
unchanged=yes
```

The initial-tile owner also remains canonical and unchanged.

## Ordering boundary

Hardware-proven fresh-map chain is now:

```text
placement                         [hardware-proven]
HUD dirty                         [hardware-proven]
transient old-vector facing       [deliberately unowned]
Player_setup                      [hardware-proven]
initial Game_executeTile          [hardware-proven]
finishRotation orientation prep   [hardware-proven]
second Game_executeTile           [next]
final durable facing              [deferred]
ST_PLAYING                        [deferred]
```

The orientation owner does not claim `finishRotation()` complete because the second tile dispatch and final facing still have not executed.

## Fail-closed hardware proof

Real CYD proved:

```text
nullView=1
nullTile=1
nullOutput=1
inactive=1
tilePending=1
missingFacing=1
angle=1
tileInactive=1
tileMismatch=1
prepareAtomic=yes
repeat=1
repeatAtomic=yes
```

Every refusal preserves the live owner graph.

## Resident / RAM proof

Real CYD:

```text
snapshotFNV=bc9071e9->bc9071e9
unchanged=yes
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes

heap8=72828->72828
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Stable heartbeat after PASS:

```text
heap=138592
heap8=72828
largest8=34804
```

Resident owner canons remain:

```text
runtime=bc432a0f
map=c5cdfc04
script=bc9b18ff
line=3658710d
texture=537319ad
automap=0b2ae445
topology=d6e8df7d
```

## Legacy / framebuffer integrity

Same-build equality witnesses from the tested firmware:

```text
gameFNV=ea6207e5->ea6207e5
playerFNV=2c811802->2c811802
canvasFNV=1b7ba23f->1b7ba23f
renderFNV=f9344dec->f9344dec
frameFNV=faa62417->faa62417
legacyRuntimeClear=yes
GameMutation=no
PlayerMutation=no
DoomCanvasMutation=no
RenderMutation=no
```

The probe contract also forbids HUD mutation and the implementation performs no HUD write. No separate HUD fingerprint is materialized by this probe, so none is promoted as a hardware canon here.

These equality FNVs are same-build witnesses, not cross-build canons.

## Final hardware PARK

```text
state=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativePlayerView=yes
nativeInitialTile=yes
nativeOrientation=yes
orientationPending=no
secondTilePending=yes
finalFacingPending=yes
finishRotationComplete=no
ST_PLAYING=no
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

Mandatory invariants remain true:

```text
shapeData == NULL
mediaTexels == NULL
runtime ZIP map access forbidden
legacy Game.entities = 0
legacy Game.monsters = 0
ST_PLAYING not reached
```

## Next bounded milestone after merge

Recover from the exact post-merge `main`, then own only the second `Game_executeTile()` inside `DoomCanvas_finishRotation()`:

```text
Game_executeTile(destX, destY, DoomCanvas_flagForFacingDir() | 0x400)
```

At the current Junction angle 64 this means input flags `0x10000400` at world `(992,1888)` / tile `943`.

Keep the final durable `checkFacingEntity()` and `ST_PLAYING` as later boundaries unless a fresh legacy audit proves otherwise.

## Merge recommendation

```text
MERGE agent/esp32-native-finish-rotation-orientation
```

Hardware-tested firmware:

```text
850f1651db7ca1943d5647a00099dfb48c9de284
```

Every later commit on this branch must remain documentation-only unless another firmware is flashed.
