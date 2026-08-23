# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #69 — native Junction spawn projection
main = 992f38374840113409e776fb82ce57ab014607e5
hardware-tested firmware = 08a3a29c5e4e4a64000fa12a877299bbb1e772a0
status = REAL-CYD HARDWARE PASS
```

Merged evidence: [`MAP1_NATIVE_JUNCTION_SPAWN.md`](MAP1_NATIVE_JUNCTION_SPAWN.md).

PR #69 leaves Junction as the committed compact native resident map and hardware-proves the deterministic fresh-map player spawn projection without mutating legacy gameplay state.

## Current candidate

```text
branch = agent/esp32-native-player-view
base   = 992f38374840113409e776fb82ce57ab014607e5
firmware candidate = fe1630ad5618dfd35bbbc555de8f9762d0b046f8
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

Active evidence: [`MAP1_NATIVE_PLAYER_VIEW.md`](MAP1_NATIVE_PLAYER_VIEW.md).

The candidate introduces the first permanent active native player/view owner. It applies the already hardware-proven Junction spawn projection to native state only while keeping HUD refresh, facing lookup, Player_setup, tile-enter and ST_PLAYING explicitly pending.

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

## Hardware-proven map canons

Entrance:

```text
resource=/intro.bsp
name=Entrance
bytes=21823
crc32=623f34e4
sourceFNV=d5cc751f
gameplayLoadMapId=1
spawnIndex=904
spawnDirection=64
snapshotBytes=96
snapshotFNV=b3811f3d
logical payload=17891 B
actual heap=18008 B
```

Entrance owner FNVs:

```text
runtime  = c3882516
map      = cd99b98e
script   = f9e3d9df
line     = e5e74861
texture  = f1fc1875
automap  = 669b1aa7
topology = 3f321e43
```

Junction:

```text
resourceMapId=9
resource=/junction.bsp
gameplayLoadMapId=2
hubProgressionGate=1
entryOffset=1974397
bytes=21051
crc32=4a2c5800
sourceFNV=fefaf5ca
spawnIndex=943
spawnDirection=64
nodes=77
lines=207
mapSprites=48
events=66
byteCodes=319
strings=126
snapshotBytes=96
snapshotFNV=bc9071e9
payload=10410 B
actual heap=10540 B
allocator overhead=130 B
entities=30
enemies=0
destructibles=3
```

Junction owner FNVs:

```text
runtime  = bc432a0f
map      = c5cdfc04
script   = bc9b18ff
line     = 3658710d
texture  = 537319ad
automap  = 0b2ae445
topology = d6e8df7d
```

## Hardware-proven transition chain

```text
CHANGEMAP pending intent
 -> level-exit stats
 -> native player exit-state
 -> LEVEL stats-menu semantic intent
 -> immutable 13-map catalog
 -> Junction target preflight
 -> explicit resident lifecycle
 -> reversible Entrance -> Junction -> Entrance
 -> committed transition with post-teardown rollback
 -> real committed Entrance -> Junction residency
 -> fresh-map load semantic
 -> native Junction player spawn projection
```

Canonical fingerprints:

```text
levelExitStatsFNV        = bd41bcfa
playerExitAppliedFNV     = 298eaaa4
statsMenuIntentFNV       = 96afe901
catalogFNV               = ce322e3f
transitionPreflightFNV   = 108e5c7b
committed WAIT_STATS FNV = 66fe636a
committed READY FNV      = 0ef58ea8
committed ROLLBACK FNV   = 2dec1442
committed COMMITTED FNV  = 2c595a62
Junction spawn FNV       = ba6af4a7
packed override FNV      = e0a5110b
```

All real MAP_INTRO opcode IDs have explicit native ownership/execution boundaries:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

## Hardware-proven fresh-map spawn

Permanent projection:

```text
EspPlayerSpawnState = 24 B
persistent heap = 0 B
```

Real Junction CYD result:

```text
stateFNV=ba6af4a7
targetMap=9
gameplayLoadMapId=2
spawnParam=00000000
source=HEADER
tileIndex=943
tile=15/29
world=992/1888
angle=64
viewZ=36
viewZOld=4
loadType=0
active=1
```

Packed override hardware canon:

```text
spawnParam=00030167
tileIndex=359
tile=7/11
world=480/736
angle=192
stateFNV=e0a5110b
```

Fresh-load semantic:

```text
loadType=0
gameIsLoaded=0
normalMapLoad=yes
savedGameLoad=no
```

Pending after PR #69:

```text
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
spawnApplied=no
```

## Current permanent player/view candidate

Files:

```text
ESP32/include/esp_player_view_state.h
ESP32/src/esp_player_view_state.c
```

API:

```text
EspPlayerView_reset()
EspPlayerView_isReady()
EspPlayerView_view()
EspPlayerView_applySpawn()
```

State:

```text
EspPlayerViewState = 44 B expected classic-ESP32 ABI
persistent heap = 0 B
```

The owner mirrors the legacy-width placement fields:

```text
viewX/viewY/viewZ/viewAngle
destX/destY/destAngle
viewZOld
```

plus target/load identity and explicit follow-up flags.

Recovered `Game_spawnPlayer()` writes `Hud.isUpdate=true` immediately after `Render.viewZOld=4` and before facing lookup. The candidate therefore records:

```text
hudRefreshPending=1
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
```

without mutating the legacy HUD.

### Expected real Junction player/view state

```text
view=992/1888/36
viewAngle=64
dest=992/1888
destAngle=64
viewZOld=4
targetMapId=9
gameplayLoadMapId=2
loadType=0
spawnApplied=1
active=1
```

Static 44-byte FNV prediction:

```text
d1131d18
```

### Expected packed override application

```text
view/dest=480/736
viewZ=36
angle=192
viewZOld=4
```

Static 44-byte FNV prediction:

```text
9ed47d08
```

Both values remain predictions until real-CYD confirmation.

## Candidate fail-closed boundary

The player/view owner refuses without mutation when:

```text
spawn pointer is null
owner is already active
spawn projection is inactive
map/load identity is invalid
tile/world geometry is inconsistent
required pending follow-ups are missing
header-source flags are inconsistent
packed override does not re-decode from sourceSpawnParam
```

The once-only active-owner rule prevents accidental double spawn application.

## Candidate hardware acceptance

The temporary probe runs after the hardware-proven Junction spawn probe and consumes its exact parked 24-byte state.

Required real-CYD proof:

```text
EspPlayerViewState bytes=44
real stateFNV=d1131d18
real view/dest=992/1888
real z=36
real angle=64
real viewZOld=4
override stateFNV=9ed47d08
repeat apply refused atomically
reset/reapply exact
Junction snapshot bc9071e9 unchanged
heap8 delta=0
largest8 delta=0
PAK closed
legacy placement fields unchanged
legacy Hud.isUpdate unchanged
legacy Player unchanged
framebuffer unchanged
legacy Render runtime clear
legacy entities=0
legacy monsters=0
ST_INTRO page=3
ST_PLAYING=no
```

Final intended PARK:

```text
mapSwapCommitted=yes
targetMap=9
junctionResident=yes
nativeSpawnState=yes
nativePlayerView=yes
spawnAppliedNative=yes
legacySpawnApplied=no
hudRefreshPending=yes
facingPending=yes
playerSetupPending=yes
tileEnterPending=yes
ST_PLAYING=no
entities=0
monsters=0
noGameplay=yes
```

## Current architecture boundary

Hardware-proven native ownership includes:

```text
compact immutable Entrance + explicit mutable owners
all MAP_INTRO opcode families
exit/save/change-map chain
Junction catalog/preflight
resident lifecycle
transactional committed map swap
Junction compact topology
fresh-map load semantic
24 B native spawn projection
```

Candidate adds:

```text
44 B permanent native player/view owner
real spawn application to native state
explicit HUD refresh pending semantic
once-only spawn application gate
```

Still intentionally outside:

```text
actual stats-menu rendering/input
legacy/native HUD refresh consumption
native facing-entity query
Player_setup-equivalent fresh-map initialization
initial tile-enter execution
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

The player/view owner is intentionally not reset by `EspMapResidentLifecycle_resetAll()`: player state and map arena have different lifetimes. A future transition orchestrator must explicitly reset/reapply player placement.

`shapeData == NULL` and `mediaTexels == NULL` remain mandatory.

## Validation

Build/flash normal environment:

```text
esp32-cyd
```

Exact code candidate:

```text
fe1630ad5618dfd35bbbc555de8f9762d0b046f8
```

Capture:

```text
[JUNCTIONVIEWPROBE]
[JUNCTIONVIEW]
[ALIVE]
```

No local build or hardware PASS is claimed for this candidate.
