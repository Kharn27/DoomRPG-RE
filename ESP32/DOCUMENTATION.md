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

Older milestone archives remain in this directory and are indexed by Git history. `PORTING_STATUS.md` is the preferred recovery point.

## Current candidate

[`MAP1_NATIVE_COMMITTED_TRANSITION.md`](MAP1_NATIVE_COMMITTED_TRANSITION.md) introduces a permanent stats-ack-gated point-of-no-return transition owner and a hardware proof that first forces post-teardown recovery, then commits Junction and deliberately leaves it resident.

```text
branch = agent/esp32-native-committed-transition
base   = fddae899fd7dc01b20cf6bd532489326380954e3
firmware candidate = 759b7f05a7c1940e98caf68e4041faa69b34cfc9
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

### Permanent API

```text
EspMapCommittedTransitionState = 24 B

EspMapCommittedTransition_reset
EspMapCommittedTransition_isCommitted
EspMapCommittedTransition_begin
EspMapCommittedTransition_ackStats
EspMapCommittedTransition_commit
```

The state is pointer-free and adds zero persistent heap.

Phases:

```text
EMPTY -> WAIT_STATS -> READY -> COMMITTED
                         |
                         +-> target failure -> ROLLED_BACK / FAILED
```

`begin()` consumes the real caller-owned `EspMapChangeMapState` only after all pending/result/stats/preflight relationships validate. This mirrors legacy `Game_changeMap()` clearing `changeMapParam` when it schedules the stats menu.

The actual map replacement is not allowed before explicit `ackStats()`, matching the recovered legacy flow where accepting `MENU_MAP_STATS` later calls `DoomCanvas_loadMap(menu.mapNameId)`.

### Transactional commit

Before any source teardown:

```text
source inventory == live runtime
target inventory == preflight-bound bytes/CRC/FNV/gameplay ID
PAK closed
source resident capture valid
```

Then the permanent state machine reuses the already-proven resident lifecycle:

```text
resetAll(source)
 -> loadFromEmpty(target)
```

Success leaves Junction resident and marks COMMITTED. Failure after teardown attempts immediate source reconstruction and reports ROLLED_BACK or FAILED.

### Candidate state fingerprints

Static predictions for real Entrance -> Junction:

```text
WAIT_STATS  = 66fe636a
READY       = 0ef58ea8
ROLLED_BACK = 2dec1442
COMMITTED   = 2c595a62
```

### Hardware probe sequence

```text
canonical Entrance
 -> real EV_CHANGEMAP event 1 / offset 1
 -> LEVEL stats intent
 -> Junction preflight
 -> source + target inventories
 -> invalid begin atomicity
 -> begin consumes pending / WAIT_STATS
 -> pre-ACK commit refused
 -> ACK / READY
 -> bad target fingerprint refused before teardown
 -> deliberately corrupt target plan
 -> source teardown
 -> target runtime failure
 -> automatic Entrance recovery
 -> exact source snapshot/heap/largest restoration
 -> true commit
 -> Junction stays resident
```

An intentional diagnostic line is expected during forced rollback:

```text
[MAPRT] FAILED unsupported plan/source
```

That line is only healthy if exact Entrance recovery follows before the final successful Junction build.

### Final PASS target

Inherited Junction canons:

```text
snapshotFNV=bc9071e9
payload=10410 B
actual resident heap=10540 B
runtime/map/script/line/texture/automap/topology FNVs:
bc432a0f / c5cdfc04 / bc9b18ff / 3658710d / 537319ad / 0b2ae445 / d6e8df7d
compact entities=30 enemies=0 destructibles=3
```

Expected final boundary:

```text
mapSwapCommitted=yes
sourceMap=1
targetMap=9
junctionResident=yes
sourceRestored=no
targetLeftResident=yes
pendingConsumed=yes
statsAck=yes
spawnParam=0 retained
spawnPending=yes
spawnApplied=no
legacy DoomCanvas/Game/Menu/Player/Render unchanged
legacy Game.entities=0
legacy Game.monsters=0
ST_INTRO page=3
ST_PLAYING=no
framebuffer unchanged
```

The unchanged intro frame with Junction native owners underneath is intentional. Rendering/gameplay are separate future consumers.

## Hardware-proven boundary through PR #67

```text
Entrance resident heap = 18008 B
Entrance snapshotFNV   = b3811f3d
Junction resident heap = 10540 B
Junction snapshotFNV   = bc9071e9
catalogFNV             = ce322e3f
preflightFNV           = 108e5c7b
statsMenuIntentFNV     = 96afe901
levelExitStatsFNV      = bd41bcfa
playerExitAppliedFNV   = 298eaaa4

all MAP_INTRO opcode families owned=yes
reversible Entrance -> Junction -> Entrance=yes
final heap drift=0
largest-block fragmentation=0
legacy entities=0
legacy monsters=0
ST_PLAYING not reached
shapeData=NULL
mediaTexels=NULL
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
 -> reversible full resident handoff          [hardware-proven]
 -> committed stats-ack-gated transition      [candidate]
 -> spawn/loadType ownership
 -> native gameplay/render loop
```

Still outside current candidate:

```text
actual stats-menu rendering/input
spawn/loadType handoff
native player position owner
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

Build/flash candidate with normal `esp32-cyd`. No CI status is published and no local build/hardware PASS is claimed.
