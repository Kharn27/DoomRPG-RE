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
| [`MAP1_NATIVE_HUD_REFRESH.md`](MAP1_NATIVE_HUD_REFRESH.md) | post-spawn HUD dirty ownership and corrected facing order | #71 | `02b7f143a12e6df86ada094af10ef580ad572aad` |
| [`MAP1_NATIVE_PLAYER_SETUP.md`](MAP1_NATIVE_PLAYER_SETUP.md) | fresh-map Player_setup semantic owner | #72 | `9077ae4496bdcc06b6b99846332ab43b38943a8a` |
| [`MAP1_NATIVE_INITIAL_TILE_ENTER.md`](MAP1_NATIVE_INITIAL_TILE_ENTER.md) | first fresh-map Game_executeTile dispatch | #73 | `0bc171affad8416ed1a7918a4a67fd4d53d61efe` |

Older milestone archives remain indexed by Git history. `PORTING_STATUS.md` is the preferred recovery point.

## Latest merged boundary

PR #73 hardware-proved the first fresh-map tile dispatch at Junction:

```text
EspPlayerInitialTileState=24 B
stateFNV=f73e28b2
world=992/1888
tile=943
flags=1000040f
eventFound=1
eventIndex=61
eligible=0
executed=0
removed=0
script FNV bc9b18ff -> bc9b18ff

PlayerView FNV c21fba3c -> 1bd0f09b
tileEnterPending=0
facingRefreshPending=1
persistent heap=0 B
```

Real-CYD RAM baseline:

```text
heap=138632
heap8=72868
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

[`MAP1_NATIVE_FINISH_ROTATION_ORIENTATION.md`](MAP1_NATIVE_FINISH_ROTATION_ORIENTATION.md) defines the next bounded candidate.

```text
branch = agent/esp32-native-finish-rotation-orientation
base   = 0bc171affad8416ed1a7918a4a67fd4d53d61efe
status = HARDWARE CANDIDATE — NOT YET CYD-PROVEN
```

### Exact recovered boundary

At the start of `DoomCanvas_finishRotation()` the legacy engine performs exactly:

```text
viewSin   = sinTable[destAngle & 255]
viewCos   = sinTable[(destAngle + 64) & 255]
viewStepX = (viewCos * 64) >> 16
viewStepY = ((-viewSin) * 64) >> 16
```

Only after those four writes does it execute the second tile event and then the durable facing query.

For the current hardware-proven Junction orientation:

```text
destAngle=64
sinTable[64]=65536
sinTable[128]=0
viewSin=65536
viewCos=0
viewStepX=0
viewStepY=-64
```

### Permanent candidate owner

```text
ESP32/include/esp_player_orientation_state.h
ESP32/src/esp_player_orientation_state.c

EspPlayerOrientationState = 24 B
persistent heap = 0 B

EspPlayerOrientation_reset
EspPlayerOrientation_isReady
EspPlayerOrientation_view
EspPlayerOrientation_prepare
EspPlayerOrientation_route
```

The owner is map-generic but this milestone enables only angle 64. It requires the active fresh-map `PlayerView` and `InitialTile` identities to match, requires `tileEnterPending=0` and `facingRefreshPending=1`, and never mutates PlayerView.

Predicted candidate Junction fingerprint:

```text
OrientationState FNV=acc754a6
```

`acc754a6` is not yet a hardware canon.

### Hardware probe

```text
ESP32/include/native_junction_orientation_probe.h
ESP32/src/native_junction_orientation_probe.c
```

The probe runs only after the hardware-proven initial-tile probe. It compares the native owner against the live legacy `Render.sinTable[64]`/`[128]` read-only and requires:

```text
native viewSin=65536 == legacySin
native viewCos=0     == legacyCos
native viewStepX=0   == legacyStepX
native viewStepY=-64 == legacyStepY
exact=yes
```

It also proves PlayerView and InitialTile remain byte-for-byte unchanged, the full Junction resident snapshot remains unchanged, framebuffer and legacy Game/Player/Hud/DoomCanvas/Render witnesses are stable, the PAK remains closed, and same-build free heap/largest block do not move.

The second `Game_executeTile()` and final `checkFacingEntity()` remain strictly deferred.

## Hardware-proven canons through PR #73

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
 -> finishRotation orientation preparation       [CURRENT CANDIDATE]
 -> second tile dispatch
 -> durable facing query
 -> ST_PLAYING / native gameplay/render loop
```

Still outside the current hardware baseline:

```text
actual stats-menu rendering/input
actual HUD rendering / renderer dirty consumption
weapon restore/select ownership when disabledWeapons!=0
finishRotation orientation until candidate PASS
second tile event
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

Return the complete `JUNCTIONROTATE` Serial block. Promote documentation to hardware PASS only after the real CYD proves exact native-vs-legacy orientation equality and zero mutation/allocation regressions.
