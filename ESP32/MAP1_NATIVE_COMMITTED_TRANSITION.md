# ESP32 native committed Junction transition milestone

Branch: `agent/esp32-native-committed-transition`

Base merged `main`:

```text
PR   = #67 — native reversible resident handoff
main = fddae899fd7dc01b20cf6bd532489326380954e3
```

Firmware candidate:

```text
759b7f05a7c1940e98caf68e4041faa69b34cfc9
```

Status: **IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING**.

## Objective

PR #67 proved that the complete native resident owner set can perform:

```text
Entrance -> EMPTY -> Junction -> EMPTY -> Entrance
```

with exact source restoration, zero final heap drift and zero largest-block fragmentation.

This milestone opens the next permanent boundary: a transition state machine that owns the stats acknowledgement and the destructive point of no return. A successful commit deliberately leaves Junction resident instead of restoring Entrance.

Legacy `Game_changeMap()` with `showStats=1` performs two distinct moments:

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

## Pending consumption

`begin()` consumes the existing caller-owned `EspMapChangeMapState` only after all source/result/stats/preflight relationships validate.

Real Entrance command already hardware-proven:

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

A commit attempt before acknowledgement must fail closed and preserve Entrance exactly.

Repeated acknowledgement in READY is an exact no-op.

The probe supplies the acknowledgement directly. It does not pretend that stats UI rendering/input exists yet.

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

## Real target canons inherited from PR #67

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

Legacy `Game.entities` and `Game.monsters` must remain zero.

## State fingerprints

Static 24-byte little-endian predictions for the real Entrance -> Junction path:

```text
WAIT_STATS  FNV = 66fe636a
READY       FNV = 0ef58ea8
ROLLED_BACK FNV = 2dec1442
COMMITTED   FNV = 2c595a62
```

These are candidate predictions until the real CYD confirms them.

## Temporary hardware probe

Files:

```text
ESP32/include/native_committed_transition_probe.h
ESP32/src/native_committed_transition_probe.c
```

The probe arms only after the hardware-proven reversible resident-handoff probe has restored Entrance.

Sequence:

```text
capture canonical Entrance
 -> reconstruct real event 1 / command offset 1 EV_CHANGEMAP
 -> prepare real LEVEL stats intent
 -> preflight resource map 9 / Junction
 -> inventory Entrance + Junction
 -> prove invalid begin atomicity
 -> valid begin consumes pending -> WAIT_STATS
 -> prove pre-ACK commit cannot tear down source
 -> ACK -> READY
 -> prove bad target fingerprint inventory rejected before teardown
 -> force structurally invalid target plan after validation
 -> source teardown occurs
 -> target runtime build fails
 -> automatic Entrance recovery
 -> require exact source snapshot/heap/largest restoration
 -> true READY commit
 -> Junction full resident build
 -> leave Junction resident at PARK
```

The forced rollback inventory preserves the real target bytes/CRC/FNV/gameplay ID but corrupts one compact-plan field. This intentionally passes the state-machine prevalidation and fails inside `EspMapRuntime_loadPackEntry()` only after source release, exercising the permanent recovery path.

## Strict acceptance

Before final commit:

```text
Entrance snapshotFNV=b3811f3d
pending real EV_CHANGEMAP active
preflightFNV=108e5c7b
statsIntentFNV=96afe901
```

State machine:

```text
sizeof(state)=24
WAIT_STATS=66fe636a
READY=0ef58ea8
ROLLED_BACK=2dec1442
COMMITTED=2c595a62
pendingConsumed=1
statsAcknowledged=1 before commit
```

Forced rollback:

```text
source restored snapshotFNV=b3811f3d
heap == pre-rollback source heap
largest == pre-rollback source largest
PAK closed
```

Final commit:

```text
Junction snapshotFNV=bc9071e9
Junction payload=10410 B
source heap -> target heap gain = 18008 - 10540 = 7468 B
largest target == largest source
PAK closed
second target capture byte-exact
repeat commit refused without mutation
```

Legacy/frame boundary:

```text
framebuffer unchanged
legacy Player witness unchanged
legacy transition/menu witness unchanged
legacy Render runtime clear
DoomCanvas_loadMapCalled=no
menuMutation=no
legacyPlayerMutation=no
spawnApplied=no
loadTypeMutation=no
Game.entities=0
Game.monsters=0
ST_INTRO page=3
```

Final PARK intentionally differs from all earlier resident probes:

```text
mapSwapCommitted=yes
sourceMap=1
targetMap=9
junctionResident=yes
sourceRestored=no          # deliberate success condition
targetLeftResident=yes
pendingConsumed=yes
statsAck=yes
spawnPending=yes
spawnApplied=no
ST_PLAYING=no
```

The visible intro framebuffer is intentionally unchanged. This milestone proves resident ownership and transition sequencing, not gameplay presentation.

## Expected Serial family

```text
[COMMITTRANSITIONPROBE] ARMED ...

=== Doom RPG ESP32-native committed Junction transition ===
[COMMITTRANSITIONPROBE] CONTRACT ...

[BSPREAD] ... Junction preflight/inventories ...
[MAPRT] FAILED unsupported plan/source   # intentional forced rollback target
[MAPRT] ... Entrance recovery ...
[MAPRT] ... Junction final commit ...

[COMMITTRANSITION] BEGIN ...
[COMMITTRANSITION] ACK ...
[COMMITTRANSITION] GATES ...
[COMMITTRANSITION] ROLLBACK ...
[COMMITTRANSITION] COMMIT ...
[COMMITTRANSITION] TARGETFNV ...
[COMMITTRANSITION] LEGACY ...
[COMMITTRANSITION] PARK ...
[ALIVE] ...
```

Normal build environment:

```text
esp32-cyd
```

No CI status is published for the candidate and no local build or hardware PASS is claimed.

## Boundary after PASS

A PASS would establish for the first time that the native engine has a durable point-of-no-return transition owner and can leave Junction as the active resident map while all legacy gameplay/render state remains untouched.

The next bounded milestone should own **spawn/loadType semantics** using the already retained `spawnParam` plus Junction BSP header values (`spawnIndex=943`, `spawnDirection=64`) before opening `ST_PLAYING` or native gameplay rendering.
