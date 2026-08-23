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

## Current merge-ready milestone

[`MAP1_NATIVE_COMMITTED_TRANSITION.md`](MAP1_NATIVE_COMMITTED_TRANSITION.md) introduces the first permanent stats-ack-gated point-of-no-return transition owner and hardware-proves both post-teardown recovery and a true committed Entrance -> Junction resident swap.

```text
branch = agent/esp32-native-committed-transition
base   = fddae899fd7dc01b20cf6bd532489326380954e3
hardware-tested firmware = 759b7f05a7c1940e98caf68e4041faa69b34cfc9
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

### Permanent API

```text
EspMapCommittedTransitionState = 24 B
persistent heap = 0 B

EspMapCommittedTransition_reset
EspMapCommittedTransition_isCommitted
EspMapCommittedTransition_begin
EspMapCommittedTransition_ackStats
EspMapCommittedTransition_commit
```

Phases:

```text
EMPTY -> WAIT_STATS -> READY -> COMMITTED
                         |
                         +-> target failure -> ROLLED_BACK / FAILED
```

`begin()` consumes the real caller-owned `EspMapChangeMapState` only after all pending/result/stats/preflight relationships validate. This mirrors legacy `Game_changeMap()` clearing `changeMapParam` when it schedules the stats menu.

The actual native map replacement is not allowed before explicit `ackStats()`, matching the recovered legacy flow where accepting `MENU_MAP_STATS` later calls `DoomCanvas_loadMap(menu.mapNameId)`.

### Hardware-proven transition-state ABI

The real CYD confirmed all predicted 24-byte FNVs exactly:

```text
WAIT_STATS  = 66fe636a
READY       = 0ef58ea8
ROLLED_BACK = 2dec1442
COMMITTED   = 2c595a62
```

Real begin:

```text
sourceMap=1
targetMap=9
gameplayLoadMapId=2
spawnParam=0
menuKind=LEVEL
pendingConsumed=1
phase=WAIT_STATS
preflightFNV=108e5c7b
statsIntentFNV=96afe901
```

ACK:

```text
statsAcknowledged=1
phase=READY
repeatAck=1
```

Fail-closed gates:

```text
invalidBegin=1
preAckCommit=1
badInventory=1
repeatCommit=1
stateAtomic=yes
sourcePreservedBeforeCommit=yes
```

### Hardware-proven post-teardown recovery

The probe deliberately corrupts only the target compact plan after validating target bytes/CRC/FNV/gameplay identity. Entrance is actually released and the target runtime intentionally emits:

```text
[MAPRT] FAILED unsupported plan/source
```

The permanent transaction then reconstructs Entrance exactly:

```text
phase=ROLLED_BACK
stateFNV=2dec1442
sourceRestored=yes
snapshotFNV=b3811f3d
heap8=65584->65584
largest8=34804->34804
packClosed=yes
```

### Hardware-proven committed Junction residency

The second READY transaction performs the real commit and deliberately leaves Junction resident:

```text
status=8 / ESP_MAP_COMMITTED_TRANSITION_OK
phase=COMMITTED
committed=1
committedStateFNV=2c595a62
targetSnapshotFNV=bc9071e9
payload=10410 B
sourceHeap=65584
targetHeap=73052
free-heap gain=7468 B
largest=34804->34804
packClosed=yes
```

Exact target FNVs:

```text
runtime  = bc432a0f
map      = c5cdfc04
script   = bc9b18ff
line     = 3658710d
texture  = 537319ad
automap  = 0b2ae445
topology = d6e8df7d
snapshot = bc9071e9
```

Target compact topology:

```text
entities=30
enemies=0
destructibles=3
```

Final resident boundary:

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
legacy Game.entities=0
legacy Game.monsters=0
ST_INTRO page=3
ST_PLAYING=no
```

This is the first hardware-proven committed native resident map replacement.

### Legacy / framebuffer integrity

Same-probe witnesses remained exact:

```text
playerFNV=0b2ae445->0b2ae445
transitionFNV=95142f8f->95142f8f
frameFNV=b8924a47->b8924a47
legacyRuntimeClear=yes
DoomCanvas_loadMapCalled=no
menuMutation=no
legacyPlayerMutation=no
spawnApplied=no
loadTypeMutation=no
```

The unchanged visible intro framebuffer with Junction native owners underneath is intentional. Residency, player placement and presentation are now explicit separate boundaries.

Post-PARK heartbeat observed:

```text
uptime=40092 ms
heap=138816
heap8=73052
largest8=34804
```

### Same-build resident cost proof

The prerequisite reversible handoff also reran successfully in the tested firmware:

```text
SOURCE   65584 / 34804
EMPTY1   83592 / 34804
JUNCTION 73052 / 34804
EMPTY2   83592 / 34804
RESTORED 65584 / 34804

Entrance cost=18008 B
Junction cost=10540 B
finalDelta=0
fragmentationDelta=0
```

The 8-byte absolute shift from the earlier PR #67 firmware is build-context only; resident costs and fingerprints remain identical.

## Hardware-proven boundary through current milestone

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
committedTransitionBytes = 24
committedTransitionFNV   = 2c595a62

all MAP_INTRO opcode families owned=yes
reversible Entrance -> Junction -> Entrance=yes
post-teardown rollback recovery=yes
committed Entrance -> Junction=yes
Junction left resident=yes
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
 -> committed stats-ack-gated transition      [hardware-proven]
 -> native spawn/loadType ownership
 -> native gameplay/render loop
```

Still outside:

```text
actual stats-menu rendering/input
spawn/loadType handoff
native player position owner
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

## Next bounded milestone after merge

Re-audit legacy `Game_spawnPlayer()` and `loadType` semantics, then own native player placement from:

```text
retained spawnParam=0
Junction BSP spawnIndex=943
Junction BSP spawnDirection=64
gameplayLoadMapId=2
```

Do not open full gameplay or `ST_PLAYING` as part of the same milestone unless the repo/legacy audit proves that boundary is unavoidable.

## Merge recommendation

```text
MERGE agent/esp32-native-committed-transition
```

Hardware-tested firmware:

```text
759b7f05a7c1940e98caf68e404467a54405ae
```

Note: the canonical full firmware SHA is `759b7f05a7c1940e98caf68e4041faa69b34cfc9`; the line above is intentionally not authoritative if truncated by a UI. Always use the full SHA from `PORTING_STATUS.md` / milestone evidence.
