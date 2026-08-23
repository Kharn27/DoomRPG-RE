# ESP32 native Junction post-load view invalidation milestone

Branch: `agent/esp32-native-post-load-view-invalidation`

Base merged `main`:

```text
PR   = #82 — post-load event / particle cleanup
main = c9d0a3fdc705acdbb613beccb17de4d98af218c3
```

Hardware-tested firmware:

```text
25976e82976bf7ed78b0506640db62bd0779ec5f
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

Recover only the exact caller write immediately after the hardware-proven empty event / particle cleanup:

```c
doomCanvas->isUpdateView = true;
```

The next caller boundary remains deliberately outside this milestone:

```c
DoomCanvas_setState(doomCanvas, ST_PLAYING);
```

`idleTime = time + 8000`, rendering, presentation and durable native save storage also remain deferred.

## Exact legacy semantics

`isUpdateView` is the DoomCanvas redraw-request flag. `DoomCanvas_updateView()` consumes it by returning early when false and temporarily clearing it while updating the view. The load-map caller explicitly writes it to true immediately before entering `ST_PLAYING`.

The real CYD established that at this fresh-Junction boundary the incoming value is already true:

```text
isUpdateViewBefore=1
isUpdateViewAfter=1
```

Therefore the legacy assignment is an identity write on this path, while remaining a real caller-order boundary.

## Permanent owner

Files:

```text
ESP32/include/esp_post_load_view_invalidation_state.h
ESP32/src/esp_post_load_view_invalidation_state.c
```

Hardware-proven ABI:

```text
EspPostLoadViewInvalidationState = 4 B
stateFNV = 4561c3c1
persistent heap = 0 B
```

State:

```text
isUpdateViewBefore=1
isUpdateViewAfter=1
targetMapId=9
active=1
```

The permanent implementation is pointer-free and legacy-engine free. It does not depend on `DoomCanvas_t`, `Render_t`, framebuffer presentation, filesystem, heap allocation or gameplay entities.

## Strict predecessor boundary

The hardware probe requires the exact event / particle cleanup owner:

```text
EspPostLoadEventParticleCleanupState = 8 B
stateFNV = 8bc79e2b
numEvents=0->0->0
particleCount=0->0
targetMap=9
active=1
```

The probe also revalidates the legacy particle topology:

```text
particleTopologyCanonical=yes
activeList=0
freeList=64
totalPool=64
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

## Fail-closed proof

The real CYD printed:

```text
nullCleanup=1
nullOutput=1
inactiveCleanup=1
targetMap=1
invalidValue=1
prepareAtomic=yes
postActivePrepare=1
repeat=1
repeatAtomic=yes
```

Invalid input cannot park partial state, already-active state is refused, and repeat routing is atomic.

## Real-CYD hardware evidence

Normal `esp32-cyd` firmware:

```text
[JUNCTIONVIEWINVALIDATE] READY stateBytes=4 stateFNV=4561c3c1 isUpdateView=1->1 targetMap=9 active=1
[JUNCTIONVIEWINVALIDATE] SEMANTIC redrawRequested=yes identityAssignment=yes edgeTransition=no legacyIsUpdateView=1->1 legacyMutation=no renderTriggered=no presentation=no
[JUNCTIONVIEWINVALIDATE] INPUT eventParticleBytes=8 eventParticleFNV=8bc79e2b unchanged=yes callerOrder=yes particleTopologyCanonical=yes activeList=0 freeList=64 totalPool=64
[JUNCTIONVIEWINVALIDATE] FAILCLOSED nullCleanup=1 nullOutput=1 inactiveCleanup=1 targetMap=1 invalidValue=1 prepareAtomic=yes postActivePrepare=1 repeat=1 repeatAtomic=yes
[JUNCTIONVIEWINVALIDATE] RESIDENT snapshotFNV=bb714d80->bb714d80 unchanged=yes mapFNV=8dba0bb4 automapFNV=b699bd75 runtimeFNV=bc432a0f scriptFNV=bc9b18ff lineFNV=3658710d textureFNV=537319ad topologyFNV=d6e8df7d payload=10410 entities=30 enemies=0 destructibles=3 packClosed=yes
[JUNCTIONVIEWINVALIDATE] RAM heap8=72588->72588 delta=0 largest8=34804->34804 delta=0 persistentHeapBytes=0
[JUNCTIONVIEWINVALIDATE] LEGACY gameFNV=6960d5bb->6960d5bb playerFNV=c64e7862->c64e7862 hudFNV=d2deba0f->d2deba0f canvasFNV=d140bc71->d140bc71 renderFNV=f9344dec->f9344dec frameFNV=faa62417->faa62417 eventQueueFNV=d985589f->d985589f particleFNV=f186cf0c->f186cf0c legacyRuntimeClear=yes GameMutation=no PlayerMutation=no HudMutation=no DoomCanvasMutation=no RenderMutation=no ParticleSystemMutation=no legacyDoomCanvas_updateViewTrueCalled=no
[JUNCTIONVIEWINVALIDATE] PARK state=9 page=3 targetMap=9 junctionResident=yes nativeEventParticleCleanup=yes nativeViewInvalidation=yes initialSavePersistencePending=yes eventParticleCleanupPending=no isUpdateViewPending=no ST_PLAYINGPending=yes idleTimePending=yes ST_PLAYING=no entities=0 monsters=0 noGameplay=yes
```

Same-build legacy/frame/event/particle FNVs above are equality witnesses only, not cross-build canons.

## RAM / integrity proof

```text
heap8=72588->72588
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
legacyDoomCanvas_updateViewTrueCalled=no
renderTriggered=no
presentation=no
```

Stable post-PARK heartbeat:

```text
heap=138352
heap8=72588
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
nativeEventParticleCleanup=yes
nativeViewInvalidation=yes
initialSavePersistencePending=yes
eventParticleCleanupPending=no
isUpdateViewPending=no
ST_PLAYINGPending=yes
idleTimePending=yes
ST_PLAYING=no
entities=0
monsters=0
noGameplay=yes
```

## Next exact caller boundary after merge

Recover the exact semantics of:

```c
DoomCanvas_setState(doomCanvas, ST_PLAYING);
```

This is a qualitatively larger boundary than the preceding scalar owners because `DoomCanvas_setState()` has state-transition behavior. Recover its exact `ST_INTRO -> ST_PLAYING` effects before implementing anything. Do not bundle `idleTime = time + 8000`, rendering, durable save storage, entity gameplay or renderer migration into that milestone unless the legacy function itself proves a side effect is inseparable from the state transition.
