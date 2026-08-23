# ESP32 native Junction first PLAYING service milestone

Branch: `agent/esp32-native-playing-service`

Base merged `main`:

```text
PR   = #85 — post-load idle time
main = cdd7f3c7bdd7f1ea472faaccf64d055e7a00a4a2
```

Hardware-tested firmware:

```text
e9c10c8759588e48478d3d702292628411c5939e
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

Leave `DoomCanvas_loadMedia()` recovery behind and establish the first permanent native PLAYING service boundary.

The service consumes the hardware-proven native ST_PLAYING and idle-time owners, requires an empty input queue and safe fresh-Junction idle gates, then records the first PLAYING iteration without calling legacy `DoomCanvas_run()` / `DoomCanvas_playingState()` and without gameplay, rendering, presentation or allocation.

Legacy inspection shows that the first valid `DoomCanvas_playingState()` frame would immediately request view/render/HUD work because `monstersTurn == false`. This milestone therefore owns the PLAYING service/dispatch decision and records render/HUD intent, while keeping the first framebuffer mutation as a separate milestone.

## Permanent owner

Files:

```text
ESP32/include/esp_native_playing_service_state.h
ESP32/src/esp_native_playing_service_state.c
```

Hardware-proven ABI and stable fingerprint:

```text
EspNativePlayingServiceState = 12 B
stateFNV = 4c50b853
persistent heap = 0 B
```

Hardware-proven owner:

```text
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

Unlike the post-load idle-time owner, this owner contains no live clock and `4c50b853` is a stable cross-boot fingerprint for this exact first-service state.

## Real-CYD semantic proof

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

## Hardware gate values

The real classic CYD established the first safe PLAYING-service boundary as:

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

Important consequences:

- player is alive;
- no delayed event, active sprite, animated door, queued input or particle work exists;
- the view is already at its destination position/angle;
- the native idle deadline has not expired;
- there is no active monster list on this fresh Junction path;
- `monstersTurn=0` means the legacy PLAYING path would request view/render/HUD work on this iteration, so native `renderIntent=1` is behavior-derived rather than invented.

## Strict predecessor proof

```text
playingBytes=12
playingFNV=73bc9acd
unchanged=yes

idleBytes=16
idleFNV=1e6f9a0e
unchanged=yes
idleDelta=8000
callerOrder=yes
```

`idleFNV=1e6f9a0e` is a witness of this boot only. The post-load idle-time owner contains live uptime, so its stable contract remains `idleTimeAfter - timeBefore = 8000`, not a cross-boot FNV.

Particle topology remained canonical:

```text
activeList=0
freeList=64
totalPool=64
```

## Fail-closed proof

```text
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

The permanent service does not consume non-empty input and cannot partially park invalid or repeated state.

The hardware probe also refuses to route the service unless the observed legacy scaffolding has safe first-frame gates: player alive, no wait/event/sprite/door/particle work, settled view and an unexpired native idle deadline.

## Resident integrity

```text
snapshotFNV=bb714d80->bb714d80
mapFNV=8dba0bb4
automapFNV=b699bd75
runtimeFNV=bc432a0f
scriptFNV=bc9b18ff
lineFNV=3658710d
textureFNV=537319ad
topologyFNV=d6e8df7d
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes
```

## RAM proof

Normal `esp32-cyd` environment:

```text
heap8=72516->72516
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Stable heartbeat after PARK:

```text
heap=138280
heap8=72516
largest8=34804
SD=ready
ZIP=ready
VIDEO=ready
CORE=ready
LAYOUT=ready
PRERENDER=ready
RENDER=ready
MAPPINGS=ready
MENUBSP=ready
```

## Legacy and framebuffer equality witnesses

Same-build witnesses only:

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

The unchanged framebuffer is intentional: this is the last renderer-free boundary before the first native gameplay frame milestone.

Mandatory runtime invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
legacy Game.entities = 0
legacy Game.monsters = 0
runtime ZIP map access forbidden
```

## Hardware-proven PARK

```text
legacyState=9
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
entities=0
monsters=0
noGameplay=yes
```

## Architectural consequence

The port now has a hardware-proven permanent native PLAYING state **and** a hardware-proven first PLAYING service iteration. The temporary legacy canvas remains parked at `ST_INTRO`; it is no longer the authority for gameplay state.

The next bounded milestone may finally mutate the logical 160x120 framebuffer, but must stay narrow: consume this stable service owner (`4c50b853`) and produce the first Junction gameplay frame natively without switching legacy state to 3, without invoking `DoomCanvas_playingState()` / `Render_render()`, and without enabling input, turns, entities or monsters.
