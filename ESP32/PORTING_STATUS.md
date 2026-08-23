# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #67 — native reversible resident handoff
main = fddae899fd7dc01b20cf6bd532489326380954e3
hardware-tested firmware = 090d7dac5c255fc42a3d12fb3441053fdefe681b
status = REAL-CYD HARDWARE PASS
```

Merged evidence: [`MAP1_NATIVE_RESIDENT_HANDOFF.md`](MAP1_NATIVE_RESIDENT_HANDOFF.md).

## Current candidate

```text
branch = agent/esp32-native-committed-transition
base   = fddae899fd7dc01b20cf6bd532489326380954e3
firmware candidate = 759b7f05a7c1940e98caf68e4041faa69b34cfc9
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

Active evidence: [`MAP1_NATIVE_COMMITTED_TRANSITION.md`](MAP1_NATIVE_COMMITTED_TRANSITION.md).

The candidate introduces the first permanent point-of-no-return transition owner. It consumes the real Entrance EV_CHANGEMAP pending state, waits for an explicit stats acknowledgement, validates source/target inventories before destruction, proves automatic source recovery after an intentionally forced post-teardown target failure, then performs the real commit and deliberately leaves Junction resident.

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

## Entrance canon

```text
resource=/intro.bsp
name=Entrance
bytes=21823 crc32=623f34e4 gameplayLoadMapId=1
nodes=223 lines=480 mapSprites=344 events=93 byteCodes=265
strings=94 stringData=7779 maxString=313
spawn=904 direction=64 camera=648 floorTex=145 ceilingTex=112
```

All real MAP_INTRO opcode IDs have explicit native ownership/execution boundaries:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

Entrance resident snapshot / RAM:

```text
EspMapResidentSnapshot = 96 B
snapshotFNV=b3811f3d

runtime payload=14095
map state=1024
script=81
line=120
texture=60
automap=103
topology=2408
payload total=17891 B
actual heap=18008 B
allocator overhead=117 B
```

Entrance FNVs:

```text
arenaFNV       = c3882516
mapStateFNV    = cd99b98e
scriptFNV      = f9e3d9df
lineFNV        = e5e74861
textureFNV     = f1fc1875
automapFNV     = 669b1aa7
topologyFNV    = 3f321e43
snapshotFNV    = b3811f3d
```

## Hardware-proven exit/transition chain through PR #67

Real EV_CHANGEMAP:

```text
event=1 commandOffset=1 globalCommand=2
arg1=80000000
mapStringIndex=0
mapName=/junction.bsp
targetMap=9
spawnParam=0
showStats=1
effects=03
pending=1
```

Native chain:

```text
CHANGEMAP pending intent
 -> level-exit stats 20 B / FNV bd41bcfa
 -> player exit-state 28 B / applied FNV 298eaaa4
 -> stats-menu intent 4 B / target 9 LEVEL / FNV 96afe901
 -> immutable 13-map catalog / FNV ce322e3f
 -> target preflight 56 B / FNV 108e5c7b
 -> explicit resident lifecycle
 -> reversible Entrance -> Junction -> Entrance full-resident proof
```

No legacy `Player_addLevelStats()`, `Game_changeMap()`, menu mutation or `DoomCanvas_loadMap()` is called by the native chain.

## Junction canon

Resource/gameplay identity split:

```text
resourceMapId      = 9 / /junction.bsp
gameplayLoadMapId  = 2
hubProgressionGate = 1
```

Source:

```text
entryOffset=1974397
bytes=21051
crc32=4a2c5800
sourceFNV=fefaf5ca
name=Junction
spawnIndex=943
spawnDirection=64
camera=0
floorTex=117
ceilingTex=151
nodes=77 lines=207 mapSprites=48 events=66 byteCodes=319 strings=126
stringData=12235 maxString=380 trailing=0
runtimePlan=8867 B
```

Full native resident snapshot hardware-proven by PR #67:

```text
snapshotFNV=bc9071e9
payload=10410 B
actual heap=10540 B
allocator overhead=130 B
buildElapsed=121 ms

runtime=8867
state=1024
script=73
line=52
texture=26
automap=32
topology=336
```

Junction FNVs:

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

Compact topology:

```text
entities=30
enemies=0
destructibles=3
```

These are native compact entities; legacy `Game.entities` and `Game.monsters` remain zero.

## Permanent resident lifecycle

```text
EspMapResidentLifecycle_resetAll()
EspMapResidentLifecycle_isEmpty()
EspMapResidentLifecycle_isReady()
EspMapResidentLifecycle_capture()
EspMapResidentLifecycle_loadFromEmpty()
```

Rules:

```text
loadFromEmpty never tears down a live source
resetAll is the explicit destructive primitive
PACK_BUSY preserves caller ownership
partial target build returns lifecycle to EMPTY
```

Teardown:

```text
topology -> automap -> texture -> line -> script -> map state -> runtime
```

Build:

```text
runtime -> map state -> script -> line -> texture -> automap -> topology
```

PR #67 hardware round-trip:

```text
SOURCE   heap8=65592 largest8=34804
EMPTY1   heap8=83600 largest8=34804
JUNCTION heap8=73060 largest8=34804
EMPTY2   heap8=83600 largest8=34804
RESTORED heap8=65592 largest8=34804
sourceCost=18008
junctionCost=10540
finalDelta=0
fragmentationDelta=0
```

## Current permanent committed-transition candidate

Files:

```text
ESP32/include/esp_map_committed_transition.h
ESP32/src/esp_map_committed_transition.c
```

API:

```text
EspMapCommittedTransition_reset()
EspMapCommittedTransition_isCommitted()
EspMapCommittedTransition_begin()
EspMapCommittedTransition_ackStats()
EspMapCommittedTransition_commit()
```

State:

```text
EspMapCommittedTransitionState = 24 B
persistent heap = 0 B
```

Phases:

```text
EMPTY
WAIT_STATS
READY
COMMITTED
ROLLED_BACK
FAILED
```

`begin()` validates the real pending owner, decoded change result, stats intent and preflight target. On success it copies only durable scalar data and resets/consumes `EspMapChangeMapState`.

For show-stats transitions:

```text
begin -> WAIT_STATS
ackStats -> READY
commit before ACK -> refused with source untouched
```

`commit()` validates both inventories and the live source runtime before `resetAll()`. Target success leaves Junction resident and moves to COMMITTED. Target failure after destruction attempts automatic source reconstruction and reports ROLLED_BACK or FAILED.

Static state FNV predictions for real Entrance -> Junction:

```text
WAIT_STATS  = 66fe636a
READY       = 0ef58ea8
ROLLED_BACK = 2dec1442
COMMITTED   = 2c595a62
```

These are not hardware canons until candidate PASS.

## Candidate hardware proof

Temporary probe:

```text
ESP32/include/native_committed_transition_probe.h
ESP32/src/native_committed_transition_probe.c
```

Sequence:

```text
canonical Entrance resident
 -> reconstruct real event 1 / offset 1 EV_CHANGEMAP
 -> prepare target 9 LEVEL stats intent
 -> fresh target preflight + Entrance/Junction inventories
 -> invalid begin atomicity
 -> valid begin consumes pending -> WAIT_STATS
 -> pre-ACK commit refusal
 -> ACK -> READY + repeat-ACK no-op
 -> corrupted target CRC rejected before teardown
 -> structurally corrupt target plan passes identity precheck
 -> reset source
 -> forced target runtime failure
 -> automatic Entrance recovery
 -> exact snapshot/heap/largest restoration
 -> true final commit
 -> Junction stays resident
 -> repeat commit refused
```

Strict final target requirements:

```text
stateBytes=24
committedStateFNV=2c595a62
Junction snapshotFNV=bc9071e9
payload=10410
source->target heap gain=7468 B
largest target == largest source
PAK closed
```

Final PARK if PASS:

```text
state=ST_INTRO page=3
mapSwapCommitted=yes
sourceMap=1
targetMap=9
junctionResident=yes
sourceRestored=no          # deliberate success
 targetLeftResident=yes
pendingConsumed=yes
statsAck=yes
spawnParam=0 retained
spawnPending=yes
spawnApplied=no
DoomCanvas_loadMapCalled=no
loadTypeMutation=no
legacy Player/Menu/Game/Render unchanged
legacy Game.entities=0
legacy Game.monsters=0
ST_PLAYING=no
noGameplay=yes
```

The visible intro framebuffer is expected to remain unchanged even though the native resident owners underneath have switched to Junction.

## Validation

Build/flash normal environment:

```text
esp32-cyd
```

Exact firmware:

```text
branch = agent/esp32-native-committed-transition
firmware = 759b7f05a7c1940e98caf68e4041faa69b34cfc9
```

Capture:

```text
[COMMITTRANSITIONPROBE]
[COMMITTRANSITION]
[BSPREAD]
[MAPRT]
[MAPSTATE]
[MAPLINESTATE]
[MAPLINETEX]
[MAPAUTOMAP]
[ALIVE]
```

An intentional line is expected during the forced rollback proof:

```text
[MAPRT] FAILED unsupported plan/source
```

It is PASS behavior only if followed by exact Entrance recovery and then a successful final Junction commit.

No CI status is published. No local build or hardware PASS is claimed.

## Architecture boundary

Hardware-proven through PR #67:

```text
native Entrance resident runtime + mutable owners
all MAP_INTRO opcode families
exit stats + player exit state
stats-menu semantic intent
map catalog + target preflight
complete native Junction resident build
reversible no-fragmentation resident handoff
```

Candidate adds:

```text
pending transition consumption
explicit stats acknowledgement gate
transactional post-teardown source recovery
committed target residency / point of no return
```

Still outside:

```text
actual stats-menu rendering/input
spawn/loadType ownership
native player position owner
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

After candidate PASS and merge, the next bounded milestone should own `spawnParam` + Junction BSP spawn semantics before opening gameplay.
