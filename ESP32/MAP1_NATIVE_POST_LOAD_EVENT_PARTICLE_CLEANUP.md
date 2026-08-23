# ESP32 native Junction post-load event / particle cleanup milestone

Branch: `agent/esp32-native-post-load-event-particle-cleanup`

Base merged `main`:

```text
PR   = #81 — post-load flag cleanup
main = c4a093d9db77a715c355a68c5aae9faaddf22e0b
```

Status: **HARDWARE CANDIDATE — NOT YET CYD-PROVEN**.

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

`ST_PLAYING`, `idleTime`, rendering and durable native save storage are also
still deferred.

## Exact legacy particle behavior recovered

`ParticleSystem_freeAllParticles()` walks the active circular doubly-linked list
`nodeListA`. Every active `ParticleNode_t` is unlinked and inserted into the
`nodeListB` free list, then `particleCount` is forced to zero.

The legacy pool is pointer-heavy:

```text
nodeListA       active-list sentinel
nodeListB       free-list sentinel
nodeListC[64]   fixed ParticleNode_t pool
particleCount   active node count
```

This ownership model is useful as executable-spec behavior but is not a
permanent ESP32 runtime architecture.

## Critical fail-closed rule

The ESP32 native path does not yet own queued-event payloads or particle payloads.
Therefore this milestone supports only the already-empty caller boundary:

```text
numEventsBefore      must be 0
particleCountBefore  must be 0
```

If the real CYD reports any non-empty queued event or active particle, the probe
must FAIL and a dedicated ownership milestone is required. The candidate must
never silently discard unowned payloads.

Bounds are still validated independently:

```text
DoomCanvas.events capacity = 8
ParticleSystem node pool    = 64
```

## Permanent owner

Files:

```text
ESP32/include/esp_post_load_event_particle_cleanup_state.h
ESP32/src/esp_post_load_event_particle_cleanup_state.c
```

Candidate ABI:

```text
EspPostLoadEventParticleCleanupState = 8 B
persistent heap = 0 B
```

Fields:

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

For a successful current-path candidate, the semantic state must be:

```text
numEvents=0 -> 0 -> 0
particleCount=0 -> 0
targetMap=9
active=1
```

The state FNV is deliberately left for the real CYD to establish.

Permanent API:

```c
EspPostLoadEventParticleCleanup_reset()
EspPostLoadEventParticleCleanup_isReady()
EspPostLoadEventParticleCleanup_view()
EspPostLoadEventParticleCleanup_prepare()
EspPostLoadEventParticleCleanup_route()
```

The permanent implementation contains no `DoomCanvas_t`, `ParticleSystem_t`,
`ParticleNode_t`, `Game_t`, filesystem, presentation or allocation dependency.

## Strict predecessor boundary

The candidate requires the exact hardware-proven flag-cleanup owner:

```text
EspPostLoadFlagCleanupState = 8 B
stateFNV = 46cb2547
isLoaded=0->0
isSaved=0->0
activeLoadType=0->0
targetMap=9
active=1
```

and the canonical Junction resident world:

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

The initial-save persistence debt remains intentionally preserved.

## Temporary hardware probe

Files:

```text
ESP32/include/native_junction_post_load_event_particle_cleanup_probe.h
ESP32/src/native_junction_post_load_event_particle_cleanup_probe.c
```

The probe runs after the flag-cleanup probe and revalidates the predecessor
owner/world boundary rather than trusting `isDone()` alone.

It reads the legacy event/particle state only as hardware scaffolding. It never
calls `ParticleSystem_freeAllParticles()` and never writes `numEvents`.

### Particle topology proof

Before accepting an empty particle count, the probe walks both legacy circular
lists with a strict bound of 64 nodes and proves:

```text
all next/prev links are reciprocal
all nodes belong to nodeListC[64]
no node appears twice
activeCount == particleCount
activeCount + freeCount == 64
all 64 pool nodes are seen exactly once
```

This prevents a stale `particleCount=0` from being mistaken for an empty active
list.

### Fail-closed/deferred tests

The probe exercises:

```text
null flag-cleanup owner
null output
inactive predecessor owner
wrong target map
numEvents > 8
particleCount > 64
non-empty queued events
non-empty active particles
pure prepare atomicity
post-active prepare refusal
repeat-route atomicity
```

Non-empty payload tests must return explicit refusal statuses and park no state.

## Hardware acceptance

The real normal `esp32-cyd` firmware must establish:

```text
stateBytes=8
stateFNV=<hardware>
numEvents=0->0->0
particleCount=0->0
targetMap=9
active=1
```

Semantic acceptance:

```text
eventQueueEmpty=yes
particleActiveListEmpty=yes
identityCleanup=yes
legacyMutation=no
```

Particle topology acceptance:

```text
particleTopologyCanonical=yes
activeList=0
freeList=64
totalPool=64
```

Predecessor proof:

```text
flagCleanupBytes=8
flagCleanupFNV=46cb2547
unchanged=yes
callerOrder=yes
```

Resident state must remain unchanged:

```text
snapshotFNV=bb714d80 -> bb714d80
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

RAM / integrity acceptance:

```text
heap8 delta=0
largest8 delta=0
persistentHeapBytes=0
framebuffer unchanged
event queue bytes unchanged
legacy ParticleSystem unchanged
Game unchanged
Player unchanged
Hud unchanged
DoomCanvas unchanged
Render unchanged
legacy runtime remains clear
legacy ParticleSystem_freeAllParticles not called
shapeData == NULL
mediaTexels == NULL
```

## Expected Serial block

```text
=== Doom RPG ESP32-native Junction post-load event particle cleanup ===
[JUNCTIONEPCLEANUPPROBE] CONTRACT ...
[JUNCTIONEPCLEANUP] READY ...
[JUNCTIONEPCLEANUP] SEMANTIC ...
[JUNCTIONEPCLEANUP] INPUT ...
[JUNCTIONEPCLEANUP] FAILCLOSED ...
[JUNCTIONEPCLEANUP] RESIDENT ...
[JUNCTIONEPCLEANUP] RAM ...
[JUNCTIONEPCLEANUP] LEGACY ...
[JUNCTIONEPCLEANUP] PARK ...
```

The probe uses `done=1` only after successful final PARK.

## Candidate PARK

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

## Next exact caller boundary after PASS + merge

Own only:

```c
doomCanvas->isUpdateView = true;
```

Do not bundle `DoomCanvas_setState(ST_PLAYING)`, `idleTime`, durable native save
storage, gameplay entities or rendering into that milestone.
