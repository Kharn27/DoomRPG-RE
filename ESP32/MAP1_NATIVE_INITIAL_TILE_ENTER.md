# ESP32 native Junction initial tile-enter milestone

Branch: `agent/esp32-native-initial-tile-enter`

Base merged `main`:

```text
PR   = #72 — native fresh-map Player_setup
main = 9077ae4496bdcc06b6b99846332ab43b38943a8a
```

Hardware-tested firmware:

```text
d8fb3e0e372b89d95c37cce558420f7fcb474419
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

PR #72 hardware-proved the fresh-map player setup boundary and parked the active native Junction player/view at:

```text
world=992/1888
angle=64
PlayerView FNV=c21fba3c
hudRefreshPending=0
facingRefreshPending=1
playerSetupPending=0
tileEnterPending=1
Player_setup semanticFNV=3b27c6a1
```

This milestone owns only the next exact recovered operation: the first fresh-map `Game_executeTile()` call made by `Game_spawnPlayer()`.

It deliberately does **not** own `finishRotation()`, its second tile execution, durable facing, `ST_PLAYING`, entity gameplay or rendering.

## Recovered legacy call

The relevant recovered sequence is:

```c
Render.viewZOld = 4;
Hud.isUpdate = true;
DoomCanvas_checkFacingEntity(...);   // transient old vectors
Player_setup(...);
Game_executeTile(game,
                 doomCanvas->viewX,
                 doomCanvas->viewY,
                 0x40f | DoomCanvas_flagForFacingDir(doomCanvas));
```

For the hardware-proven Junction spawn:

```text
viewX=992
viewY=1888
tileX=15
tileY=29
tileIndex=943
destAngle=64
flagForFacingDir(64)=0x10000000
inputFlags=0x1000040f
```

Recovered `Game_executeTile()` semantics are:

```text
if Game.f658b: return false
skipAdvanceTurn=false
bounds-check x>>6 / y>>6
if tile has an event:
  find event by tile
  Game_runEvent(event, start=0, flags)
return whether an event command reported success
```

The native owner represents `skipAdvanceTurn=false` without mutating legacy `Game_t`.

## Permanent owner

Files:

```text
ESP32/include/esp_player_initial_tile.h
ESP32/src/esp_player_initial_tile.c
```

Hardware-proven classic-ESP32 ABI:

```text
EspPlayerInitialTileState = 24 B
persistent heap = 0 B
stateFNV = f73e28b2
```

Real Junction state:

```text
tile=943
flags=1000040f
eventFound=1
eventIndex=61
eventState=0
eventFlags=0
blocked=0
eligible=0
executed=0
removed=0
skipAdvanceTurn=0
active=1
targetMap=9
gameplayLoadMapId=2
loadType=0
playerKeys=00000000
```

`f73e28b2` is now the hardware canon for this 24-byte initial-tile owner at the fresh Junction spawn.

## Important real-event result

The real tile 943 does contain an event:

```text
eventFound=1
eventIndex=61
```

However, with the exact recovered fresh-map flags `0x1000040f`, the current event state and zero player keys, **no command is eligible**:

```text
eligible=0
executed=0
removed=0
blocked=0
```

Therefore this first fresh-map tile dispatch requires no opcode execution and no script mutation. The tiny generic executor remains deliberately limited to 11/19/20; no additional opcode family was implicitly enabled by this milestone.

## Native machinery reused

The implementation reuses:

```text
EspMapEvents_findByTile()
EspMapEvents_describe()
EspMapEvents_getCommand()
EspMapEventFilter_prepare()
EspMapEventFilter_evaluate()
EspMapScriptState event-state / removed-command overlay
EspMapOpcodeExecutor_execute()
```

A complete side-effect-free preflight still rejects any eligible unsupported opcode with `ESP_PLAYER_INITIAL_TILE_OPCODE_DEFERRED`. The real hardware path simply proved that this gate is not reached for the initial Junction tile-enter because there are zero eligible commands.

## Player/view ownership transfer

Hardware proved the exact one-bit ownership transfer:

```text
beforeFNV=c21fba3c
afterFNV=1bd0f09b
hudRefreshPending=0
facingRefreshPending=1
playerSetupPending=0
tileEnterPending=0
placementExact=yes
```

`1bd0f09b` was previously only a predicted fingerprint. It is now a hardware canon.

Only `tileEnterPending` is consumed. `finishRotation`, its second tile execution and durable facing remain pending.

## Script / resident integrity

The native script overlay did not change:

```text
script beforeFNV=bc9b18ff
afterFNV=bc9b18ff
changed=no
```

All resident owners remained canonical:

```text
runtimeFNV=bc432a0f
mapStateFNV=c5cdfc04
lineFNV=3658710d
textureFNV=537319ad
automapFNV=0b2ae445
topologyFNV=d6e8df7d
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes
runtimeStable=yes
nonScriptMutableStable=yes
```

## Hardware fail-closed proof

Real CYD proved:

```text
nullView=1
nullSetup=1
nullOutput=1
angle=1
blocked=1
missingTile=1
missingFacing=1
setupMismatch=1
prepareAtomic=yes
repeat=1
repeatAtomic=yes
```

These gates prove invalid context/order is rejected without partially consuming the player/view boundary.

## RAM proof

Normal `esp32-cyd` hardware run:

```text
heap8=72868->72868
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Stable heartbeat after PASS:

```text
heap=138632
heap8=72868
largest8=34804
```

## Legacy / framebuffer non-mutation

Same-build equality witnesses:

```text
gameFNV=c655ff85->c655ff85
playerFNV=774ed642->774ed642
frameFNV=7a95b5b5->7a95b5b5
legacyRuntimeClear=yes
GameMutation=no
PlayerMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
```

These three FNVs are same-build equality witnesses, not cross-build canons.

## Final hardware PARK

```text
state=9
page=3
targetMap=9
junctionResident=yes
nativePlayerView=yes
nativePlayerSetup=yes
nativeInitialTile=yes
tileEnterPending=no
facingPending=yes
finishRotationPending=yes
secondTilePending=yes
finalFacingPending=yes
ST_PLAYING=no
entities=0
monsters=0
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

## Correct ordering boundary

Hardware-proven fresh-map chain is now:

```text
placement                         [hardware-proven]
HUD dirty                         [hardware-proven]
transient old-vector facing       [deliberately unowned]
Player_setup                      [hardware-proven]
initial Game_executeTile          [hardware-proven]
finishRotation orientation prep   [next]
second Game_executeTile           [deferred]
final durable facing              [deferred]
ST_PLAYING                        [deferred]
```

The natural next bounded milestone after merge is the `DoomCanvas_finishRotation()` orientation preparation (`viewSin`, `viewCos`, `viewStepX`, `viewStepY`) while keeping its second tile dispatch and final facing as separate boundaries unless a fresh legacy audit proves a tighter safe grouping.

## Merge recommendation

```text
MERGE agent/esp32-native-initial-tile-enter
```

Hardware-tested firmware:

```text
d8fb3e0e372b89d95c37cce558420f7fcb474419
```

Every commit after that firmware SHA on this branch must remain documentation-only unless another firmware is flashed.
