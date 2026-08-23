# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #66 — native transition preflight
main = 9f981f490282200f216aef66d22608d2244beb00
hardware-tested preflight firmware = 4d78a66548fab6373c06c67f107f176fc3988b1c
```

Merged evidence: [`MAP1_NATIVE_TRANSITION_PREFLIGHT.md`](MAP1_NATIVE_TRANSITION_PREFLIGHT.md).

## Current candidate

```text
branch = agent/esp32-native-resident-handoff
base   = 9f981f490282200f216aef66d22608d2244beb00
firmware candidate = f71520281254ff9d0b2d5e4be1b3611e29ca87c4
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

Active evidence: [`MAP1_NATIVE_RESIDENT_HANDOFF.md`](MAP1_NATIVE_RESIDENT_HANDOFF.md).

The candidate adds a generic explicit resident-map lifecycle and a destructive-but-auto-restored `Entrance -> EMPTY -> Junction -> EMPTY -> Entrance` hardware proof. It does not call legacy `DoomCanvas_loadMap()`, does not commit Junction as the active gameplay map, and must PARK with Entrance restored.

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

## Hardware-proven current resident RAM

Actual classic-CYD heap cost:

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

Logical owner payloads used by the new lifecycle snapshot are:

```text
runtime arena       14112 B
map state            1024 B
script state           81 B
line state            120 B
texture state          60 B
automap state         103 B
topology              2408 B
---------------------------
payload total        17908 B
allocator overhead     100 B
actual heap total    18008 B
```

## Hardware-proven fingerprints

```text
arenaFNV                 = c3882516
mapStateFNV              = cd99b98e
scriptFNV                = f9e3d9df
lineStateFNV             = e5e74861
lineTextureStateFNV      = f1fc1875
automapStateFNV          = 669b1aa7
saveRouteOwnerFNV        = 06ea6ea8
saveRouteResultFNV       = c2ecb064
changeMapOwnerFNV        = f75eb7c7
changeMapResultFNV       = 2f40c9be
spriteTopologyFNV        = 3f321e43
showResultFNV            = 6029eb3c
hideResultFNV            = d24f5bae
contextAfterShowFNV      = 2de723aa
contextAfterHideFNV      = bb1d78a4
legacyEntityTopologyFNV  = f8f9b485
levelExitStatsFNV        = bd41bcfa
levelExitNoStatsFNV      = d9532169
levelExitMapId2FNV       = ceb6ad21
levelExitShowSensFNV     = 5155b517
levelExitSecretOpenFNV   = 6694b0e1
playerExitInitialFNV     = 940b0171
playerExitAppliedFNV     = 298eaaa4
playerExitResultFNV      = 5d10a566
playerExitAllMasksFNV    = c93e8128
playerExitLiveFNV        = 57fce418
statsMenuIntentFNV       = 96afe901
statsMenuEndGameFNV      = deea91b4
statsMenuZeroFNV         = 4b95f515
catalogFNV               = ce322e3f
transitionPreflightFNV   = 108e5c7b
junctionSourceFNV        = fefaf5ca
```

Current candidate static source snapshot target:

```text
EspMapResidentSnapshot = 96 B
Entrance snapshotFNV   = 97090c81
```

This whole-snapshot FNV is not hardware canon until the candidate passes.

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

Native consumers proven through target preflight:

```text
CHANGEMAP pending intent
 -> EspMapLevelExitStats = 20 B, FNV=bd41bcfa
      loadMapId=1 secrets=0/6 monsters=0/30 effects=1f
 -> EspPlayerExitState = 28 B, appliedFNV=298eaaa4
 -> EspStatsMenuIntent = 4 B, target=9 kind=LEVEL FNV=96afe901
 -> immutable 13-map catalog, FNV=ce322e3f
 -> EspMapTransitionPreflightResult = 56 B, FNV=108e5c7b
      resourceMapId=9 gameplayLoadMapId=2
```

No legacy `Player_addLevelStats()`, `Game_changeMap()`, menu mutation or map load is called by those stages.

## Hardware-proven Junction target

Critical identity split:

```text
resourceMapId      = 9 / /junction.bsp
gameplayLoadMapId  = 2
hubProgressionGate = 1
```

Target source:

```text
entryOffset=1974397
bytes=21051
crc32=4a2c5800
fnv1a=fefaf5ca
name=Junction
spawn=943 direction=64 camera=0
floorTex=117 ceilingTex=151
```

Structure / immutable compact plan:

```text
nodes=77 lines=207 mapSprites=48 events=66 byteCodes=319 strings=126
stringData=12235 maxString=380 trailing=0
persistentPlanBytes=8867
readCalls=83 window=256 B
```

Preflight was reproduced twice exactly:

```text
resultFNV=108e5c7b
repeatFNV=108e5c7b
repeatExact=1
```

## Current permanent resident lifecycle candidate

Files:

```text
ESP32/include/esp_map_resident_lifecycle.h
ESP32/src/esp_map_resident_lifecycle.c
```

API:

```text
EspMapResidentLifecycle_resetAll()
EspMapResidentLifecycle_isEmpty()
EspMapResidentLifecycle_isReady()
EspMapResidentLifecycle_capture()
EspMapResidentLifecycle_loadFromEmpty()
```

`loadFromEmpty()` is intentionally non-destructive. If any current owner is live it returns `NOT_EMPTY` before PAK I/O. The destructive transition point is always an explicit `resetAll()` call.

Reverse dependency teardown:

```text
topology -> automap -> texture -> line -> script -> map state -> runtime
```

Empty-only build owns one temporary PAK session and reconstructs:

```text
runtime -> map state -> script -> line -> texture -> automap -> topology
```

Topology uses `/entities.db` while the lifecycle owns the open PAK session. On any build failure all partially created owners are released, output is zero and the lifecycle returns EMPTY.

`PACK_BUSY` preserves caller ownership of an already-open PAK.

## Candidate reversible hardware proof

Temporary probe:

```text
ESP32/include/native_resident_handoff_probe.h
ESP32/src/native_resident_handoff_probe.c
```

Sequence:

```text
Entrance SOURCE capture
 -> NOT_EMPTY / invalid fail-closed gates
 -> inventory Entrance + Junction
 -> resetAll
 -> EMPTY1
 -> PACK_BUSY caller-ownership gate
 -> load Junction full resident set
 -> capture Junction twice exactly
 -> resetAll
 -> EMPTY2 == EMPTY1 heap/largest
 -> reload Entrance
 -> RESTORED == SOURCE byte-for-byte
```

If any failure occurs after releasing Entrance, the probe attempts immediate recovery by rebuilding `/intro.bsp` before PARK.

### Junction resident payload target

Derived from already-proven Junction counts plus current owner formulas:

```text
runtime=8867
state=1024
script=73
line=52
texture=26
automap=32
topology=336
payload=10410 B
```

Hardware must establish:

```text
actual target heap cost + allocator overhead
Junction runtime/map/script/line/texture/automap/topology FNVs
whole Junction snapshot FNV
entity/enemy/destructible counts
largest-block behavior
build elapsed
```

### Strict RAM acceptance

```text
EMPTY1 - SOURCE heap = 18008 B
EMPTY2 heap/largest == EMPTY1 heap/largest
RESTORED heap/largest == SOURCE heap/largest
final heap delta = 0
PAK closed
```

The fragmentation checks are intentional; do not relax them without analyzing real hardware evidence.

### Legacy/final boundary

At final PARK the candidate must prove:

```text
Entrance source restored exactly
framebuffer same-build equality
legacy Player witness unchanged
legacy transition/menu witness unchanged
legacy Render runtime clear
DoomCanvas_loadMapCalled=no
menuMutation=no
legacyPlayerMutation=no
mapSwapCommitted=no
targetLeftResident=no
entities=0 monsters=0
ST_INTRO page=3
noGameplay=yes
```

Persistent state at PARK must remain the proven Entrance `18008 B`.

## Validation

Build/flash normal environment:

```text
esp32-cyd
```

Branch / exact firmware:

```text
agent/esp32-native-resident-handoff
f71520281254ff9d0b2d5e4be1b3611e29ca87c4
```

Capture:

```text
[RESIDENTHANDOFFPROBE]
[RESIDENTHANDOFF]
[BSPREAD]
[MAPRT]
[MAPSTATE]
[MAPLINESTATE]
[MAPLINETEX]
[MAPAUTOMAP]
[ALIVE]
```

No CI status is published. No local build or hardware PASS is claimed.

## Architecture boundary

Hardware-proven through PR #66:

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
```

Candidate adds:

```text
generic explicit resident lifecycle
reversible full resident-map handoff proof
first temporary full Junction native residency
```

Still outside:

```text
committed native transition state machine
leaving Junction resident after transition
spawn/loadType handoff
actual stats-menu rendering/input
full native entity/monster gameplay
ST_PLAYING progression
native gameplay renderer
sound playback
```

Do not merge until exact firmware `f71520281254ff9d0b2d5e4be1b3611e29ca87c4` passes on the real CYD and every later commit remains documentation-only.
