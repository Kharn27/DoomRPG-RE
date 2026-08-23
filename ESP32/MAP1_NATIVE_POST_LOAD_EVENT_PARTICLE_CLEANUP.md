# ESP32 native Junction post-load event / particle cleanup milestone

Branch: `agent/esp32-native-post-load-event-particle-cleanup`

Base merged `main`:

```text
PR   = #81 — post-load flag cleanup
main = c4a093d9db77a715c355a68c5aae9faaddf22e0b
```

Hardware-tested firmware:

```text
48d47b1c2e6e7276ca555e5811933fd033f496ed
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

Recover only the exact caller sequence immediately after the hardware-proven
`Game.isLoaded/isSaved/activeLoadType` cleanup:

```c
doomCanvas->numEvents = 0;
ParticleSystem_freeAllParticles(doomCanvas->particleSystem);
doomCanvas->numEvents = 0;
```

The next caller write remains outside this milestone:

```c
doomCanvas->isUpdateView = true;
```

`ST_PLAYING`, `idleTime`, rendering and durable native save storage remain
deferred.

## Exact legacy behavior

`ParticleSystem_freeAllParticles()` walks the active circular doubly-linked list
`nodeListA`, unlinks each active `ParticleNode_t`, inserts it into free-list
`nodeListB`, then forces `particleCount=0`.

Legacy topology:

```text
nodeListA       active-list sentinel
nodeListB       free-list sentinel
nodeListC[64]   fixed ParticleNode_t pool
particleCount   active node count
```

That pointer-heavy representation remains executable-spec behavior only; it is
not adopted as permanent ESP32 architecture.

## Fail-closed ownership rule

The native path does not yet own queued-event payloads or particle payloads.
Therefore this milestone supports only an already-empty boundary:

```text
numEventsBefore=0
particleCountBefore=0
```

Any non-empty input is explicitly refused. No payload is silently discarded.

The real CYD proved the current Junction path is exactly empty:

```text
numEvents=0->0->0
particleCount=0->0
```

## Permanent owner

Files:

```text
ESP32/include/esp_post_load_event_particle_cleanup_state.h
ESP32/src/esp_post_load_event_particle_cleanup_state.c
```

Hardware-proven ABI:

```text
EspPostLoadEventParticleCleanupState = 8 B
stateFNV = 8bc79e2b
persistent heap = 0 B
```

State:

```text
numEventsBefore=0
numEventsAfterFirstClear=0
particleCountBefore=0
particleCountAfterClear=0
numEventsAfterSecondClear=0
targetMapId=9
active=1
```

Permanent API:

```c
EspPostLoadEventParticleCleanup_reset()
EspPostLoadEventParticleCleanup_isReady()
EspPostLoadEventParticleCleanup_view()
EspPostLoadEventParticleCleanup_prepare()
EspPostLoadEventParticleCleanup_route()
```

The permanent implementation has no `DoomCanvas_t`, `ParticleSystem_t`,
`ParticleNode_t`, `Game_t`, filesystem, presentation or allocation dependency.

## Strict predecessor boundary

The hardware probe revalidated the exact predecessor:

```text
EspPostLoadFlagCleanupState = 8 B
stateFNV = 46cb2547
isLoaded=0->0
isSaved=0->0
activeLoadType=0->0
targetMap=9
active=1
```

and the canonical resident Junction world:

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
```

The initial-save persistence debt remains preserved.

## Particle topology hardware proof

The probe bounded-walked both circular lists and proved reciprocal links, pool
membership, uniqueness and exact coverage of all 64 nodes.

Real-CYD result:

```text
particleTopologyCanonical=yes
activeList=0
freeList=64
totalPool=64
```

Therefore `particleCount=0` is not a stale scalar: the active list is genuinely
empty and all 64 pool nodes are on the free list exactly once.

## Fail-closed proof

The real CYD printed:

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

Non-empty payloads therefore refuse routing and cannot park partial state.

## Real-CYD hardware evidence

Normal `esp32-cyd` firmware:

```text
[JUNCTIONEPCLEANUP] READY stateBytes=8 stateFNV=8bc79e2b numEvents=0->0->0 particleCount=0->0 targetMap=9 active=1
[JUNCTIONEPCLEANUP] SEMANTIC eventQueueEmpty=yes particleActiveListEmpty=yes identityCleanup=yes legacyNumEvents=0->0 legacyParticleCount=0->0 legacyMutation=no
[JUNCTIONEPCLEANUP] INPUT flagCleanupBytes=8 flagCleanupFNV=46cb2547 unchanged=yes callerOrder=yes particleTopologyCanonical=yes activeList=0 freeList=64 totalPool=64
[JUNCTIONEPCLEANUP] FAILCLOSED nullFlag=1 nullOutput=1 inactiveFlag=1 targetMap=1 invalidEvents=1 invalidParticles=1 nonemptyEvents=1 nonemptyParticles=1 prepareAtomic=yes postActivePrepare=1 repeat=1 repeatAtomic=yes
[JUNCTIONEPCLEANUP] RESIDENT snapshotFNV=bb714d80->bb714d80 unchanged=yes mapFNV=8dba0bb4 automapFNV=b699bd75 runtimeFNV=bc432a0f scriptFNV=bc9b18ff lineFNV=3658710d textureFNV=537319ad topologyFNV=d6e8df7d payload=10410 entities=30 enemies=0 destructibles=3 packClosed=yes
[JUNCTIONEPCLEANUP] RAM heap8=72604->72604 delta=0 largest8=34804->34804 delta=0 persistentHeapBytes=0
[JUNCTIONEPCLEANUP] LEGACY gameFNV=6960d5bb->6960d5bb playerFNV=c64e7862->c64e7862 hudFNV=d2deba0f->d2deba0f canvasFNV=96abe6d0->96abe6d0 renderFNV=f9344dec->f9344dec frameFNV=805df09e->805df09e eventQueueFNV=d985589f->d985589f particleFNV=f186cf0c->f186cf0c legacyRuntimeClear=yes GameMutation=no PlayerMutation=no HudMutation=no DoomCanvasMutation=no RenderMutation=no ParticleSystemMutation=no legacyParticle_freeAllCalled=no
[JUNCTIONEPCLEANUP] PARK state=9 page=3 targetMap=9 junctionResident=yes nativePostLoadFlagCleanup=yes nativeEventParticleCleanup=yes initialSavePersistencePending=yes flagCleanupPending=no eventParticleCleanupPending=no isUpdateViewPending=yes ST_PLAYING=no entities=0 monsters=0 noGameplay=yes
```

Same-build legacy/frame/event/particle FNVs above are equality witnesses only,
not cross-build canons.

## RAM / integrity proof

```text
heap8=72604->72604
largest8=34804->34804
persistentHeapBytes=0
snapshotFNV=bb714d80->bb714d80
packClosed=yes
legacyRuntimeClear=yes
GameMutation=no
PlayerMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
ParticleSystemMutation=no
legacyParticle_freeAllCalled=no
```

Stable post-PARK heartbeat:

```text
heap=138368
heap8=72604
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

Mandatory invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
legacy Game.entities = 0
legacy Game.monsters = 0
ST_PLAYING = not reached
```

## Hardware-proven PARK

```text
state=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativePostLoadFlagCleanup=yes
nativeEventParticleCleanup=yes
initialSavePersistencePending=yes
flagCleanupPending=no
eventParticleCleanupPending=no
isUpdateViewPending=yes
ST_PLAYING=no
entities=0
monsters=0
noGameplay=yes
```

## Next exact caller boundary after merge

Own only:

```c
doomCanvas->isUpdateView = true;
```

Do not bundle `DoomCanvas_setState(ST_PLAYING)`, `idleTime`, durable native save
storage, gameplay entities or rendering into that milestone.
