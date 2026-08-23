# ESP32 documentation map

This file defines the current ESP32 CYD documentation map.

## Source of truth

- [`README.md`](README.md): stable build/flash guide.
- [`PORTING_STATUS.md`](PORTING_STATUS.md): authoritative current recovery point.
- Milestone archives: detailed implementation and hardware evidence/candidate contracts.

## Recent merged milestones

| Archive | Purpose | PR | Merged `main` |
| --- | --- | ---: | --- |
| [`MAP1_NATIVE_CHANGE_MAP_INTENT.md`](MAP1_NATIVE_CHANGE_MAP_INTENT.md) | CHANGEMAP pending transition intent | #61 | `fc39ac60757e0d992e3729a5044a9d83e9994971` |
| [`MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md`](MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md) | SHOW/HIDE compact topology; all MAP_INTRO opcode families bounded | #62 | `ed5cd9a09c9ae36f999661f4284f64400681b1af` |
| [`MAP1_NATIVE_LEVEL_EXIT_STATS.md`](MAP1_NATIVE_LEVEL_EXIT_STATS.md) | map-derived level-exit stats | #63 | `533784b5483e14a12558fb08c9331d8b744caa88` |
| [`MAP1_NATIVE_PLAYER_EXIT_STATE.md`](MAP1_NATIVE_PLAYER_EXIT_STATE.md) | pointer-free player exit writes | #64 | `3759bcd12a3f6d36a6a696457110ab27474c24b8` |
| [`MAP1_NATIVE_STATS_MENU_INTENT.md`](MAP1_NATIVE_STATS_MENU_INTENT.md) | LEVEL/OVERALL stats-menu intent | #65 | `c8679133351fa00e01a67103386b7676660c4a6e` |
| [`MAP1_NATIVE_TRANSITION_PREFLIGHT.md`](MAP1_NATIVE_TRANSITION_PREFLIGHT.md) | map catalog + Junction PAK/BSP preflight | #66 | `9f981f490282200f216aef66d22608d2244beb00` |
| [`MAP1_NATIVE_RESIDENT_HANDOFF.md`](MAP1_NATIVE_RESIDENT_HANDOFF.md) | reversible full resident Entrance/Junction handoff | #67 | `fddae899fd7dc01b20cf6bd532489326380954e3` |
| [`MAP1_NATIVE_COMMITTED_TRANSITION.md`](MAP1_NATIVE_COMMITTED_TRANSITION.md) | transactional committed Entrance -> Junction resident swap | #68 | `00268a100c6662cb883f9a02d979b4f29eecbf12` |
| [`MAP1_NATIVE_JUNCTION_SPAWN.md`](MAP1_NATIVE_JUNCTION_SPAWN.md) | fresh-map Junction spawn/load projection | #69 | `992f38374840113409e776fb82ce57ab014607e5` |
| [`MAP1_NATIVE_PLAYER_VIEW.md`](MAP1_NATIVE_PLAYER_VIEW.md) | permanent active Junction player/view owner | #70 | `8a82891bb8d9c62582170cc4b3b74d270849e77b` |
| [`MAP1_NATIVE_HUD_REFRESH.md`](MAP1_NATIVE_HUD_REFRESH.md) | post-spawn HUD dirty ownership | #71 | `02b7f143a12e6df86ada094af10ef580ad572aad` |
| [`MAP1_NATIVE_PLAYER_SETUP.md`](MAP1_NATIVE_PLAYER_SETUP.md) | fresh-map Player_setup semantic owner | #72 | `9077ae4496bdcc06b6b99846332ab43b38943a8a` |
| [`MAP1_NATIVE_INITIAL_TILE_ENTER.md`](MAP1_NATIVE_INITIAL_TILE_ENTER.md) | first fresh-map Game_executeTile dispatch | #73 | `0bc171affad8416ed1a7918a4a67fd4d53d61efe` |
| [`MAP1_NATIVE_FINISH_ROTATION_ORIENTATION.md`](MAP1_NATIVE_FINISH_ROTATION_ORIENTATION.md) | finishRotation orientation preparation | #74 | `2decae5067438dc1a2d9c29335cfc0cad5538645` |

Older milestone archives remain indexed by Git history. `PORTING_STATUS.md` is
the preferred recovery point.

## Latest merged boundary

PR #74 hardware-proved the four orientation writes at the start of
`DoomCanvas_finishRotation()`:

```text
EspPlayerOrientationState=24 B
stateFNV=acc754a6
destAngle=64
viewSin=65536
viewCos=0
viewStepX=0
viewStepY=-64
native-vs-legacy exact=yes
persistent heap=0 B
```

Input owners remain canonical:

```text
PlayerView FNV=1bd0f09b
InitialTile FNV=f73e28b2
facingRefreshPending=1
tileEnterPending=0
```

Real-CYD RAM baseline:

```text
heap=138592
heap8=72828
largest8=34804
```

Junction resident remains:

```text
snapshotFNV=bc9071e9
payload=10410 B
actual heap=10540 B
entities=30
enemies=0
destructibles=3
```

## Current hardware candidate

[`MAP1_NATIVE_FINISH_ROTATION_SECOND_TILE.md`](MAP1_NATIVE_FINISH_ROTATION_SECOND_TILE.md)
defines the next bounded candidate.

```text
branch = agent/esp32-native-finish-rotation-second-tile
base   = 2decae5067438dc1a2d9c29335cfc0cad5538645
status = HARDWARE CANDIDATE — NOT YET CYD-PROVEN
```

### Exact recovered boundary

After orientation preparation, legacy `DoomCanvas_finishRotation()` performs:

```text
Game_executeTile(destX, destY, DoomCanvas_flagForFacingDir() | 0x400)
```

For the current Junction state:

```text
world=992/1888
tile=943
destAngle=64
facing flag=0x10000000
input flags=0x10000400
```

The final durable `checkFacingEntity()` follows this call and remains outside
the candidate.

### Permanent candidate owner

```text
ESP32/include/esp_player_finish_rotation_tile.h
ESP32/src/esp_player_finish_rotation_tile.c

EspPlayerFinishRotationTileState = 24 B
persistent heap = 0 B

EspPlayerFinishRotationTile_reset
EspPlayerFinishRotationTile_isReady
EspPlayerFinishRotationTile_view
EspPlayerFinishRotationTile_prepare
EspPlayerFinishRotationTile_route
```

The candidate reuses the permanent allocation-free event lookup/descriptor,
script overlay and `Game_runEvent()` filtering semantics. It leaves PlayerView,
InitialTile and Orientation byte-for-byte unchanged.

### Deliberate executor boundary

The generic executor remains restricted to:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

The candidate preflights the entire filtered event before mutation. If the
second-tile `0x400` flag makes any unsupported opcode eligible, the correct
result is:

```text
[JUNCTIONTILE2] DEFERRED ... code=<id> arg1=<...> arg2=<...> failClosed=yes
```

with no live owner/script mutation. This is discovery, not PASS.

If all eligible commands are supported, route execution uses only the native
script overlay, applies recovered `arg2 & 0x200` removal there, rolls back on
failure and parks the 24-byte second-tile owner.

No candidate state FNV is guessed in advance; the real event result is recorded
from Serial.

### Hardware probe

```text
ESP32/include/native_junction_finish_rotation_tile_probe.h
ESP32/src/native_junction_finish_rotation_tile_probe.c
```

A complete candidate route begins with:

```text
=== Doom RPG ESP32-native Junction finishRotation second tile ===
[JUNCTIONTILE2] READY ...
```

and must prove:

```text
tile=943
flags=10000400
PlayerView FNV=1bd0f09b unchanged
InitialTile FNV=f73e28b2 unchanged
Orientation FNV=acc754a6 unchanged
facingRefreshPending=1
final facing deferred
zero same-build heap delta
no legacy/framebuffer mutation
non-script resident owners stable
```

Script FNV may change only if eligible supported state commands execute.

## Hardware-proven canons through PR #74

```text
Entrance snapshotFNV=b3811f3d
Entrance heap=18008 B
Junction snapshotFNV=bc9071e9
Junction heap=10540 B
Junction sourceFNV=fefaf5ca
catalogFNV=ce322e3f
committed COMMITTED FNV=2c595a62
JunctionSpawnFNV=ba6af4a7
JunctionPlayerViewFNV=d1131d18
postHudPlayerViewFNV=d17fa0d1
JunctionHudRefreshFNV=6965ee06
PlayerSetupSemanticFNV=3b27c6a1
postSetupPlayerViewFNV=c21fba3c
JunctionInitialTileFNV=f73e28b2
postInitialTilePlayerViewFNV=1bd0f09b
JunctionOrientationFNV=acc754a6
```

Junction resident owner FNVs:

```text
runtime=bc432a0f
map=c5cdfc04
script=bc9b18ff
line=3658710d
texture=537319ad
automap=0b2ae445
topology=d6e8df7d
```

## Architecture direction

```text
original Doom RPG behavior/data
 -> /DoomRPG-ESP32.pak backing store
 -> compact immutable native map                 [hardware-proven]
 -> explicit compact mutable overlays            [hardware-proven]
 -> native event semantic owners                 [hardware-proven by family]
 -> transition/resident lifecycle                [hardware-proven]
 -> native player spawn projection               [hardware-proven]
 -> permanent native player/view application     [hardware-proven]
 -> post-spawn HUD dirty ownership               [hardware-proven]
 -> Player_setup-equivalent session root         [hardware-proven]
 -> initial tile-enter dispatch                  [hardware-proven]
 -> finishRotation orientation preparation       [hardware-proven]
 -> finishRotation second tile dispatch          [CURRENT CANDIDATE]
 -> durable facing query
 -> ST_PLAYING / native gameplay/render loop
```

Current hardware PARK:

```text
state=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativePlayerView=yes
nativeInitialTile=yes
nativeOrientation=yes
secondTilePending=yes
finalFacingPending=yes
finishRotationComplete=no
ST_PLAYING=no
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

Still outside:

```text
actual stats-menu rendering/input
actual HUD rendering / renderer dirty consumption
weapon restore/select ownership when disabledWeapons!=0
second tile until candidate PASS
final native facing-entity query
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

Mandatory invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
runtime ZIP map access forbidden
legacy Game.entities = 0
legacy Game.monsters = 0
ST_PLAYING not reached
```

## Next test

Build and flash the normal environment:

```text
esp32-cyd
```

Return the complete `JUNCTIONTILE2` Serial block. Promote documentation to
hardware PASS only after a complete native route. If the probe reports
`DEFERRED`, implement that exact native opcode integration and re-test first.
