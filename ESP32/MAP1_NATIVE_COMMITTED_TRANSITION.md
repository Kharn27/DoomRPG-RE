# ESP32 native committed Junction transition milestone

Branch: `agent/esp32-native-committed-transition`

Base merged `main`:

```text
PR   = #67 — native reversible resident handoff
main = fddae899fd7dc01b20cf6bd532489326380954e3
```

Hardware-tested firmware content:

```text
759b7f05a7c1940e98caf68e4041faa69b34cfc9
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

PR #67 proved that the complete native resident owner set can perform:

```text
Entrance -> EMPTY -> Junction -> EMPTY -> Entrance
```

with exact source restoration, zero final heap drift and zero largest-block fragmentation.

This milestone opens the next permanent boundary: a transition state machine that owns the stats acknowledgement and the destructive point of no return. A successful commit deliberately leaves Junction resident instead of restoring Entrance.

Recovered legacy `Game_changeMap()` with `showStats=1` performs two distinct moments:

```text
Game_changeMap()
 -> resolve target
 -> apply exit stats
 -> select MENU_MAP_STATS
 -> clear changeMapParam

later, user accepts stats menu
 -> DoomCanvas_loadMap(menu.mapNameId)
```

The native equivalent therefore explicitly models `WAIT_STATS` before destructive residency replacement.

Still excluded:

```text
actual stats-menu rendering/input
legacy DoomCanvas_loadMap()
ST_LOADING / ST_PLAYING
player spawn placement
loadType ownership
legacy Game/Render entity population
native gameplay/rendering
```

## Permanent state machine

Files:

```text
ESP32/include/esp_map_committed_transition.h
ESP32/src/esp_map_committed_transition.c
```

Caller-owned state:

```text
EspMapCommittedTransitionState = 24 B
persistent heap = 0 B
```

Fields bind the transition to the exact preflighted target:

```text
targetSourceBytes
targetSourceCrc32
targetSourceFNV1a
spawnParam
sourceMapId
targetMapId
targetGameplayLoadMapId
menuKind
phase
pendingConsumed
statsAcknowledged
committed
```

No pointers, map-local string spans or inventories survive in this state.

Phases:

```text
EMPTY
WAIT_STATS
READY
COMMITTED
ROLLED_BACK
FAILED
```

API:

```text
EspMapCommittedTransition_reset()
EspMapCommittedTransition_isCommitted()
EspMapCommittedTransition_begin()
EspMapCommittedTransition_ackStats()
EspMapCommittedTransition_commit()
```

Hardware confirms status `8` on the final commit, which is permanent enum value `ESP_MAP_COMMITTED_TRANSITION_OK`.

## Pending consumption

`begin()` consumes the existing caller-owned `EspMapChangeMapState` only after all source/result/stats/preflight relationships validate.

Real Entrance command:

```text
event=1
offset=1
globalCommand=2
arg1=80000000
mapStringIndex=0
mapName=/junction.bsp
targetMap=9
spawnParam=0
showStats=1
pending=1
```

Successful native begin therefore does:

```text
pending active
 -> copy durable scalar transition data
 -> pending reset/consumed
 -> phase WAIT_STATS
```

This matches legacy `Game_changeMap()` clearing `changeMapParam` when it schedules the stats menu, before the later menu acknowledgement loads the map.

Invalid begin is atomic and leaves both transition state and pending owner unchanged.

## Stats acknowledgement

For the real show-stats path:

```text
WAIT_STATS
 -> ackStats()
 -> READY
```

A commit attempt before acknowledgement fails closed and preserves Entrance exactly.

Repeated acknowledgement in READY is an exact no-op.

The probe supplies the acknowledgement directly. It does not claim that stats UI rendering/input exists yet.

## Commit transaction

`commit()` accepts caller-owned source and target `EspBspInventory` values that were produced before destructive teardown.

Before `resetAll()` it verifies:

```text
state == READY
PAK closed
source runtime matches source inventory
target inventory matches preflight-bound bytes/CRC/FNV/gameplayLoadMapId
resident source capture succeeds
```

Only then:

```text
EspMapResidentLifecycle_resetAll()
 -> loadFromEmpty(target)
```

Target success:

```text
phase=COMMITTED
committed=1
Junction remains resident
```

Target failure after source destruction:

```text
reset partial target
 -> loadFromEmpty(source)
 -> phase=ROLLED_BACK if recovery succeeds
 -> phase=FAILED if source recovery also fails
```

The pending CHANGEMAP remains consumed after rollback, matching legacy timing: the transition parameter had already been cleared before the later load attempt.

## Real target canons

Junction source identity:

```text
resourceMapId=9
resource=/junction.bsp
gameplayLoadMapId=2
entryOffset=1974397
bytes=21051
crc32=4a2c5800
sourceFNV=fefaf5ca
spawnIndex=943
spawnDirection=64
```

Junction native resident snapshot:

```text
snapshotBytes=96
snapshotFNV=bc9071e9
payload=10410 B
actual heap=10540 B
allocator overhead=130 B

runtime=8867
state=1024
script=73
line=52
texture=26
automap=32
topology=336
```

FNVs:

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

Topology/cardinality:

```text
nodes=77 lines=207 sprites=48 events=66 byteCodes=319 strings=126
compact entities=30 enemies=0 destructibles=3
```

Legacy `Game.entities` and `Game.monsters` remain zero.

## Hardware-proven state fingerprints

The real CYD exactly confirmed the static 24-byte little-endian predictions:

```text
WAIT_STATS  FNV = 66fe636a
READY       FNV = 0ef58ea8
ROLLED_BACK FNV = 2dec1442
COMMITTED   FNV = 2c595a62
```

This proves the permanent ABI and all four real transition phases used by the probe.

## Real-CYD forced rollback proof

The probe intentionally supplied a Junction inventory with correct target bytes/CRC/FNV/gameplay ID but a structurally invalid compact plan. This passed state-machine identity checks, released Entrance, then failed inside target runtime construction as intended:

```text
[MAPRT] FAILED unsupported plan/source
```

That line is expected PASS behavior in this milestone.

The state machine then rebuilt Entrance using the caller-owned source inventory. Hardware proved:

```text
forced=1
phase=ROLLED_BACK
stateFNV=2dec1442
sourceRestored=yes
snapshotFNV=b3811f3d
heap8=65584->65584
largest8=34804->34804
packClosed=yes
```

Therefore the recovery path is proven after the destructive point has actually been crossed.

## Real-CYD final committed swap

After the rollback proof, a fresh READY state performed the true target commit.

Hardware output:

```text
status=8 / ESP_MAP_COMMITTED_TRANSITION_OK
phase=COMMITTED
committed=1
committedStateFNV=2c595a62
targetSnapshotFNV=bc9071e9
payload=10410
targetHeapGain=7468
sourceHeap=65584
targetHeap=73052
largest=34804->34804
packClosed=yes
```

The resident-cost equation matched exactly:

```text
Entrance heap cost = 18008 B
Junction heap cost = 10540 B
expected free-heap gain = 18008 - 10540 = 7468 B
observed gain = 73052 - 65584 = 7468 B
```

The final native target fingerprints all matched the previously proven Junction canons:

```text
arena=bc432a0f
map=c5cdfc04
script=bc9b18ff
line=3658710d
texture=537319ad
automap=0b2ae445
topology=d6e8df7d
entities=30
enemies=0
destructibles=3
```

Most importantly, there is no final source restoration:

```text
junctionResident=yes
sourceRestored=no
targetLeftResident=yes
mapSwapCommitted=yes
```

This is the first hardware-proven committed resident map replacement in the native port.

## Fail-closed / atomic gates

Hardware proved:

```text
invalidBegin=1
preAckCommit=1
badInventory=1
repeatCommit=1
stateAtomic=yes
sourcePreservedBeforeCommit=yes
repeatAck=1
```

The pre-ACK and bad-inventory paths cannot tear down Entrance. Repeated final commit is refused without mutating the committed state.

## Pending / stats boundary

Real hardware begin/ACK output:

```text
stateBytes=24
sourceMap=1
targetMap=9
gameplayLoadMapId=2
spawnParam=0
menuKind=1 / LEVEL
pendingConsumed=1
phase=WAIT_STATS
waitStateFNV=66fe636a
preflightFNV=108e5c7b
statsIntentFNV=96afe901
```

then:

```text
statsAcknowledged=1
phase=READY
readyStateFNV=0ef58ea8
repeatAck=1
```

The Serial `BEGIN` line prints `menuKind=1pendingConsumed=1` without a separating space. This is formatting-only; both numeric values are unambiguous and the full state FNV matches exactly.

## Legacy / framebuffer integrity

The committed swap touches only native resident owners and the new native transition state.

Hardware same-probe witnesses:

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

`transitionFNV=95142f8f` and `frameFNV=b8924a47` are same-build/same-probe equality witnesses, not cross-build permanent canons.

The unchanged visible intro frame is intentional. Native resident lifecycle and gameplay presentation are now explicitly decoupled.

## Final PARK boundary

Real hardware proved:

```text
state=9
page=3
committedTransition=yes
mapSwapCommitted=yes
sourceMap=1
targetMap=9
junctionResident=yes
sourceRestored=no
targetLeftResident=yes
nativePayload=10410
persistentHeapProven=10540
pendingConsumed=yes
statsAck=yes
spawnPending=yes
spawnApplied=no
ST_PLAYING=no
legacy entities=0
legacy monsters=0
noGameplay=yes
```

Post-PARK heartbeat observed:

```text
uptime=40092 ms
heap=138816
heap8=73052
largest8=34804
SD=ready
VIDEO=ready
CORE=ready
```

The existing `ZIP=ready` heartbeat label does not imply runtime ZIP map access. Runtime backing remains `/DoomRPG-ESP32.pak`.

## Same-build inherited reversible-handoff witness

The same flashed firmware re-ran PR #67's reversible handoff immediately before the committed transition. It remained exact with this build's absolute heap baseline:

```text
SOURCE   heap8=65584
EMPTY1   heap8=83592
JUNCTION heap8=73052
EMPTY2   heap8=83592
RESTORED heap8=65584
largest8=34804 throughout
sourceCost=18008
junctionCost=10540
finalDelta=0
fragmentationDelta=0
```

The 8-byte absolute shift versus the earlier PR #67 firmware is build-context only. Resident costs, payloads, fingerprints and exact restoration remain unchanged.

## Hardware acceptance status

The real classic CYD proved:

```text
real pending CHANGEMAP consumed exactly once
WAIT_STATS gate
explicit stats ACK
pre-ACK fail closed
inventory fail closed
post-teardown forced rollback
byte-exact Entrance recovery
real Junction commit
Junction left resident
exact 7468 B resident-cost delta
largest block unchanged
all target FNVs exact
PAK closed
legacy runtime untouched
framebuffer untouched
spawn/loadType untouched
ST_PLAYING not reached
```

This milestone is **REAL-CYD HARDWARE PASS / MERGE-READY**.

Hardware-tested firmware content:

```text
759b7f05a7c1940e98caf68e4041faa69b34cfc9
```

Every later commit on this branch must remain documentation-only unless another firmware is flashed.

## Boundary after PASS

The native engine now owns a durable point-of-no-return transition and can leave Junction as the active resident map while all legacy gameplay/render state remains untouched.

Next bounded milestone after merge should own **native player spawn/load semantics** using:

```text
retained CHANGEMAP spawnParam=0
Junction BSP spawnIndex=943
Junction BSP spawnDirection=64
gameplayLoadMapId=2
```

That milestone should still avoid opening full gameplay or `ST_PLAYING` until the exact legacy `Game_spawnPlayer()` / loadType semantics are re-audited.

## Merge recommendation

```text
MERGE agent/esp32-native-committed-transition
```
