# ESP32 native Junction finishRotation second tile milestone

Branch: `agent/esp32-native-finish-rotation-second-tile`

Base merged `main`:

```text
PR   = #74 — native finishRotation orientation
main = 2decae5067438dc1a2d9c29335cfc0cad5538645
```

Hardware-tested firmware:

```text
df4f62687d99eb3b3e9569ae6861b6909d59c82d
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

Own only the second `Game_executeTile()` inside recovered `DoomCanvas_finishRotation()` after the hardware-proven orientation preparation:

```c
Game_executeTile(doomCanvas->game,
                 doomCanvas->destX,
                 doomCanvas->destY,
                 DoomCanvas_flagForFacingDir(doomCanvas) | 0x400);
```

The final durable `DoomCanvas_checkFacingEntity()` and `ST_PLAYING` remain strictly deferred.

## Exact hardware-proven call

For the current Junction fresh-map state:

```text
worldX=992
worldY=1888
tileX=15
tileY=29
tileIndex=943
destAngle=64
DoomCanvas_flagForFacingDir(64)=0x10000000
run/block flag=0x00000400
inputFlags=0x10000400
```

Recovered `Game_executeTile()` semantics remain:

```text
if Game.f658b: return false
skipAdvanceTurn=false
bounds-check x>>6 / y>>6
if tile has an event:
  find event by tile
  Game_runEvent(event, start=0, flags)
return event-command result
```

The native path represents `skipAdvanceTurn=false` in its own owner and never mutates legacy `Game_t`.

## Permanent owner

Files:

```text
ESP32/include/esp_player_finish_rotation_tile.h
ESP32/src/esp_player_finish_rotation_tile.c
```

Hardware-proven ABI:

```text
EspPlayerFinishRotationTileState = 24 B
persistent heap = 0 B
stateFNV = 09e58e0d
```

Real Junction owner:

```text
tile=943
flags=10000400
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

`09e58e0d` is now the canonical hardware fingerprint for this 24-byte second-tile owner.

## Important real-event result

The second tile call resolves the same real tile/event:

```text
tile=943
eventIndex=61
```

Under the exact `finishRotation()` flags `0x10000400`, however, no command is eligible:

```text
eligible=0
executed=0
removed=0
blocked=0
```

Therefore the second tile dispatch executes no opcode and makes no script mutation. The generic `EspMapOpcodeExecutor` remains deliberately restricted to IDs 11/19/20; this milestone broadens nothing.

Together with the first fresh-map tile call, hardware now proves:

```text
first tile  flags=0x1000040f -> event 61 -> eligible=0
second tile flags=0x10000400 -> event 61 -> eligible=0
```

## Native machinery reused

The implementation reuses the permanent native primitives:

```text
EspMapEvents_findByTile()
EspMapEvents_describe()
EspMapEvents_getCommand()
EspMapEventFilter_prepare()
EspMapEventFilter_evaluate()
EspMapScriptState event-state / removed-command overlay
EspMapOpcodeExecutor_execute()
```

A complete side-effect-free preflight still rejects any eligible unsupported opcode with `ESP_PLAYER_FINISH_ROTATION_TILE_OPCODE_DEFERRED`. The real Junction path simply proved that this gate is not reached because the filtered command count is zero.

## Input owner integrity

Hardware proved all previously owned player boundaries remain byte-for-byte canonical:

```text
PlayerView FNV=1bd0f09b -> 1bd0f09b
InitialTile FNV=f73e28b2 unchanged
Orientation FNV=acc754a6 unchanged
facingRefreshPending=1
tileEnterPending=0
```

No PlayerView pending bit is consumed by the second tile. The remaining pending semantic is final durable facing.

## Script / resident integrity

The native script overlay remained unchanged:

```text
script beforeFNV=bc9b18ff
afterFNV=bc9b18ff
changed=no
```

All resident owners remain canonical:

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
nonScriptStable=yes
```

## Hardware fail-closed proof

Real CYD proved:

```text
nullView=1
nullInitial=1
nullOrientation=1
nullOutput=1
inactive=1
tilePending=1
missingFacing=1
angle=1
blocked=1
initialMismatch=1
orientationInactive=1
orientationMismatch=1
prepareAtomic=yes
repeat=1
repeatAtomic=yes
```

Every refusal preserves the live owner graph and script state.

## RAM proof

Normal `esp32-cyd` hardware run:

```text
heap8=72796->72796
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Stable heartbeat after PASS:

```text
heap=138560
heap8=72796
largest8=34804
```

## Legacy / framebuffer integrity

Same-build equality witnesses from the tested firmware:

```text
gameFNV=c655ff85->c655ff85
playerFNV=774ed642->774ed642
canvasFNV=1b7ba23f->1b7ba23f
renderFNV=f9344dec->f9344dec
frameFNV=e6066fb0->e6066fb0
legacyRuntimeClear=yes
GameMutation=no
PlayerMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
```

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
nativeSecondTile=yes
secondTilePending=no
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

## Correct ordering boundary

Hardware-proven fresh-map chain is now:

```text
placement                         [hardware-proven]
HUD dirty                         [hardware-proven]
transient old-vector facing       [deliberately unowned]
Player_setup                      [hardware-proven]
initial Game_executeTile          [hardware-proven]
finishRotation orientation prep   [hardware-proven]
second Game_executeTile           [hardware-proven]
final durable facing              [next]
ST_PLAYING                        [deferred]
```

`finishRotationComplete` remains false because the durable facing query is the final operation of legacy `DoomCanvas_finishRotation()` and still has no native owner.

## Next bounded milestone after merge

Recover from the exact post-merge `main`, then own only:

```text
DoomCanvas_checkFacingEntity()   # durable final facing result
```

That milestone should derive the facing query from the hardware-proven native player position/orientation and compact resident topology without reviving legacy entities/render runtime. `ST_PLAYING` remains a later boundary.

## Merge recommendation

```text
MERGE agent/esp32-native-finish-rotation-second-tile
```

Hardware-tested firmware:

```text
df4f62687d99eb3b3e9569ae6861b6909d59c82d
```

Every later commit on this branch must remain documentation-only unless another firmware is flashed.
