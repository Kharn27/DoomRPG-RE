# ESP32 native Junction finishRotation orientation milestone

Branch: `agent/esp32-native-finish-rotation-orientation`

Base merged `main`:

```text
PR   = #73 — native initial tile-enter
main = 0bc171affad8416ed1a7918a4a67fd4d53d61efe
```

Status: **HARDWARE CANDIDATE — NOT YET CYD-PROVEN**.

## Objective

PR #73 hardware-proved the first fresh-map `Game_executeTile()` at Junction and parks the native player state at:

```text
world=992/1888
angle=64
PlayerView FNV=1bd0f09b
InitialTile FNV=f73e28b2
hudRefreshPending=0
facingRefreshPending=1
playerSetupPending=0
tileEnterPending=0
```

This milestone owns only the first four writes in recovered `DoomCanvas_finishRotation()`:

```c
doomCanvas->viewSin = doomCanvas->render->sinTable[doomCanvas->destAngle & 255];
doomCanvas->viewCos = doomCanvas->render->sinTable[(doomCanvas->destAngle + 64) & 255];
doomCanvas->viewStepX = (doomCanvas->viewCos * 64) >> 16;
doomCanvas->viewStepY = ((-doomCanvas->viewSin) * 64) >> 16;
```

It deliberately does **not** execute the following second `Game_executeTile()`, does not perform the final durable `DoomCanvas_checkFacingEntity()`, does not mutate the legacy canvas/renderer, and does not enter `ST_PLAYING`.

## Exact current Junction result

For the hardware-proven fresh-map orientation:

```text
destAngle=64
sinTable[64]  = 65536
sinTable[128] = 0
viewSin       = 65536
viewCos       = 0
viewStepX     = 0
viewStepY     = -64
```

The permanent owner derives these exact 16.16 fixed-point values without consulting legacy `Render_t`. The temporary hardware probe additionally reads `Render.sinTable[64]` and `[128]` **read-only** and requires exact equality with the native result.

Only angle 64 is enabled by this milestone. Other directions fail closed until deliberately promoted. The permanent API is otherwise map-generic: it requires matching fresh-map player/view and initial-tile identities rather than hard-coding Junction IDs.

## Permanent owner

Files:

```text
ESP32/include/esp_player_orientation_state.h
ESP32/src/esp_player_orientation_state.c
```

Candidate ABI:

```text
EspPlayerOrientationState = 24 B
persistent heap = 0 B
```

State fields:

```text
int32 viewSin
int32 viewCos
int32 viewStepX
int32 viewStepY
uint8 targetMapId
uint8 gameplayLoadMapId
uint8 loadType
uint8 destAngle
uint8 prepared
uint8 active
uint8 reserved[2]
```

Candidate Junction state fingerprint:

```text
stateFNV=acc754a6
```

`acc754a6` is a **predicted candidate fingerprint**, not a hardware canon until the real CYD proves it.

## API

```text
EspPlayerOrientation_reset()
EspPlayerOrientation_isReady()
EspPlayerOrientation_view()
EspPlayerOrientation_prepare()
EspPlayerOrientation_route()
```

`prepare()` is pure and zeroes caller output on refusal. `route()` parks only the new 24-byte owner. It does not consume or mutate `EspPlayerViewState`; the post-initial-tile player/view canon must therefore remain exactly `1bd0f09b`.

## Ordering contract

Required input order:

```text
placement                         [hardware-proven]
HUD dirty                         [hardware-proven]
transient old-vector facing       [deliberately unowned]
Player_setup                      [hardware-proven]
initial Game_executeTile          [hardware-proven]
finishRotation orientation prep   [THIS MILESTONE]
second Game_executeTile           [deferred]
final durable facing              [deferred]
ST_PLAYING                        [deferred]
```

The orientation route requires:

```text
active/spawned PlayerView
fresh loadType=0
hudRefreshPending=0
playerSetupPending=0
tileEnterPending=0
facingRefreshPending=1
matching active InitialTile owner
viewAngle == destAngle == 64
```

## Hardware probe

Temporary probe files:

```text
ESP32/include/native_junction_orientation_probe.h
ESP32/src/native_junction_orientation_probe.c
```

The normal `esp32-cyd` lifecycle chains the probe after `Esp32JunctionInitialTileProbe` completes. It arms on one Arduino loop and executes on the next.

Expected PASS markers:

```text
=== Doom RPG ESP32-native Junction finishRotation orientation ===
[JUNCTIONROTATE] READY ...
[JUNCTIONROTATE] PLAYER ... unchanged=yes ...
[JUNCTIONROTATE] ORDER ... secondTileDeferred=yes finalFacingDeferred=yes ...
[JUNCTIONROTATE] FAILCLOSED ...
[JUNCTIONROTATE] RESIDENT ... unchanged=yes ...
[JUNCTIONROTATE] RAM ... delta=0 ...
[JUNCTIONROTATE] LEGACY ... Mutation=no ...
[JUNCTIONROTATE] PARK ... nativeOrientation=yes ... ST_PLAYING=no ...
```

The READY line must prove:

```text
stateBytes=24
stateFNV=acc754a6        # candidate expectation
angle=64
viewSin=65536
viewCos=0
viewStepX=0
viewStepY=-64
legacySin=65536
legacyCos=0
legacyStepX=0
legacyStepY=-64
exact=yes
```

## Fail-closed proof

The probe exercises at least:

```text
null PlayerView
null InitialTile
null output
inactive PlayerView
tileEnterPending unexpectedly restored
missing facing pending
unsupported angle
inactive InitialTile
mismatched map/load identity
repeat route
```

Every refusal must leave the permanent owner zero/unmodified as appropriate, preserve PlayerView and InitialTile byte-for-byte, and leave the resident snapshot unchanged.

## Mandatory integrity

The probe requires:

```text
Junction snapshotFNV=bc9071e9
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

Same-build free heap/largest-block, framebuffer and legacy Game/Player/DoomCanvas/Render witnesses must remain exactly unchanged.

Permanent invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
runtime ZIP map access forbidden
legacy Game.entities = 0
legacy Game.monsters = 0
ST_PLAYING not reached
```

## Promotion rule

Do not mark this milestone hardware-proven until the normal `esp32-cyd` Serial log proves exact native-vs-legacy orientation equality, the 24-byte owner, zero allocation delta, unchanged PlayerView/InitialTile/resident state, and no legacy/framebuffer mutation.

After PASS, update this archive, `PORTING_STATUS.md` and `DOCUMENTATION.md` using the exact Serial values. Every commit after the flashed firmware SHA must then remain documentation-only.

## Next boundary after PASS

The next exact recovered operation is the second `Game_executeTile()` inside `DoomCanvas_finishRotation()`:

```text
Game_executeTile(destX, destY, DoomCanvas_flagForFacingDir() | 0x400)
```

Keep the final durable `checkFacingEntity()` and `ST_PLAYING` as later milestones unless the fresh legacy audit proves otherwise.
