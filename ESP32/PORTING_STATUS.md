# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #85 — post-load idle time
main = cdd7f3c7bdd7f1ea472faaccf64d055e7a00a4a2
hardware-tested firmware = 1349ed314487bcade159ce92c6ad9c27b75735d5
status = REAL-CYD HARDWARE PASS
```

Merged evidence: [`MAP1_NATIVE_POST_LOAD_IDLE_TIME.md`](MAP1_NATIVE_POST_LOAD_IDLE_TIME.md).

The successful fresh-Junction `DoomCanvas_loadMedia()` caller tail is semantically complete.

## Current merge-ready milestone

```text
branch = agent/esp32-native-playing-service
base   = cdd7f3c7bdd7f1ea472faaccf64d055e7a00a4a2
hardware-tested firmware = e9c10c8759588e48478d3d702292628411c5939e
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Evidence: [`MAP1_NATIVE_PLAYING_SERVICE.md`](MAP1_NATIVE_PLAYING_SERVICE.md).

This milestone establishes the first permanent native PLAYING service iteration while keeping input, gameplay, rendering and presentation deferred.

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
native ST_PLAYING = reached
legacy ST_PLAYING = not reached
native PLAYING service = reached
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

## Hardware-proven transition/player/post-load/PLAYING chain

```text
CHANGEMAP pending intent
 -> level-exit stats
 -> native player exit-state
 -> LEVEL stats-menu semantic intent
 -> immutable 13-map catalog
 -> Junction transition preflight
 -> resident lifecycle / committed swap
 -> fresh-map spawn projection
 -> active player/view owner
 -> post-spawn HUD dirty owner
 -> Player_setup session owner
 -> initial tile owner
 -> finishRotation orientation owner
 -> finishRotation second-tile owner
 -> durable facing owner
 -> post-load HUD-clear owner
 -> direct Junction GIVEMAP owner
 -> current-weapon self-select owner
 -> initial-save semantic intent owner
 -> post-load flag cleanup owner
 -> event/particle cleanup owner
 -> post-load view-invalidation owner
 -> native ST_PLAYING transition owner
 -> post-load idle-time owner
 -> first native PLAYING service owner
```

Stable canonical fingerprints:

```text
levelExitStatsFNV                 = bd41bcfa
playerExitAppliedFNV              = 298eaaa4
statsMenuIntentFNV                = 96afe901
catalogFNV                        = ce322e3f
transitionPreflightFNV            = 108e5c7b
committed WAIT_STATS FNV          = 66fe636a
committed READY FNV               = 0ef58ea8
committed ROLLBACK FNV            = 2dec1442
committed COMMITTED FNV           = 2c595a62
Junction spawn FNV                = ba6af4a7
packed override FNV               = e0a5110b
Junction player/view FNV          = d1131d18
packed override view FNV          = 9ed47d08
post-HUD player/view FNV          = d17fa0d1
Junction HUD refresh FNV          = 6965ee06
Player_setup semantic FNV         = 3b27c6a1
post-setup player/view FNV        = c21fba3c
Junction initial-tile FNV         = f73e28b2
post-initial-tile player FNV      = 1bd0f09b
Junction orientation FNV          = acc754a6
Junction second-tile FNV          = 09e58e0d
Junction durable-facing FNV       = 95aa1108
post-facing player/view FNV       = afcdcf74
Junction post-load HUD clear      = b7383e18
Junction post-load GIVEMAP        = 448e587d
Junction weapon self-select       = 699f3cf3
Junction initial-save intent      = 0bf1a911
Junction post-load flag cleanup   = 46cb2547
Junction event/particle cleanup   = 8bc79e2b
Junction view invalidation        = 4561c3c1
Junction native ST_PLAYING        = 73bc9acd
Junction native PLAYING service   = 4c50b853
```

The idle-time owner FNV is intentionally not cross-boot canonical because it contains live uptime. The current PLAYING-service run observed `idleFNV=1e6f9a0e`, while the stable idle contract remains `idleTimeAfter-timeBefore=8000`.

Generic `EspMapOpcodeExecutor` remains intentionally only 11/19/20.

## Hardware-proven native ST_PLAYING

```text
EspPostLoadPlayingTransitionState = 12 B
stateFNV=73bc9acd
state=9->3
monstersTurn=0
displaySoftKeys=0
restoreSoftKeys=0->0
skipCheckState=0->1
softKeyIntent=1
softKeyPresentationDeferred=0
targetMap=9
active=1
nativeST_PLAYING=yes
legacyST_PLAYING=no
persistentHeapBytes=0
```

Legacy `DoomCanvas.state` remains parked at `ST_INTRO` so the legacy loop cannot enter `DoomCanvas_playingState()` / `Render_render()`.

## Hardware-proven post-load idle time

Stable contract:

```text
EspPostLoadIdleTimeState = 16 B
idleTimeAfter = timeBefore + 8000
targetMap=9
active=1
persistentHeapBytes=0
```

Reference hardware run for PR #85:

```text
timeBefore=4600
idleTimeBefore=0
idleTimeAfter=12600
delta=8000
stateFNV=d6e95f57   # run-specific witness
```

Current playing-service boot revalidated the same semantic owner with `idleFNV=1e6f9a0e` and `idleDelta=8000`.

## Hardware-proven first native PLAYING service

Permanent files:

```text
ESP32/include/esp_native_playing_service_state.h
ESP32/src/esp_native_playing_service_state.c
```

Owner:

```text
EspNativePlayingServiceState = 12 B
stateFNV=4c50b853
persistentHeapBytes=0
nativeState=3
serviceOrdinal=1
inputCountBefore=0
inputConsumed=0
gameplayDispatched=0
renderIntent=1
renderDeferred=1
presentationDeferred=1
hudIntent=1
targetMapId=9
active=1
reserved=0
```

Real-CYD semantic proof:

```text
firstNativePlayingService=yes
nativeST_PLAYING=yes
inputQueueEmpty=yes
inputConsumed=no
gameplayDispatch=no
renderRequested=yes
renderDeferred=yes
hudRequested=yes
presentation=no
legacyDoomCanvas_runCalled=no
legacyDoomCanvas_playingStateCalled=no
legacyDoomCanvas_updateViewCalled=no
legacyDoomCanvas_drawRGBCalled=no
```

Real-CYD first-service gates:

```text
renderOnly=0
health=30
waitTime=0
activeSprites=0
monstersTurn=0
openDoors=0
animDoors=0
viewSettled=yes
isUpdateView=1
particleCount=0
idleDeadlinePending=yes
currentTime=4200
nativeIdleDeadline=12200
activeMonstersPresent=no
```

Strict predecessor / fail-closed proof:

```text
playingBytes=12
playingFNV=73bc9acd
unchanged=yes
idleBytes=16
idleFNV=1e6f9a0e
unchanged=yes
idleDelta=8000
callerOrder=yes
particleTopologyCanonical=yes
activeList=0
freeList=64
totalPool=64

nullPlaying=1
nullIdle=1
nullOutput=1
inactivePlaying=1
badIdle=1
inputPending=1
prepareAtomic=yes
postActivePrepare=1
repeat=1
repeatAtomic=yes
```

Resident / RAM integrity:

```text
snapshotFNV=bb714d80->bb714d80
runtimeFNV=bc432a0f
mapFNV=8dba0bb4
scriptFNV=bc9b18ff
lineFNV=3658710d
textureFNV=537319ad
automapFNV=b699bd75
topologyFNV=d6e8df7d
heap8=72516->72516
largest8=34804->34804
persistentHeapBytes=0
packClosed=yes
```

Same-build equality witnesses only:

```text
gameFNV=cfb0e7fb->cfb0e7fb
playerFNV=a7a56b94->a7a56b94
hudFNV=d2deba0f->d2deba0f
canvasFNV=ae31f4d7->ae31f4d7
renderFNV=f9344dec->f9344dec
frameFNV=ee9d9dbc->ee9d9dbc
eventQueueFNV=d985589f->d985589f
particleFNV=f186cf0c->f186cf0c
legacyState=9->9
legacyRuntimeClear=yes
GameMutation=no
PlayerMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
ParticleSystemMutation=no
frameMutation=no
```

## Fresh-Junction load tail: complete

Every successful caller statement through `return true` is now hardware-proven semantically. There is no remaining post-load statement to recover before PLAYING service/render work.

## Current hardware PARK

```text
legacyState=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativeST_PLAYING=yes
nativeIdleTime=yes
postLoadTailComplete=yes
nativePlayingService=yes
playingServicePending=no
firstFramePending=yes
gameplayDispatchPending=yes
rendererPending=yes
initialSavePersistencePending=yes
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

## Still intentionally outside

```text
native durable save storage
cross-map durable SAVEGAME route payload
native queued-event payload ownership for non-empty contexts
native particle payload/runtime ownership for non-empty contexts
first native gameplay framebuffer render/presentation
native input dispatch
turn advancement
full native entity/monster gameplay
sound playback
```

## Probe completion semantics

Historical temporary probes may set `done=1` on terminal failure. `*_isDone()` alone is not a PASS certificate. Downstream probes must revalidate exact predecessor owners/world state. The current PLAYING-service probe sets `done=1` only after successful PARK.

## Merge recommendation

```text
MERGE agent/esp32-native-playing-service
```

Hardware-tested firmware:

```text
e9c10c8759588e48478d3d702292628411c5939e
```

All commits after that tested SHA must remain documentation-only.

## Next bounded milestone after merge

Recover exact new `main`, then consume the stable native PLAYING-service owner (`stateFNV=4c50b853`) to produce the **first native Junction gameplay frame**.

Keep it bounded:

```text
input consumed = no
turn advanced = no
legacy Game.entities = 0
legacy Game.monsters = 0
legacy DoomCanvas.state = ST_INTRO
legacy DoomCanvas_playingState = not called
legacy Render_render = not called
shapeData = NULL
mediaTexels = NULL
```

The milestone may finally mutate the logical 160x120 framebuffer and present it, but must not bundle input, turn logic, entities/monsters or a full legacy-style renderer architecture.
