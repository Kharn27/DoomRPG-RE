# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #75 — native finishRotation second tile
main = 7a0e57cf13d02320be3a238dc73499a023c9f04c
hardware-tested firmware = df4f62687d99eb3b3e9569ae6861b6909d59c82d
status = REAL-CYD HARDWARE PASS
```

Merged evidence: [`MAP1_NATIVE_FINISH_ROTATION_SECOND_TILE.md`](MAP1_NATIVE_FINISH_ROTATION_SECOND_TILE.md).

## Current hardware candidate

```text
branch = agent/esp32-native-durable-facing
base   = 7a0e57cf13d02320be3a238dc73499a023c9f04c
status = HARDWARE CANDIDATE — NOT YET CYD-PROVEN
```

Candidate: [`MAP1_NATIVE_DURABLE_FACING.md`](MAP1_NATIVE_DURABLE_FACING.md).

This candidate owns only final durable `DoomCanvas_checkFacingEntity()` at the
end of recovered `DoomCanvas_finishRotation()`. `ST_PLAYING` remains deferred.

## Permanent invariants

```text
board       = ESP32-2432S028R classic CYD
MCU         = ESP32-D0WD-V3 dual core 240 MHz
flash       = 4 MB
PSRAM       = none
framebuffer = 160x120 RGB565 = 38400 B
shapeData   = NULL
mediaTexels = NULL
runtime ZIP = forbidden
backing     = /DoomRPG-ESP32.pak
legacy Game.entities = 0
legacy Game.monsters = 0
ST_PLAYING  = not reached
```

## Hardware-proven map canons

Entrance:

```text
resource=/intro.bsp
bytes=21823
crc32=623f34e4
sourceFNV=d5cc751f
gameplayLoadMapId=1
spawnIndex=904
spawnDirection=64
snapshotFNV=b3811f3d
logical payload=17891 B
actual heap=18008 B
```

Junction:

```text
resourceMapId=9 / /junction.bsp
gameplayLoadMapId=2
sourceBytes=21051
crc32=4a2c5800
sourceFNV=fefaf5ca
spawnIndex=943
spawnDirection=64
snapshotFNV=bc9071e9
payload=10410 B
actual heap=10540 B
entities=30
enemies=0
destructibles=3
```

Resident owner FNVs:

```text
runtime  = bc432a0f
map      = c5cdfc04
script   = bc9b18ff
line     = 3658710d
texture  = 537319ad
automap  = 0b2ae445
topology = d6e8df7d
```

## Hardware-proven transition/player chain

```text
CHANGEMAP pending intent
 -> level-exit stats
 -> native player exit-state
 -> LEVEL stats-menu semantic intent
 -> immutable 13-map catalog
 -> Junction transition preflight
 -> resident lifecycle / committed swap
 -> 24 B fresh-map spawn projection
 -> 44 B active player/view owner
 -> 8 B HUD dirty owner
 -> 24 B Player_setup session owner
 -> 24 B initial tile owner
 -> 24 B finishRotation orientation owner
 -> 24 B finishRotation second-tile owner
```

Canonical fingerprints:

```text
levelExitStatsFNV              = bd41bcfa
playerExitAppliedFNV           = 298eaaa4
statsMenuIntentFNV             = 96afe901
catalogFNV                     = ce322e3f
transitionPreflightFNV         = 108e5c7b
committed WAIT_STATS FNV       = 66fe636a
committed READY FNV            = 0ef58ea8
committed ROLLBACK FNV         = 2dec1442
committed COMMITTED FNV        = 2c595a62
Junction spawn FNV             = ba6af4a7
packed override FNV            = e0a5110b
Junction player/view FNV       = d1131d18
packed override view FNV       = 9ed47d08
post-HUD player/view FNV       = d17fa0d1
Junction HUD refresh FNV       = 6965ee06
Player_setup semantic FNV      = 3b27c6a1
post-setup player/view FNV     = c21fba3c
Junction initial-tile FNV      = f73e28b2
post-initial-tile player FNV   = 1bd0f09b
Junction orientation FNV       = acc754a6
Junction second-tile FNV       = 09e58e0d
```

Generic `EspMapOpcodeExecutor` remains intentionally only 11/19/20. All real
MAP_INTRO opcode families already have dedicated native boundaries.

## Latest hardware boundary

Current real-CYD player owners before durable facing:

```text
PlayerView FNV=1bd0f09b
world=992/1888
angle=64
hudRefreshPending=0
facingRefreshPending=1
playerSetupPending=0
tileEnterPending=0

InitialTile FNV=f73e28b2
Orientation FNV=acc754a6
SecondTile FNV=09e58e0d
```

Both exact fresh-map tile dispatches hit event 61 but have no eligible commands:

```text
first tile  flags=0x1000040f -> eligible=0 executed=0 removed=0
second tile flags=0x10000400 -> eligible=0 executed=0 removed=0
script FNV bc9b18ff -> bc9b18ff
```

Latest hardware RAM baseline:

```text
heap=138560
heap8=72796
largest8=34804
persistentHeapBytes=0
```

## Recovered durable facing semantics

Exact legacy final operation:

```text
DoomCanvas_checkFacingEntity()
```

At Junction angle 64:

```text
traceStart=(992,1857)
traceEnd=(992,1665)
traceFlags=0x0001f6ff
tiles=(15,29)->(15,28)->(15,27)->(15,26)
```

Recovered final selection chooses the first `Game_trace()` result for which:

```text
eType == 14
OR Entity.info & 0x00200000    # line entity
OR Entity.info == 0            # wall sentinel
OR sprite tile differs from trace-start tile
```

Ordinary entities on the start tile are skipped. Types 14/15 enter the trace
only when their oriented wall-sprite plane actually crosses the segment.

## Current durable-facing candidate

Permanent spatial helper:

```text
ESP32/include/esp_map_topology_query.h
ESP32/src/esp_map_topology_query.c
EspMapTopologyQuery_findLinkedOnTile()
```

It walks linked map-sprite entities in compact `nextOnTile` order and returns
tri-state found/empty/invalid. No allocation or legacy pointers.

Permanent owner:

```text
ESP32/include/esp_player_facing_state.h
ESP32/src/esp_player_facing_state.c
EspPlayerFacingState = 32 B
persistent heap = 0 B
```

API:

```text
EspPlayerFacing_reset()
EspPlayerFacing_isReady()
EspPlayerFacing_view()
EspPlayerFacing_prepare()
EspPlayerFacing_route()
EspPlayerView_consumeFacing()
```

The resolver combines immutable native sprite/line geometry, compact sprite
entity topology, native line-open state and bounded `/entities.db` PAK reads for
line entity type/subtype. The PAK is closed before return.

Facing kinds:

```text
0 none
1 sprite
2 line
3 wall sentinel
```

No hit kind/index or facing-owner FNV is predicted before real hardware.

Only `facingRefreshPending` is consumed after a complete resolution. Candidate
post-facing PlayerView fingerprint:

```text
1bd0f09b -> afcdcf74
```

`afcdcf74` remains predicted until CYD proof.

Temporary probe:

```text
ESP32/include/native_junction_facing_probe.h
ESP32/src/native_junction_facing_probe.c
```

Expected hardware block begins:

```text
=== Doom RPG ESP32-native Junction durable facing ===
[JUNCTIONFACING] READY ...
```

It reports actual facing state FNV/kind/index/tile/type/subtype/identity and
requires input owners + resident snapshot unchanged, pack closed, heap delta 0,
legacy `Player.facingEntity` untouched and `ST_PLAYING=no`.

## Current hardware PARK

Before candidate execution:

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

Candidate successful PARK is expected to change only:

```text
nativeFacing=yes
facingPending=no
finishRotationComplete=yes
ST_PLAYING=no
```

## Still intentionally outside

```text
actual stats-menu rendering/input
actual HUD rendering / renderer dirty consumption
weapon restore/select ownership when disabledWeapons!=0
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

## Next action

Build/flash normal environment:

```text
esp32-cyd
```

Use the complete `[JUNCTIONFACING]` Serial block as hardware truth. Do not mark
this milestone merge-ready until the real CYD proves resolution, post-view
`afcdcf74`, closed PAK and zero owner/RAM/legacy regressions.
