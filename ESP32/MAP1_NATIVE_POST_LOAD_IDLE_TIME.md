# ESP32 native Junction post-load idle-time milestone

Branch: `agent/esp32-native-post-load-idle-time`

Base merged `main`:

```text
PR   = #84 — native ST_PLAYING transition
main = 0a2cf860e074b19240f50fc65822710ab8d505bb
```

Hardware-tested firmware:

```text
1349ed314487bcade159ce92c6ad9c27b75735d5
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

Recover only the final successful fresh-map caller write in `DoomCanvas_loadMedia()`:

```c
doomCanvas->idleTime = doomCanvas->time + 8000;
```

The native owner records the temporal result without mutating legacy `DoomCanvas_t`, dispatching gameplay, rendering, presenting or allocating.

This milestone completes the recovered successful fresh-Junction caller tail. Native PLAYING loop/input dispatch and native rendering remain separate architectural milestones.

## Permanent owner

Files:

```text
ESP32/include/esp_post_load_idle_time_state.h
ESP32/src/esp_post_load_idle_time_state.c
```

ABI:

```text
EspPostLoadIdleTimeState = 16 B
persistent heap = 0 B
```

Fields:

```text
int32_t timeBefore
int32_t idleTimeBefore
int32_t idleTimeAfter
uint8_t targetMapId
uint8_t active
uint8_t reserved0
uint8_t reserved1
```

The permanent relation is:

```text
idleTimeAfter = timeBefore + 8000
```

The implementation fails closed for negative `timeBefore` and for values above `INT32_MAX - 8000`.

## Real-CYD hardware result

Normal `esp32-cyd` firmware printed:

```text
[JUNCTIONIDLETIME] READY stateBytes=16 stateFNV=d6e95f57 time=4600 idleTime=0->12600 delta=8000 targetMap=9 active=1
[JUNCTIONIDLETIME] SEMANTIC nativeST_PLAYING=yes loadTailComplete=yes idleDeadlineOwned=yes legacyTime=4600->4600 legacyIdleTime=0->0 legacyMutation=no gameplayDispatch=no rendering=no presentation=no
[JUNCTIONIDLETIME] INPUT playingBytes=12 playingFNV=73bc9acd unchanged=yes callerOrder=yes nativeState=3 particleTopologyCanonical=yes activeList=0 freeList=64 totalPool=64
[JUNCTIONIDLETIME] FAILCLOSED nullPlaying=1 nullOutput=1 inactivePlaying=1 targetMap=1 negativeTime=1 overflowTime=1 prepareAtomic=yes postActivePrepare=1 repeat=1 repeatAtomic=yes
[JUNCTIONIDLETIME] RESIDENT snapshotFNV=bb714d80->bb714d80 unchanged=yes mapFNV=8dba0bb4 automapFNV=b699bd75 runtimeFNV=bc432a0f scriptFNV=bc9b18ff lineFNV=3658710d textureFNV=537319ad topologyFNV=d6e8df7d payload=10410 entities=30 enemies=0 destructibles=3 packClosed=yes
[JUNCTIONIDLETIME] RAM heap8=72540->72540 delta=0 largest8=34804->34804 delta=0 persistentHeapBytes=0
[JUNCTIONIDLETIME] LEGACY gameFNV=3982324b->3982324b playerFNV=c64e7862->c64e7862 hudFNV=d2deba0f->d2deba0f canvasFNV=afd3b96c->afd3b96c renderFNV=f9344dec->f9344dec frameFNV=10f53ffb->10f53ffb eventQueueFNV=d985589f->d985589f particleFNV=f186cf0c->f186cf0c legacyState=9->9 time=4600->4600 idleTime=0->0 legacyRuntimeClear=yes GameMutation=no PlayerMutation=no HudMutation=no DoomCanvasMutation=no RenderMutation=no ParticleSystemMutation=no
[JUNCTIONIDLETIME] PARK legacyState=9 page=3 targetMap=9 junctionResident=yes nativeST_PLAYING=yes nativeIdleTime=yes postLoadTailComplete=yes ST_PLAYINGPending=no idleTimePending=no gameplayDispatchPending=yes rendererPending=yes initialSavePersistencePending=yes entities=0 monsters=0 noGameplay=yes
```

## Stable semantic canon

```text
stateBytes=16
delta=8000
targetMap=9
active=1
nativeST_PLAYING=yes
nativeIdleTime=yes
postLoadTailComplete=yes
ST_PLAYINGPending=no
idleTimePending=no
persistentHeapBytes=0
```

The following values are hardware-run witnesses, not cross-boot constants:

```text
timeBefore=4600
idleTimeBefore=0
idleTimeAfter=12600
stateFNV=d6e95f57
```

`stateFNV` depends on `timeBefore`, therefore a future valid boot is expected to produce a different idle-time owner FNV while preserving the stable semantic relation `idleTimeAfter - timeBefore = 8000`.

## Strict predecessor proof

```text
EspPostLoadPlayingTransitionState = 12 B
playingFNV=73bc9acd
unchanged=yes
callerOrder=yes
nativeState=3
```

Particle topology stayed canonical:

```text
activeList=0
freeList=64
totalPool=64
```

## Fail-closed proof

```text
nullPlaying=1
nullOutput=1
inactivePlaying=1
targetMap=1
negativeTime=1
overflowTime=1
prepareAtomic=yes
postActivePrepare=1
repeat=1
repeatAtomic=yes
```

## Resident / RAM integrity

```text
snapshotFNV=bb714d80->bb714d80
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

heap8=72540->72540
largest8=34804->34804
persistentHeapBytes=0
```

## Legacy / framebuffer equality witnesses

Same-build witnesses only:

```text
gameFNV=3982324b->3982324b
playerFNV=c64e7862->c64e7862
hudFNV=d2deba0f->d2deba0f
canvasFNV=afd3b96c->afd3b96c
renderFNV=f9344dec->f9344dec
frameFNV=10f53ffb->10f53ffb
eventQueueFNV=d985589f->d985589f
particleFNV=f186cf0c->f186cf0c
legacyState=9->9
time=4600->4600
idleTime=0->0
legacyRuntimeClear=yes
GameMutation=no
PlayerMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
ParticleSystemMutation=no
```

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
ST_PLAYINGPending=no
idleTimePending=no
gameplayDispatchPending=yes
rendererPending=yes
initialSavePersistencePending=yes
entities=0
monsters=0
noGameplay=yes
```

## Architectural consequence

There is now no remaining successful fresh-map caller statement after this owner in `DoomCanvas_loadMedia()`.

The next milestone must therefore leave post-load recovery and introduce a bounded native PLAYING-loop boundary that consumes the hardware-proven native state without switching legacy `DoomCanvas.state` to `ST_PLAYING` and without invoking `DoomCanvas_playingState()` / the removed legacy renderer.
