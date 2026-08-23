# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #66 — native transition preflight
main = 9f981f490282200f216aef66d22608d2244beb00
hardware-tested preflight firmware = 4d78a66548fab6373c06c67f107f176fc3988b1c
```

Merged evidence: [`MAP1_NATIVE_TRANSITION_PREFLIGHT.md`](MAP1_NATIVE_TRANSITION_PREFLIGHT.md).

## Current merge-ready milestone

```text
branch = agent/esp32-native-resident-handoff
base   = 9f981f490282200f216aef66d22608d2244beb00
hardware-tested firmware = 090d7dac5c255fc42a3d12fb3441053fdefe681b
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Active evidence: [`MAP1_NATIVE_RESIDENT_HANDOFF.md`](MAP1_NATIVE_RESIDENT_HANDOFF.md).

Validation history:

```text
v1 f71520281254ff9d0b2d5e4be1b3611e29ca87c4
   safe FAIL before teardown at combined source guard

v2 13f7f3787bf6de08595167081af1ea628ae30946
   safe diagnostic FAIL; isolated logical-payload vs heap-cost confusion

v3 090d7dac5c255fc42a3d12fb3441053fdefe681b
   REAL-CYD HARDWARE PASS
```

The permanent lifecycle itself was correct throughout: `EspMapResidentSnapshot.runtimeArenaBytes` is logical payload. The faulty probe expectation used Entrance runtime heap cost `14112` instead of runtime payload `14095`.

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
entities    = 0
monsters    = 0
ST_PLAYING  = not reached
```

## MAP_INTRO / Entrance canon

```text
resource=/intro.bsp
name=Entrance
bytes=21823 crc32=623f34e4 gameplayLoadMapId=1
nodes=223 lines=480 mapSprites=344 events=93 byteCodes=265
strings=94 stringData=7779 maxString=313
spawn=904 direction=64 camera=648 floorTex=145 ceilingTex=112
```

All real MAP_INTRO opcode IDs have native ownership/execution boundaries:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

## Hardware-proven Entrance resident RAM

Actual heap cost:

```text
immutable arena          14112 B
mutable tile state        1040 B
mutable script state       100 B
mutable line state         136 B
mutable texture state       76 B
mutable automap state      120 B
mutable sprite topology   2424 B
-------------------------------
total                    18008 B
```

Logical payload captured by the resident lifecycle:

```text
runtime arena       14095 B
map state            1024 B
script state           81 B
line state            120 B
texture state          60 B
automap state         103 B
topology              2408 B
---------------------------
payload total        17891 B
allocator overhead     117 B
actual heap total    18008 B
```

## Hardware-proven Entrance fingerprints

```text
residentSnapshotFNV       = b3811f3d
arenaFNV                  = c3882516
mapStateFNV               = cd99b98e
scriptFNV                 = f9e3d9df
lineStateFNV              = e5e74861
lineTextureStateFNV       = f1fc1875
automapStateFNV           = 669b1aa7
spriteTopologyFNV         = 3f321e43
saveRouteOwnerFNV         = 06ea6ea8
saveRouteResultFNV        = c2ecb064
changeMapOwnerFNV         = f75eb7c7
changeMapResultFNV        = 2f40c9be
showResultFNV             = 6029eb3c
hideResultFNV             = d24f5bae
contextAfterShowFNV       = 2de723aa
contextAfterHideFNV       = bb1d78a4
legacyEntityTopologyFNV   = f8f9b485
levelExitStatsFNV         = bd41bcfa
levelExitNoStatsFNV       = d9532169
levelExitMapId2FNV        = ceb6ad21
levelExitShowSensFNV      = 5155b517
levelExitSecretOpenFNV    = 6694b0e1
playerExitInitialFNV      = 940b0171
playerExitAppliedFNV      = 298eaaa4
playerExitResultFNV       = 5d10a566
playerExitAllMasksFNV     = c93e8128
statsMenuIntentFNV        = 96afe901
statsMenuEndGameFNV       = deea91b4
statsMenuZeroFNV          = 4b95f515
catalogFNV                = ce322e3f
transitionPreflightFNV    = 108e5c7b
junctionSourceFNV         = fefaf5ca
```

Same-build framebuffer FNV is only an equality witness, not a cross-build canon.

## Hardware-proven CHANGEMAP exit chain

Real MAP_INTRO command:

```text
name=/junction.bsp
targetMap=9 / MAP_JUNCTION
spawnParam=0
showStats=1
effects=03
pending=1
```

Native consumer chain:

```text
CHANGEMAP pending intent
 -> EspMapLevelExitStats = 20 B, FNV=bd41bcfa
 -> EspPlayerExitState = 28 B, appliedFNV=298eaaa4
 -> EspStatsMenuIntent = 4 B, target=9 kind=LEVEL FNV=96afe901
 -> immutable 13-map catalog, FNV=ce322e3f
 -> EspMapTransitionPreflightResult = 56 B, FNV=108e5c7b
      resourceMapId=9 gameplayLoadMapId=2
 -> explicit resident lifecycle
      Entrance -> EMPTY -> Junction -> EMPTY -> Entrance
```

No legacy `Player_addLevelStats()`, `Game_changeMap()`, menu mutation or `DoomCanvas_loadMap()` is called by this native chain.

## Hardware-proven Junction target source

Critical identity split:

```text
resourceMapId      = 9 / /junction.bsp
gameplayLoadMapId  = 2
hubProgressionGate = 1
```

Source/preflight:

```text
entryOffset=1974397
bytes=21051
crc32=4a2c5800
sourceFNV=fefaf5ca
name=Junction
spawn=943 direction=64 camera=0
floorTex=117 ceilingTex=151
nodes=77 lines=207 mapSprites=48 events=66 byteCodes=319 strings=126
stringData=12235 maxString=380 trailing=0
persistentPlanBytes=8867
readCalls=83 window=256 B
preflightResultFNV=108e5c7b
```

## Permanent resident lifecycle

Files/API:

```text
ESP32/include/esp_map_resident_lifecycle.h
ESP32/src/esp_map_resident_lifecycle.c

EspMapResidentLifecycle_resetAll()
EspMapResidentLifecycle_isEmpty()
EspMapResidentLifecycle_isReady()
EspMapResidentLifecycle_capture()
EspMapResidentLifecycle_loadFromEmpty()
```

`loadFromEmpty()` is intentionally non-destructive. A live owner set returns `NOT_EMPTY` before PAK I/O. Destruction is always an explicit `resetAll()`.

Teardown:

```text
topology -> automap -> texture -> line -> script -> map state -> runtime
```

Build from EMPTY:

```text
runtime -> map state -> script -> line -> texture -> automap -> topology
```

The lifecycle owns one temporary PAK session, uses `/entities.db` for topology, closes PAK before return, preserves caller ownership on `PACK_BUSY`, and returns EMPTY on partial-build failure.

## Hardware-proven reversible resident handoff

Fail-closed gates:

```text
notEmpty=1
invalid=1
nullCapture=1
packBusy=1
busyZero=1
callerOwnsPack=1
emptyAtomic=yes
```

Entrance release:

```text
SOURCE heap8=65592 largest8=34804
EMPTY1 heap8=83600 largest8=34804
released=18008
sourcePayload=17891
allocatorOverhead=117
allOwnersEmpty=yes
```

### Full Junction resident owner set

```text
EspMapResidentSnapshot = 96 B
snapshotFNV = bc9071e9
elapsed     = 121 ms

runtime   = 8867 B
state     = 1024 B
script    = 73 B
line      = 52 B
texture   = 26 B
automap   = 32 B
topology  = 336 B
----------------
payload   = 10410 B
heapCost  = 10540 B
overhead  = 130 B
```

Resident FNVs:

```text
junctionRuntimeFNV  = bc432a0f
junctionMapStateFNV = c5cdfc04
junctionScriptFNV   = bc9b18ff
junctionLineFNV     = 3658710d
junctionTextureFNV  = 537319ad
junctionAutomapFNV  = 0b2ae445
junctionTopologyFNV = d6e8df7d
junctionSnapshotFNV = bc9071e9
```

Topology/cardinalities:

```text
nodes=77
lines=207
sprites=48
events=66
byteCodes=319
strings=126
entities=30
enemies=0
destructibles=3
```

Junction resident RAM:

```text
heap8=73060
largest8=34804
```

### Target release and source restoration

```text
EMPTY2 heap8=83600 largest8=34804
emptyExact=1
targetReleased=10540
fragmentationDelta=0

RESTORED snapshotFNV=b3811f3d
exact=1
heap8=65592->65592
largest8=34804->34804
```

Whole RAM sequence:

```text
SOURCE   65592 / 34804
EMPTY1   83600 / 34804
JUNCTION 73060 / 34804
EMPTY2   83600 / 34804
RESTORED 65592 / 34804

sourceCost   = 18008
junctionCost = 10540
finalDelta   = 0
```

The largest free 8-bit block remains `34804` throughout the complete destructive/rebuild round-trip.

## Legacy / final integrity

```text
playerFNV     0b2ae445 -> 0b2ae445
transitionFNV f450c49f -> f450c49f
frameFNV      ee9d9dbc -> ee9d9dbc
legacyRuntimeClear=yes
sourceTeardownNativeOnly=yes
DoomCanvas_loadMapCalled=no
menuMutation=no
legacyPlayerMutation=no
```

Final PARK:

```text
state=9 page=3
nativeResidentLifecycle=yes
reversibleHandoff=yes
junctionResidentProven=yes
sourceRestored=yes
targetLeftResident=no
packClosed=yes
persistentBytes=18008
mapSwapCommitted=no
entities=0 monsters=0 noGameplay=yes
```

Stable post-PASS heartbeat:

```text
heap=131356 heap8=65592 largest8=34804
```

Same-run pre-Start-Game heartbeats were stable at `heap=93948 heap8=28184 largest8=18420`; these are lifecycle context, not resident-map canons.

## Current architecture boundary

Hardware-proven ownership now includes:

```text
compact immutable native Entrance + explicit mutable owners
all 16 MAP_INTRO opcode families
SAVEGAME route
CHANGEMAP intent
SHOW/HIDE compact topology
level-exit stats
player exit-state
stats-menu intent
generic 13-map catalog
Junction target PAK/BSP preflight
resourceMapId vs gameplayLoadMapId semantics
generic explicit resident lifecycle
complete temporary Junction resident build
exact reversible Entrance -> Junction -> Entrance handoff
no-fragmentation resident round-trip
```

Still intentionally outside:

```text
committed Junction residency
native transition point-of-no-return state machine
spawn/loadType handoff
actual stats-menu rendering/input
full native entity/monster gameplay
ST_PLAYING progression
native gameplay renderer
sound playback
```

The next bounded milestone should own a **committed native transition state machine** using the proven resident lifecycle rather than reimplementing teardown/build order.

## Merge recommendation

```text
MERGE agent/esp32-native-resident-handoff
```

Hardware-tested firmware is `090d7dac5c255fc42a3d12fb3441053fdefe681b`. Every later commit must remain documentation-only until merge.
