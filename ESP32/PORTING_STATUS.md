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

## Current merge-ready milestone

```text
branch = agent/esp32-native-committed-transition
base   = fddae899fd7dc01b20cf6bd532489326380954e3
hardware-tested firmware = 759b7f05a7c1940e98caf68e4041faa69b34cfc9
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Active evidence: [`MAP1_NATIVE_COMMITTED_TRANSITION.md`](MAP1_NATIVE_COMMITTED_TRANSITION.md).

This milestone introduces the first permanent point-of-no-return transition owner. It consumes the real Entrance EV_CHANGEMAP pending state, waits for explicit stats acknowledgement, proves fail-closed pre-teardown gates, forces and recovers from a post-teardown target failure, then performs the real commit and deliberately leaves Junction resident.

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

## Hardware-proven exit / transition chain

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

Native chain now hardware-proven end-to-end through committed residency:

```text
CHANGEMAP pending intent
 -> level-exit stats 20 B / FNV bd41bcfa
 -> player exit-state 28 B / applied FNV 298eaaa4
 -> stats-menu intent 4 B / target 9 LEVEL / FNV 96afe901
 -> immutable 13-map catalog / FNV ce322e3f
 -> target preflight 56 B / FNV 108e5c7b
 -> explicit resident lifecycle
 -> reversible Entrance -> Junction -> Entrance proof
 -> committed transition state machine 24 B
 -> real Entrance -> Junction committed residency
```

No legacy `Player_addLevelStats()`, `Game_changeMap()`, menu mutation or `DoomCanvas_loadMap()` is called by this native chain.

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

Full native resident snapshot:

```text
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

Same-build reversible proof immediately before the committed transition:

```text
SOURCE   heap8=65584 largest8=34804
EMPTY1   heap8=83592 largest8=34804
JUNCTION heap8=73052 largest8=34804
EMPTY2   heap8=83592 largest8=34804
RESTORED heap8=65584 largest8=34804
sourceCost=18008
junctionCost=10540
finalDelta=0
fragmentationDelta=0
```

The absolute 8-byte shift versus the PR #67 firmware is build-context only; payloads, resident costs and fingerprints are unchanged.

## Permanent committed transition

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

Hardware-proven state FNVs:

```text
WAIT_STATS  = 66fe636a
READY       = 0ef58ea8
ROLLED_BACK = 2dec1442
COMMITTED   = 2c595a62
```

## Hardware proof: begin / ACK / gates

Real CYD:

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

ACK:

```text
statsAcknowledged=1
phase=READY
readyStateFNV=0ef58ea8
repeatAck=1
```

Fail-closed / atomic gates:

```text
invalidBegin=1
preAckCommit=1
badInventory=1
repeatCommit=1
stateAtomic=yes
sourcePreservedBeforeCommit=yes
```

The Serial `BEGIN` line lacks one space between `menuKind=1` and `pendingConsumed=1`; this is formatting-only.

## Hardware proof: forced post-teardown rollback

The probe intentionally corrupts the target compact plan while keeping target bytes/CRC/FNV/gameplay ID valid. The expected target construction failure occurs only after Entrance teardown:

```text
[MAPRT] FAILED unsupported plan/source
```

Automatic recovery then proves:

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

This is a real post-point-of-no-return recovery proof, not a preflight rejection.

## Hardware proof: final committed Junction residency

Final commit:

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

Exact resident-cost delta:

```text
Entrance actual heap=18008 B
Junction actual heap=10540 B
free-heap gain=7468 B
```

Target FNVs all match Junction canon:

```text
arena=bc432a0f
map=c5cdfc04
script=bc9b18ff
line=3658710d
texture=537319ad
automap=0b2ae445
topology=d6e8df7d
entities=30 enemies=0 destructibles=3
```

Final PARK:

```text
state=9 / ST_INTRO
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
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

This is the first hardware-proven committed native resident map replacement.

## Legacy / framebuffer integrity

Same-probe equality witnesses:

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

`transitionFNV=95142f8f` and `frameFNV=b8924a47` are same-build witnesses, not cross-build canons.

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

The heartbeat's existing `ZIP=ready` label does not mean runtime map ZIP access. Runtime backing remains `/DoomRPG-ESP32.pak`.

## Current architecture boundary

Hardware-proven ownership now includes:

```text
compact immutable native Entrance + explicit mutable owners
all 16 MAP_INTRO opcode families
SAVEGAME route
CHANGEMAP pending transition intent
SHOW/HIDE compact topology
native level-exit stats
native player exit-state
native stats-menu semantic intent
generic 13-map catalog
Junction target PAK/BSP preflight
explicit resident lifecycle
reversible Entrance -> Junction -> Entrance handoff
24 B committed transition state machine
real pending consumption
explicit stats ACK gate
post-teardown automatic Entrance recovery
committed Entrance -> Junction native resident map swap
Junction left resident with 30 compact entities
```

Still intentionally outside:

```text
actual stats-menu rendering/input
spawn/loadType ownership
native player position owner
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

`shapeData == NULL` and `mediaTexels == NULL` remain mandatory.

## Next direction after merge

The next bounded milestone should re-audit exact legacy `Game_spawnPlayer()` / `loadType` semantics and own native player placement using the already retained transition data:

```text
spawnParam=0
Junction BSP spawnIndex=943
Junction BSP spawnDirection=64
gameplayLoadMapId=2
```

Do not open full gameplay or `ST_PLAYING` merely because Junction is now resident.

## Merge recommendation

```text
MERGE agent/esp32-native-committed-transition
```

Hardware-tested firmware is:

```text
759b7f05a7c1940e98caf68e4041faa69b34cfc9
```

Every later commit on this branch must remain documentation-only unless another firmware is flashed.
