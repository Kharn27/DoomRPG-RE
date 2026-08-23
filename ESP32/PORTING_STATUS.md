# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #81 — post-load flag cleanup
main = c4a093d9db77a715c355a68c5aae9faaddf22e0b
hardware-tested firmware = 7f16e08f6948da121815ba669fcbbff7e061e2b7
status = REAL-CYD HARDWARE PASS
```

Merged evidence: [`MAP1_NATIVE_POST_LOAD_FLAG_CLEANUP.md`](MAP1_NATIVE_POST_LOAD_FLAG_CLEANUP.md).

## Current hardware candidate

```text
branch = agent/esp32-native-post-load-event-particle-cleanup
base   = c4a093d9db77a715c355a68c5aae9faaddf22e0b
status = HARDWARE CANDIDATE — NOT YET CYD-PROVEN
```

Candidate: [`MAP1_NATIVE_POST_LOAD_EVENT_PARTICLE_CLEANUP.md`](MAP1_NATIVE_POST_LOAD_EVENT_PARTICLE_CLEANUP.md).

It owns only:

```c
doomCanvas->numEvents = 0;
ParticleSystem_freeAllParticles(doomCanvas->particleSystem);
doomCanvas->numEvents = 0;
```

The permanent path does not call the legacy particle function. Because queued-
event and particle payload ownership is not native yet, this milestone is fail-
closed unless both collections are already empty.

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
bytes=21823
crc32=623f34e4
sourceFNV=d5cc751f
gameplayLoadMapId=1
spawnIndex=904
spawnDirection=64
snapshotFNV=b3811f3d
logical payload=17891 B
actual heap=18008 B
```

Junction:

```text
resourceMapId=9 / /junction.bsp
gameplayLoadMapId=2
sourceBytes=21051
crc32=4a2c5800
sourceFNV=fefaf5ca
spawnIndex=943
spawnDirection=64
payload=10410 B
actual heap=10540 B
entities=30
enemies=0
destructibles=3
```

Current Junction resident owner FNVs:

```text
runtime  = bc432a0f
map      = 8dba0bb4
script   = bc9b18ff
line     = 3658710d
texture  = 537319ad
automap  = b699bd75
topology = d6e8df7d
snapshot = bb714d80
```

## Hardware-proven transition/player/post-load chain

```text
CHANGEMAP pending intent
 -> level-exit stats
 -> native player exit-state
 -> LEVEL stats-menu semantic intent
 -> immutable 13-map catalog
 -> Junction transition preflight
 -> resident lifecycle / committed swap
 -> 24 B fresh-map spawn projection
 -> 44 B active player/view owner
 -> 8 B post-spawn HUD dirty owner
 -> 24 B Player_setup session owner
 -> 24 B initial tile owner
 -> 24 B finishRotation orientation owner
 -> 24 B finishRotation second-tile owner
 -> 32 B durable facing owner
 -> 8 B post-load HUD-clear owner
 -> 16 B direct Junction GIVEMAP caller-order owner
 -> 8 B current-weapon self-select caller-order owner
 -> 24 B initial-save semantic intent owner
 -> 8 B post-load flag cleanup owner
```

Canonical fingerprints:

```text
levelExitStatsFNV               = bd41bcfa
playerExitAppliedFNV            = 298eaaa4
statsMenuIntentFNV              = 96afe901
catalogFNV                      = ce322e3f
transitionPreflightFNV          = 108e5c7b
committed WAIT_STATS FNV        = 66fe636a
committed READY FNV             = 0ef58ea8
committed ROLLBACK FNV          = 2dec1442
committed COMMITTED FNV         = 2c595a62
Junction spawn FNV              = ba6af4a7
packed override FNV             = e0a5110b
Junction player/view FNV        = d1131d18
packed override view FNV        = 9ed47d08
post-HUD player/view FNV        = d17fa0d1
Junction HUD refresh FNV        = 6965ee06
Player_setup semantic FNV       = 3b27c6a1
post-setup player/view FNV      = c21fba3c
Junction initial-tile FNV       = f73e28b2
post-initial-tile player FNV    = 1bd0f09b
Junction orientation FNV        = acc754a6
Junction second-tile FNV        = 09e58e0d
Junction durable-facing FNV     = 95aa1108
post-facing player/view FNV     = afcdcf74
Junction post-load HUD clear    = b7383e18
Junction post-load GIVEMAP      = 448e587d
Junction weapon self-select     = 699f3cf3
Junction initial-save intent    = 0bf1a911
Junction post-load flag cleanup = 46cb2547
```

Generic `EspMapOpcodeExecutor` remains intentionally only 11/19/20.

## Last hardware-proven caller boundary: flag cleanup

```text
EspPostLoadFlagCleanupState = 8 B
stateFNV=46cb2547
isLoaded=0->0
isSaved=0->0
activeLoadType=0->0
targetMap=9
active=1
persistentHeapBytes=0
```

Resident remained exactly `bb714d80`; normal-env RAM was:

```text
heap8=72620->72620
largest8=34804->34804
persistentHeapBytes=0
```

Current hardware PARK before the new candidate:

```text
state=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativeInitialSaveIntent=yes
nativePostLoadFlagCleanup=yes
initialSavePersistencePending=yes
flagCleanupPending=no
eventParticleCleanupPending=yes
isUpdateViewPending=yes
ST_PLAYING=no
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

## Current event / particle cleanup design

### Exact legacy behavior

The caller performs:

```text
1. DoomCanvas.numEvents = 0
2. ParticleSystem_freeAllParticles()
3. DoomCanvas.numEvents = 0 again
```

`DoomCanvas.events` has capacity 8 and `numEvents` is a byte.

Legacy `ParticleSystem_freeAllParticles()` traverses a pointer-heavy circular
active list and moves every active node back to a circular free list. The pool is
`nodeListC[64]`.

### Permanent owner

```text
ESP32/include/esp_post_load_event_particle_cleanup_state.h
ESP32/src/esp_post_load_event_particle_cleanup_state.c
EspPostLoadEventParticleCleanupState = 8 B candidate
persistent heap = 0 B
```

Candidate semantic fields:

```text
numEventsBefore
numEventsAfterFirstClear
particleCountBefore
particleCountAfterClear
numEventsAfterSecondClear
targetMapId
active
reserved
```

Only the empty current path is supported:

```text
numEventsBefore=0
particleCountBefore=0
```

Any non-empty input returns an explicit fail-closed status. The permanent owner
never receives legacy pointers or event/particle payloads.

### Hardware candidate acceptance

Expected semantic state if the CYD confirms the empty boundary:

```text
stateBytes=8
stateFNV=<hardware establishes>
numEvents=0->0->0
particleCount=0->0
targetMap=9
active=1
```

Required predecessor:

```text
flagCleanupBytes=8
flagCleanupFNV=46cb2547
unchanged=yes
callerOrder=yes
```

Required particle topology proof:

```text
particleTopologyCanonical=yes
activeList=0
freeList=64
totalPool=64
```

The probe validates reciprocal links, pool membership, uniqueness and exact
coverage of all 64 nodes before accepting `particleCount=0`.

Required fail-closed proof:

```text
nullFlag=1
nullOutput=1
inactiveFlag=1
targetMap=1
invalidEvents=1
invalidParticles=1
nonemptyEvents=1
nonemptyParticles=1
prepareAtomic=yes
postActivePrepare=1
repeat=1
repeatAtomic=yes
```

Resident must remain:

```text
snapshotFNV=bb714d80
runtimeFNV=bc432a0f
mapFNV=8dba0bb4
scriptFNV=bc9b18ff
lineFNV=3658710d
textureFNV=537319ad
automapFNV=b699bd75
topologyFNV=d6e8df7d
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes
```

Required side-effect proof:

```text
heap/largest delta=0
frame unchanged
event queue bytes unchanged
legacy ParticleSystem unchanged
legacy ParticleSystem_freeAllParticles not called
Game/Player/Hud/DoomCanvas/Render unchanged
legacy runtime clear
ST_PLAYING=no
entities=0
monsters=0
```

Candidate successful PARK adds:

```text
nativeEventParticleCleanup=yes
eventParticleCleanupPending=no
isUpdateViewPending=yes
ST_PLAYING=no
```

## Probe completion semantics

Historical probes may set `done=1` on terminal failure. Downstream stages must
revalidate exact predecessor owners/world state. The current candidate follows
the newer convention and sets its own `done=1` only after successful PARK.

## Exact recovered caller order

```text
DoomCanvas_finishRotation()                  [hardware-proven complete]
Hud.msgCount=0                              [hardware-proven]
Hud.statBarMessage=NULL                     [hardware-proven]
Hud.logMessage[0]='\0'                     [hardware-proven]
if Junction: Game_givemap()                 [hardware-proven]
Player_selectWeapon(player, player->weapon) [hardware-proven]
conditional Game_saveState(...)             [hardware-proven semantic intent]
Game.isLoaded=false                         [hardware-proven]
Game.isSaved=false                          [hardware-proven]
Game.activeLoadType=0                       [hardware-proven]
DoomCanvas.numEvents=0                      [CURRENT CANDIDATE]
ParticleSystem_freeAllParticles(...)        [CURRENT CANDIDATE]
DoomCanvas.numEvents=0                      [CURRENT CANDIDATE]
DoomCanvas.isUpdateView=true                [next after PASS/merge]
DoomCanvas_setState(ST_PLAYING)             [deferred]
idleTime=time+8000                          [deferred]
```

## Still intentionally outside

```text
native durable save storage
cross-map durable SAVEGAME route payload
native queued-event payload ownership
native particle payload/runtime ownership
isUpdateView caller write
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

## Next test

Build/flash normal `esp32-cyd` from the current candidate branch and return the
complete `[JUNCTIONEPCLEANUP]` Serial block. Promote only after real-CYD PASS.

## Next bounded milestone after PASS + merge

Recover exact new `main`, then own only:

```c
doomCanvas->isUpdateView = true;
```

Do not bundle `ST_PLAYING`, `idleTime`, rendering or durable native save storage.
