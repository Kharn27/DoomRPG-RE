# ESP32 documentation map

This file defines the current ESP32 CYD documentation map.

## Source of truth

- [`README.md`](README.md): stable build/flash guide.
- [`PORTING_STATUS.md`](PORTING_STATUS.md): authoritative current recovery point.
- Milestone archives: detailed implementation and hardware evidence.

## Recent merged milestones

| Archive | Purpose | PR | Merged `main` |
| --- | --- | ---: | --- |
| [`MAP1_NATIVE_CHANGE_MAP_INTENT.md`](MAP1_NATIVE_CHANGE_MAP_INTENT.md) | CHANGEMAP pending transition intent | #61 | `fc39ac60757e0d992e3729a5044a9d83e9994971` |
| [`MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md`](MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md) | SHOW/HIDE compact topology; all MAP_INTRO opcode families owned | #62 | `ed5cd9a09c9ae36f999661f4284f64400681b1af` |
| [`MAP1_NATIVE_LEVEL_EXIT_STATS.md`](MAP1_NATIVE_LEVEL_EXIT_STATS.md) | map-derived level-exit stats | #63 | `533784b5483e14a12558fb08c9331d8b744caa88` |
| [`MAP1_NATIVE_PLAYER_EXIT_STATE.md`](MAP1_NATIVE_PLAYER_EXIT_STATE.md) | pointer-free player exit writes | #64 | `3759bcd12a3f6d36a6a696457110ab27474c24b8` |
| [`MAP1_NATIVE_STATS_MENU_INTENT.md`](MAP1_NATIVE_STATS_MENU_INTENT.md) | LEVEL/OVERALL stats-menu intent | #65 | `c8679133351fa00e01a67103386b7676660c4a6e` |
| [`MAP1_NATIVE_TRANSITION_PREFLIGHT.md`](MAP1_NATIVE_TRANSITION_PREFLIGHT.md) | map catalog + Junction PAK/BSP preflight | #66 | `9f981f490282200f216aef66d22608d2244beb00` |
| [`MAP1_NATIVE_RESIDENT_HANDOFF.md`](MAP1_NATIVE_RESIDENT_HANDOFF.md) | reversible full resident Entrance/Junction handoff | #67 | `fddae899fd7dc01b20cf6bd532489326380954e3` |
| [`MAP1_NATIVE_COMMITTED_TRANSITION.md`](MAP1_NATIVE_COMMITTED_TRANSITION.md) | transactional committed Entrance -> Junction resident swap | #68 | `00268a100c6662cb883f9a02d979b4f29eecbf12` |

Older milestone archives remain in this directory and are indexed by Git history. `PORTING_STATUS.md` is the preferred recovery point.

## Latest merged boundary

PR #68 hardware-proved:

```text
EspMapCommittedTransitionState = 24 B
WAIT_STATS FNV  = 66fe636a
READY FNV       = 0ef58ea8
ROLLED_BACK FNV = 2dec1442
COMMITTED FNV   = 2c595a62

mapSwapCommitted=yes
sourceMap=1
targetMap=9
junctionResident=yes
sourceRestored=no
targetLeftResident=yes
spawnParam=0 retained
spawnApplied=no
ST_PLAYING=no
```

Junction remains the active compact native resident map:

```text
snapshotFNV=bc9071e9
payload=10410 B
actual heap=10540 B
runtime/map/script/line/texture/automap/topology FNVs:
bc432a0f / c5cdfc04 / bc9b18ff / 3658710d / 537319ad / 0b2ae445 / d6e8df7d
compact entities=30 enemies=0 destructibles=3
```

Legacy `Game.entities`, `Game.monsters` and legacy Render runtime remain untouched.

## Current candidate

[`MAP1_NATIVE_JUNCTION_SPAWN.md`](MAP1_NATIVE_JUNCTION_SPAWN.md) introduces a permanent pointer-free projection of recovered `Game_spawnPlayer()` placement for the already committed Junction map.

```text
branch = agent/esp32-native-junction-spawn
base   = 00268a100c6662cb883f9a02d979b4f29eecbf12
firmware candidate = 08a3a29c5e4e4a64000fa12a877299bbb1e772a0
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

### Permanent API

```text
EspPlayerSpawnState = 24 B expected classic-ESP32 ABI
persistent heap = 0 B

EspPlayerSpawn_reset
EspPlayerSpawn_prepareCommitted
```

The API requires a COMMITTED transition plus a complete target inventory matching the current resident runtime.

It supports only the ordinary fresh-map load context:

```text
loadType=0
gameIsLoaded=0
```

Saved-game restoration remains fail-closed.

### Recovered placement semantics

When `spawnParam == 0`:

```text
x = mapSpawnIndex % 32
y = mapSpawnIndex / 32
angle = mapSpawnDir
```

When `spawnParam != 0`:

```text
x = spawnParam & 31
y = (spawnParam >> 5) & 31
angle = (spawnParam >> 10) & 255
```

Common projected placement:

```text
worldX = x*64 + 32
worldY = y*64 + 32
viewZ=36
viewZOld=4
```

The candidate does **not** execute the next legacy effects:

```text
DoomCanvas_checkFacingEntity
Player_setup
initial Game_executeTile
ST_PLAYING
```

Instead the native state records:

```text
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
```

### Real Junction prediction

Committed transition:

```text
spawnParam=0
```

Junction header:

```text
spawnIndex=943
spawnDirection=64
```

Expected state:

```text
tileIndex=943
tile=15/29
world=992/1888
angle=64
viewZ=36
viewZOld=4
spawnSource=HEADER
loadType=0
```

Static 24-byte FNV prediction:

```text
ba6af4a7
```

This remains a candidate value until real-CYD confirmation.

### Synthetic packed-override prediction

Probe-local committed transition copy:

```text
spawnParam=00030167
x=7
y=11
angle=192
tileIndex=359
world=480/736
spawnSource=OVERRIDE
```

Static FNV prediction:

```text
e0a5110b
```

The real committed transition remains unchanged.

### Hardware probe acceptance

The temporary probe runs after committed Junction residency and must prove:

```text
real stateBytes=24
real stateFNV=ba6af4a7
real tile=15/29
real world=992/1888
real angle=64

override stateFNV=e0a5110b
override tile=7/11
override world=480/736
override angle=192

fresh loadType=0
saved/loaded contexts refused
all invalid inputs fail closed with zero output

Junction snapshot bc9071e9 unchanged
heap8 delta=0
largest8 delta=0
PAK closed
framebuffer unchanged
legacy placement fields unchanged
legacy Player unchanged
legacy Render runtime clear
legacy entities=0
legacy monsters=0
spawnApplied=no
ST_PLAYING=no
```

Expected final PARK:

```text
committedTransition=yes
mapSwapCommitted=yes
targetMap=9
junctionResident=yes
nativeSpawnState=yes
spawnProjected=yes
spawnApplied=no
loadType=0
facingPending=yes
playerSetupPending=yes
tileEnterPending=yes
ST_PLAYING=no
entities=0
monsters=0
noGameplay=yes
```

## Hardware-proven canons inherited by candidate

```text
Entrance snapshotFNV=b3811f3d
Entrance heap=18008 B
Junction snapshotFNV=bc9071e9
Junction heap=10540 B
Junction sourceFNV=fefaf5ca
catalogFNV=ce322e3f
preflightFNV=108e5c7b
statsMenuIntentFNV=96afe901
levelExitStatsFNV=bd41bcfa
playerExitAppliedFNV=298eaaa4
committedTransitionFNV=2c595a62
```

## Architecture direction

```text
original Doom RPG behavior/data
 -> native pack-backed parsers
 -> compact immutable map + explicit mutable owners
 -> native event/script ownership
 -> exit chain
 -> map catalog/preflight
 -> explicit resident lifecycle
 -> reversible full resident handoff              [hardware-proven]
 -> committed stats-ack-gated transition          [hardware-proven]
 -> native fresh-map spawn/load projection        [candidate]
 -> native player/view + facing/setup/tile enter
 -> native gameplay/render loop
```

Still outside candidate:

```text
actual stats-menu rendering/input
application of projected spawn coordinates
native facing-entity query
Player_setup-equivalent native initialization
initial tile-enter execution
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

`shapeData == NULL` and `mediaTexels == NULL` remain mandatory.

Build/flash the current candidate with the normal `esp32-cyd` environment. No local build or hardware PASS is claimed.
