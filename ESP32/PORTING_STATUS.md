# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #68 — native committed Junction transition
main = 00268a100c6662cb883f9a02d979b4f29eecbf12
hardware-tested firmware = 759b7f05a7c1940e98caf68e4041faa69b34cfc9
status = REAL-CYD HARDWARE PASS
```

Merged evidence: [`MAP1_NATIVE_COMMITTED_TRANSITION.md`](MAP1_NATIVE_COMMITTED_TRANSITION.md).

PR #68 is the first hardware-proven committed native resident map replacement: Entrance is released, Junction is built and deliberately left resident, while the legacy Game/Player/Menu/Render/DoomCanvas gameplay world remains untouched.

## Current candidate

```text
branch = agent/esp32-native-junction-spawn
base   = 00268a100c6662cb883f9a02d979b4f29eecbf12
firmware candidate = 08a3a29c5e4e4a64000fa12a877299bbb1e772a0
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

Active evidence: [`MAP1_NATIVE_JUNCTION_SPAWN.md`](MAP1_NATIVE_JUNCTION_SPAWN.md).

The candidate introduces a 24-byte pointer-free native player spawn projection for the already committed Junction map. It reproduces the deterministic placement writes of recovered `Game_spawnPlayer()` without applying them to legacy state and without entering gameplay.

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
bytes=21823
crc32=623f34e4
sourceFNV=d5cc751f
gameplayLoadMapId=1
spawnIndex=904
spawnDirection=64
camera=648
floorTex=145
ceilingTex=112
nodes=223
lines=480
mapSprites=344
events=93
byteCodes=265
strings=94
stringData=7779
```

Entrance native resident memory:

```text
snapshotBytes=96
snapshotFNV=b3811f3d
logical payload=17891 B
actual heap=18008 B
allocator overhead=117 B
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

All real MAP_INTRO opcode IDs have explicit native ownership/execution boundaries:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

## Hardware-proven transition chain through PR #68

Real Entrance CHANGEMAP:

```text
event=1
commandOffset=1
globalCommand=2
arg1=80000000
mapName=/junction.bsp
targetMap=9
spawnParam=0
showStats=1
pending=1
```

Native chain:

```text
CHANGEMAP pending intent
 -> level-exit stats
 -> native player exit-state
 -> LEVEL stats-menu semantic intent
 -> immutable 13-map catalog
 -> Junction target preflight
 -> explicit resident lifecycle
 -> reversible Entrance -> Junction -> Entrance proof
 -> committed transition state machine
 -> real committed Entrance -> Junction resident swap
```

Canonical transition fingerprints:

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
```

The committed state is:

```text
EspMapCommittedTransitionState = 24 B
persistent heap = 0 B
```

## Junction source canon

Resource identity and gameplay progression identity remain distinct:

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
nodes=77
lines=207
mapSprites=48
events=66
byteCodes=319
strings=126
stringData=12235
runtimePlan=8867 B
```

## Hardware-proven Junction resident canon

```text
snapshotBytes=96
snapshotFNV=bc9071e9
payload=10410 B
actual heap=10540 B
allocator overhead=130 B
```

Owner payloads:

```text
runtime   = 8867 B
map       = 1024 B
script    = 73 B
line      = 52 B
texture   = 26 B
automap   = 32 B
topology  = 336 B
```

Owner FNVs:

```text
runtime  = bc432a0f
map      = c5cdfc04
script   = bc9b18ff
line     = 3658710d
texture  = 537319ad
automap  = 0b2ae445
topology = d6e8df7d
```

Compact topology:

```text
entities=30
enemies=0
destructibles=3
```

These are compact native topology entities. Legacy `Game.entities` and `Game.monsters` remain zero.

## Hardware-proven committed residency

PR #68 final commit:

```text
phase=COMMITTED
committed=1
committedStateFNV=2c595a62
sourceMap=1
targetMap=9
targetSnapshotFNV=bc9071e9
sourceHeap=65584
targetHeap=73052
free-heap gain=7468 B
largest=34804->34804
packClosed=yes
```

Final boundary:

```text
mapSwapCommitted=yes
junctionResident=yes
sourceRestored=no
targetLeftResident=yes
pendingConsumed=yes
statsAck=yes
spawnParam=0 retained
spawnPending=yes
spawnApplied=no
ST_INTRO page=3
ST_PLAYING=no
```

The hardware probe also proved real post-teardown rollback before the final successful commit:

```text
forced target build failure
 -> Entrance reconstructed
 -> snapshotFNV=b3811f3d
 -> heap/largest exact
 -> pack closed
```

## Recovered Game_spawnPlayer semantics

For fresh placement, recovered legacy code behaves as follows.

Header fallback when `spawnParam == 0`:

```text
x     = mapSpawnIndex % 32
y     = mapSpawnIndex / 32
angle = mapSpawnDir
```

Packed override when `spawnParam != 0`:

```text
x     = spawnParam & 31
y     = (spawnParam >> 5) & 31
angle = (spawnParam >> 10) & 255
spawnParam cleared
```

Common placement writes:

```text
viewX = destX = x*64 + 32
viewY = destY = y*64 + 32
viewZ = 36
viewAngle = destAngle = angle
Render.viewZOld = 4
```

Then legacy calls:

```text
DoomCanvas_checkFacingEntity()
if (!game->isLoaded):
    Player_setup()
    Game_executeTile(...)
```

The current candidate owns only the pure placement projection. Facing refresh, Player setup and tile-enter remain pending.

## Recovered load semantics

Normal CHANGEMAP loading uses:

```text
DoomCanvas.loadType = 0
Game.isLoaded       = 0
```

A nonzero `loadType` belongs to saved-state restoration and routes loading into `Game_loadState(loadType)`.

Current candidate therefore supports only:

```text
loadType=0
gameIsLoaded=0
```

and fails closed on saved/loaded contexts.

## Current permanent spawn candidate

Files:

```text
ESP32/include/esp_player_spawn_state.h
ESP32/src/esp_player_spawn_state.c
```

API:

```text
EspPlayerSpawn_reset()
EspPlayerSpawn_prepareCommitted()
```

State:

```text
EspPlayerSpawnState = 24 B expected ABI
persistent heap = 0 B
```

The permanent function requires:

```text
committed transition
fresh-map load context
complete target inventory matching committed target
currently resident runtime matching inventory
```

No PAK I/O, allocation or legacy mutation occurs.

### Real Junction candidate projection

Because committed `spawnParam=0`, the BSP header is authoritative:

```text
spawnIndex=943
spawnDirection=64

tileIndex=943
tileX=15
tileY=29
worldX=992
worldY=1888
angle=64
viewZ=36
viewZOld=4
spawnSource=HEADER
overrideUsed=0
loadType=0
```

Pending effects:

```text
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
```

Static 24-byte FNV prediction:

```text
ba6af4a7
```

This is not a hardware canon until the real CYD confirms it.

### Synthetic packed override candidate

Probe-local transition copy:

```text
spawnParam=00030167
x=7
y=11
angle=192
tileIndex=359
worldX=480
worldY=736
spawnSource=OVERRIDE
```

Static FNV prediction:

```text
e0a5110b
```

The real committed transition remains unchanged.

## Candidate hardware proof

Temporary probe:

```text
ESP32/include/native_junction_spawn_probe.h
ESP32/src/native_junction_spawn_probe.c
```

It runs only after the committed-transition probe has left Junction resident.

It requires:

```text
Junction snapshot bc9071e9 before/after
PAK closed
heap8 unchanged
largest8 unchanged
framebuffer unchanged
legacy placement fields unchanged
legacy Player witness unchanged
legacy Render runtime clear
legacy entities=0
legacy monsters=0
ST_INTRO page=3
```

Fail-closed cases:

```text
null transition
null inventory
null output
not committed
loadType != 0
gameIsLoaded != 0
target mismatch
runtime mismatch
invalid header spawn index
reset to zero
```

Final intended PARK:

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

## Validation

Build/flash normal environment:

```text
esp32-cyd
```

Exact code candidate:

```text
08a3a29c5e4e4a64000fa12a877299bbb1e772a0
```

Capture:

```text
[JUNCTIONSPAWNPROBE]
[JUNCTIONSPAWN]
[BSPREAD]
[ALIVE]
```

Expected decisive values:

```text
stateBytes=24
real stateFNV=ba6af4a7
real tile=15/29
real world=992/1888
real angle=64
real tileIndex=943

override stateFNV=e0a5110b
override tile=7/11
override world=480/736
override angle=192

snapshotFNV=bc9071e9->bc9071e9
heap delta=0
largest delta=0
```

No local build or hardware PASS is claimed for this candidate.

## Architecture boundary

Hardware-proven through merged PR #68:

```text
all MAP_INTRO opcode families owned
exit stats + player exit-state
stats-menu semantic intent
map catalog + Junction preflight
explicit resident lifecycle
reversible full resident handoff
committed transition state machine
post-teardown recovery
real committed Entrance -> Junction native resident swap
Junction left resident
```

Candidate adds:

```text
fresh-map load semantic = loadType 0
native player spawn projection
BSP-header fallback when spawnParam=0
packed spawn override decode
explicit facing/setup/tile-enter pending boundary
```

Still outside:

```text
actual stats-menu rendering/input
application of spawn coordinates to a native gameplay/view owner
facing-entity computation
Player_setup-equivalent native initialization
initial tile-enter execution
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

`shapeData == NULL` and `mediaTexels == NULL` remain mandatory.
