# ESP32 native Junction post-load ST_PLAYING transition milestone

Branch: `agent/esp32-native-post-load-playing-transition`

Base merged `main`:

```text
PR   = #83 — post-load view invalidation
main = 4b5a9a368fbe4ee7938b2e3d11218b312d631f47
```

Hardware-tested firmware:

```text
afda93f0a28af5c34620fef2ac3354a24b3f91f5
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

Recover the complete relevant semantics of the exact caller boundary:

```c
DoomCanvas_setState(doomCanvas, ST_PLAYING);
```

for the hardware-proven fresh-Junction path, without calling the legacy function and without waking the pointer-heavy legacy gameplay renderer.

The immediately following caller write remains outside this milestone:

```c
doomCanvas->idleTime = doomCanvas->time + 8000;
```

Durable save persistence, native gameplay dispatch and native gameplay rendering also remain deferred.

## Exact recovered legacy semantics

For the proven incoming state `ST_INTRO`, `DoomCanvas_setState(..., ST_PLAYING)` performs the relevant effects:

```text
oldState = ST_INTRO
state = ST_PLAYING
restoreSoftKeys = false because state changed
if (!game->monstersTurn): request DoomCanvas_drawSoftKeys("Menu", "Map")
skipCheckState = true
```

`DoomCanvas_drawSoftKeys()` is not a pure state helper. When `displaySoftKeys` is enabled it mutates soft-key state and performs drawing through legacy HUD/video primitives. Therefore the permanent ESP32 path records the `Menu/Map` semantic intent instead of invoking that renderer/UI path.

The legacy `DoomCanvas.state` deliberately remains `ST_INTRO` in this milestone. Switching the legacy state to `ST_PLAYING` would cause the next legacy loop to enter `DoomCanvas_playingState()` and `Render_render()` while the legacy runtime is intentionally empty (`shapeData == NULL`, `mediaTexels == NULL`, entities/monsters zero). The native owner is the authoritative state transition.

## Permanent owner

Files:

```text
ESP32/include/esp_post_load_playing_transition_state.h
ESP32/src/esp_post_load_playing_transition_state.c
```

Hardware-proven ABI:

```text
EspPostLoadPlayingTransitionState = 12 B
stateFNV = 73bc9acd
persistent heap = 0 B
```

Real-CYD state:

```text
state=9->3
monstersTurn=0
displaySoftKeys=0
restoreSoftKeys=0->0
skipCheckState=0->1
softKeyIntent=1            # Menu/Map requested
softKeyPresentationDeferred=0
targetMap=9
active=1
```

`softKeyIntent=1` means `ESP_POST_LOAD_PLAYING_SOFTKEY_MENU_MAP`.

The real CYD established the important gate combination:

```text
monstersTurn=0      -> legacy would request drawSoftKeys("Menu", "Map")
displaySoftKeys=0   -> drawSoftKeys would not enter its drawing/mutation body
restoreSoftKeys=0->0
```

So the call request is semantically real while there is no visible soft-key presentation debt on this classic CYD path.

## Semantic proof

```text
nativeST_PLAYING=yes
legacyST_PLAYING=no
stateTransition=yes
softKeyCallRequested=yes
softKeyVisible=no
softKeyLabels=Menu/Map
restoreSoftKeysResult=0
skipCheckStateResult=1
legacyDoomCanvas_setStateCalled=no
legacyDoomCanvas_drawSoftKeysCalled=no
rendering=no
presentation=no
```

This is the first hardware-proven native `ST_PLAYING` state. It is not yet gameplay execution: the legacy state remains parked at `ST_INTRO`, and native gameplay/renderer dispatch remains a separate milestone.

## Strict predecessor boundary

The probe revalidated the exact view-invalidation owner:

```text
EspPostLoadViewInvalidationState = 4 B
stateFNV = 4561c3c1
isUpdateView=1->1
targetMap=9
active=1
unchanged=yes
callerOrder=yes
```

Particle topology also remained canonical:

```text
particleTopologyCanonical=yes
activeList=0
freeList=64
totalPool=64
```

## Fail-closed proof

Real-CYD:

```text
nullView=1
nullOutput=1
inactiveView=1
targetMap=1
invalidMonstersTurn=1
invalidDisplaySoftKeys=1
invalidRestoreSoftKeys=1
invalidSkipCheckState=1
prepareAtomic=yes
postActivePrepare=1
repeat=1
repeatAtomic=yes
```

Invalid scalar gates cannot park partial state, and already-active/repeat paths remain atomic.

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
heap8=72552->72552
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Stable post-PARK heartbeat:

```text
heap=138316
heap8=72552
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

## Legacy / framebuffer equality witnesses

Same-build equality witnesses only:

```text
gameFNV=002b366b->002b366b
playerFNV=c64e7862->c64e7862
hudFNV=d2deba0f->d2deba0f
canvasFNV=4331fadc->4331fadc
renderFNV=f9344dec->f9344dec
frameFNV=b8924a47->b8924a47
eventQueueFNV=d985589f->d985589f
particleFNV=f186cf0c->f186cf0c
legacyState=9->9
monstersTurn=0->0
displaySoftKeys=0->0
restoreSoftKeys=0->0
skipCheckState=0->0
legacyRuntimeClear=yes
GameMutation=no
PlayerMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
ParticleSystemMutation=no
legacyDoomCanvas_setStateCalled=no
legacyDoomCanvas_drawSoftKeysCalled=no
```

These FNVs are equality witnesses for this firmware only, not cross-build canons.

## Hardware-proven PARK

```text
legacyState=9
page=3
targetMap=9
junctionResident=yes
nativeViewInvalidation=yes
nativeST_PLAYING=yes
legacyST_PLAYING=no
initialSavePersistencePending=yes
ST_PLAYINGPending=no
idleTimePending=yes
rendererPending=yes
entities=0
monsters=0
noGameplay=yes
```

Mandatory memory/runtime invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
runtime ZIP map access forbidden
legacy Game.entities = 0
legacy Game.monsters = 0
```

## Next exact caller boundary after merge

Recover exact new `main`, then own only:

```c
doomCanvas->idleTime = doomCanvas->time + 8000;
```

Do not bundle native gameplay dispatch or renderer activation into that scalar caller-order milestone. After the post-load caller is complete, the next architectural work is a native PLAYING loop/input/render boundary that consumes the native ST_PLAYING owner without switching the legacy DoomCanvas into its renderer path.
