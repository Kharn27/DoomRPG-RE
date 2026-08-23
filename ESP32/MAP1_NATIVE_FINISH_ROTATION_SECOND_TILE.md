# ESP32 native Junction finishRotation second tile milestone

Branch: `agent/esp32-native-finish-rotation-second-tile`

Base merged `main`:

```text
PR   = #74 — native finishRotation orientation
main = 2decae5067438dc1a2d9c29335cfc0cad5538645
```

Status: **HARDWARE CANDIDATE — NOT YET CYD-PROVEN**.

## Objective

PR #74 hardware-proved the first four orientation writes in recovered
`DoomCanvas_finishRotation()` and parks Junction at:

```text
PlayerView FNV=1bd0f09b
InitialTile FNV=f73e28b2
Orientation FNV=acc754a6
world=992/1888
destAngle=64
viewSin=65536
viewCos=0
viewStepX=0
viewStepY=-64
facingRefreshPending=1
```

This milestone owns only the next exact legacy operation:

```c
Game_executeTile(doomCanvas->game,
                 doomCanvas->destX,
                 doomCanvas->destY,
                 DoomCanvas_flagForFacingDir(doomCanvas) | 0x400);
```

The final durable `DoomCanvas_checkFacingEntity()` and `ST_PLAYING` remain
strictly deferred.

## Exact current Junction call

For the hardware-proven fresh-map placement/orientation:

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

The native path represents `skipAdvanceTurn=false` in its own owner and never
mutates legacy `Game_t`.

## Permanent owner

Files:

```text
ESP32/include/esp_player_finish_rotation_tile.h
ESP32/src/esp_player_finish_rotation_tile.c
```

Candidate ABI:

```text
EspPlayerFinishRotationTileState = 24 B
persistent heap = 0 B
```

The state records:

```text
input flags
tile/event identity
initial mutable event state/event flags
eligible/executed/removed command counts
BLOCKINPUT result
skipAdvanceTurn semantic
target/load identity
active state
```

No second-tile state FNV is predicted in advance because the real event command
eligibility under `0x10000400` is intentionally discovered by the hardware
probe rather than guessed.

## API

```text
EspPlayerFinishRotationTile_reset()
EspPlayerFinishRotationTile_isReady()
EspPlayerFinishRotationTile_view()
EspPlayerFinishRotationTile_prepare()
EspPlayerFinishRotationTile_route()
```

Required live owners:

```text
PlayerView active, tileEnterPending=0, facingRefreshPending=1
InitialTile active and identity-matched
Orientation active/prepared and identity-matched
angle=64 with 65536/0/0/-64 orientation values
fresh loadType=0
```

The API is allocation-free and does not mutate PlayerView, InitialTile or
Orientation.

## Event execution boundary

The implementation intentionally reuses the permanent native primitives:

```text
EspMapEvents_findByTile()
EspMapEvents_describe()
EspMapEvents_getCommand()
EspMapEventFilter_prepare()
EspMapEventFilter_evaluate()
EspMapScriptState mutable event/removed-command overlay
EspMapOpcodeExecutor_execute()
```

The generic executor remains deliberately restricted to:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

`prepare()` scans the complete filtered command set before any mutation. If the
real event 61 exposes an eligible opcode outside those IDs under the second-tile
flags, it returns:

```text
ESP_PLAYER_FINISH_ROTATION_TILE_OPCODE_DEFERRED
```

and reports the exact opcode/command offset with no mutation. No fallback to
legacy `Game_executeEvent()` is permitted.

## Atomic route

If all eligible commands are already supported, route execution:

1. executes only through `EspMapOpcodeExecutor`;
2. applies recovered `arg2 & 0x200` command removal in `EspMapScriptState`;
3. keeps bounded rollback state on the stack;
4. restores all script mutations if execution fails;
5. parks the 24-byte second-tile owner only after complete success.

Unlike the first tile milestone, no PlayerView pending bit is consumed here.
The final-facing boundary is represented by `facingRefreshPending=1` remaining
unchanged.

## Hardware probe

Temporary probe files:

```text
ESP32/include/native_junction_finish_rotation_tile_probe.h
ESP32/src/native_junction_finish_rotation_tile_probe.c
```

The normal `esp32-cyd` chain arms it only after the hardware-proven orientation
probe has completed.

Two diagnostic outcomes are intentionally valid before promotion.

### A. Complete native route

Expected markers begin with:

```text
=== Doom RPG ESP32-native Junction finishRotation second tile ===
[JUNCTIONTILE2] READY ...
```

The probe records the real:

```text
stateFNV
eventFound/eventIndex/eventState/eventFlags
eligible/executed/removed counts
script FNV before/after
```

and requires:

```text
tile=943
flags=10000400
PlayerView FNV=1bd0f09b unchanged
InitialTile FNV=f73e28b2 unchanged
Orientation FNV=acc754a6 unchanged
facingRefreshPending=1
final facing still deferred
zero same-build heap delta
no legacy/framebuffer mutation
non-script resident owners stable
```

Script state may change only if eligible supported state opcodes execute.

### B. Unsupported real opcode discovery

Expected marker:

```text
[JUNCTIONTILE2] DEFERRED ... code=<id> arg1=<...> arg2=<...> failClosed=yes
```

This is a successful fail-closed discovery but **not** a second-tile hardware
PASS. The exact real opcode becomes the next bounded prerequisite and must be
integrated natively before this milestone can be promoted.

## Fail-closed proof

The probe exercises:

```text
null PlayerView
null InitialTile
null Orientation
null output
inactive PlayerView
tileEnterPending unexpectedly restored
missing facing pending
wrong angle
Game.f658b execution block
InitialTile identity mismatch
inactive Orientation
Orientation fixed-point mismatch
repeat route
pure-prepare atomicity
```

## Mandatory integrity

The pre-route Junction resident canon is:

```text
snapshotFNV=bc9071e9
runtimeFNV=bc432a0f
mapStateFNV=c5cdfc04
scriptStateFNV=bc9b18ff
lineFNV=3658710d
textureFNV=537319ad
automapFNV=0b2ae445
topologyFNV=d6e8df7d
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes
```

If script execution changes the script overlay, only the script FNV is allowed
to differ. Runtime/map/line/texture/automap/topology and resident topology/counts
must remain stable.

Permanent invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
runtime ZIP map access forbidden
legacy Game.entities = 0
legacy Game.monsters = 0
ST_PLAYING not reached
```

## Ordering boundary

```text
placement                         [hardware-proven]
HUD dirty                         [hardware-proven]
transient old-vector facing       [deliberately unowned]
Player_setup                      [hardware-proven]
initial Game_executeTile          [hardware-proven]
finishRotation orientation prep   [hardware-proven]
second Game_executeTile           [THIS MILESTONE]
final durable facing              [deferred]
ST_PLAYING                        [deferred]
```

Even after a complete second-tile PASS, `finishRotationComplete` remains false
until the durable facing query receives its own native boundary.

## Promotion rule

Do not mark this milestone hardware-proven until the real CYD reaches a complete
`[JUNCTIONTILE2] READY` route. A `DEFERRED` result is discovery only and must be
followed by a bounded native implementation/re-test.

After PASS, update this archive, `PORTING_STATUS.md` and `DOCUMENTATION.md` with
the exact Serial values. Every commit after the flashed firmware SHA must then
remain documentation-only.

## Next boundary after PASS

The next exact legacy operation is:

```text
DoomCanvas_checkFacingEntity()   # durable final facing result
```

Keep `ST_PLAYING` progression separate until that facing ownership is proven.
