# ESP32 native Junction post-load flag cleanup milestone

Branch: `agent/esp32-native-post-load-flag-cleanup`

Base merged `main`:

```text
PR   = #80 — post-load initial-save semantic intent
main = b669488c6f577d1004ac5a1dc742392698d66095
```

Hardware-tested firmware:

```text
7f16e08f6948da121815ba669fcbbff7e061e2b7
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

Recover only the three contiguous legacy scalar writes immediately after the
hardware-proven initial-save intent boundary:

```c
game->isLoaded = false;
game->isSaved = false;
game->activeLoadType = 0;
```

The exact legacy caller then continues with queued-event / particle cleanup, so
that later work is deliberately outside this milestone.

## Legacy semantics recovered

The three writes are strictly contiguous. No helper call or other side effect
occurs between them.

Relevant bookkeeping roles:

```text
Game.isLoaded       = load-state marker; fresh-map save is gated on !isLoaded
Game.isSaved        = legacy loading UI bookkeeping ("Game Saved" vs "Game Loaded")
Game.activeLoadType = remembered load-state type
```

The real CYD established that all three incoming values are already zero at this
native fresh-Junction boundary:

```text
isLoadedBefore=0
isSavedBefore=0
activeLoadTypeBefore=0
```

Therefore the exact legacy writes are semantic identity assignments on this
specific path. They still remain a real caller-order boundary and are represented
explicitly rather than silently skipped.

## Permanent owner

Files:

```text
ESP32/include/esp_post_load_flag_cleanup_state.h
ESP32/src/esp_post_load_flag_cleanup_state.c
```

Hardware-proven ABI:

```text
EspPostLoadFlagCleanupState = 8 B
stateFNV = 46cb2547
persistent heap = 0 B
```

State:

```text
isLoadedBefore=0
isSavedBefore=0
activeLoadTypeBefore=0
isLoadedAfter=0
isSavedAfter=0
activeLoadTypeAfter=0
targetMapId=9
active=1
```

The permanent implementation is legacy-engine free. It depends on the
hardware-proven `EspPostLoadInitialSaveIntentState`, receives three scalar input
values, validates the exact Junction/save-intent world boundary, and parks one
pointer-free state. It has no `Game_t`, filesystem, presentation, allocation,
particle or event dependency.

Permanent API:

```c
EspPostLoadFlagCleanup_reset()
EspPostLoadFlagCleanup_isReady()
EspPostLoadFlagCleanup_view()
EspPostLoadFlagCleanup_prepare()
EspPostLoadFlagCleanup_route()
```

## Strict predecessor boundary

The hardware probe requires the canonical initial-save owner:

```text
saveIntentBytes=24
saveIntentFNV=0bf1a911
mapId=9
view=992/1888
angle=64
isLoadedBefore=0
saveMode=0
saveRequired=1
componentMask=0f
persistenceDeferred=1
presentationDeferred=1
active=1
```

The existing persistence debt remains intentionally preserved. This cleanup does
not imply durable save storage has been implemented.

The resident Junction world remains canonical:

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
nullIntent=1
nullOutput=1
inactiveIntent=1
targetMap=1
invalidLoaded=1
invalidSaved=1
invalidLoadType=1
loadedMismatch=1
prepareAtomic=yes
postActivePrepare=1
repeat=1
repeatAtomic=yes
```

This proves invalid inputs do not park partial state, already-active state is
refused, and repeated routing is atomic.

## Real-CYD hardware evidence

The normal `esp32-cyd` firmware printed:

```text
[JUNCTIONFLAGCLEANUP] READY stateBytes=8 stateFNV=46cb2547 isLoaded=0->0 isSaved=0->0 activeLoadType=0->0 targetMap=9 active=1
[JUNCTIONFLAGCLEANUP] SEMANTIC isLoadedCleared=yes isSavedCleared=yes activeLoadTypeCleared=yes legacyValues=0/0/0->0/0/0 legacyMutation=no
[JUNCTIONFLAGCLEANUP] INPUT saveIntentBytes=24 saveIntentFNV=0bf1a911 unchanged=yes callerOrder=yes persistenceDebtPreserved=yes
[JUNCTIONFLAGCLEANUP] FAILCLOSED nullIntent=1 nullOutput=1 inactiveIntent=1 targetMap=1 invalidLoaded=1 invalidSaved=1 invalidLoadType=1 loadedMismatch=1 prepareAtomic=yes postActivePrepare=1 repeat=1 repeatAtomic=yes
[JUNCTIONFLAGCLEANUP] RESIDENT snapshotFNV=bb714d80->bb714d80 unchanged=yes mapFNV=8dba0bb4 automapFNV=b699bd75 runtimeFNV=bc432a0f scriptFNV=bc9b18ff lineFNV=3658710d textureFNV=537319ad topologyFNV=d6e8df7d payload=10410 entities=30 enemies=0 destructibles=3 packClosed=yes
[JUNCTIONFLAGCLEANUP] RAM heap8=72620->72620 delta=0 largest8=34804->34804 delta=0 persistentHeapBytes=0
[JUNCTIONFLAGCLEANUP] LEGACY gameFNV=6960d5bb->6960d5bb playerFNV=c64e7862->c64e7862 hudFNV=d2deba0f->d2deba0f canvasFNV=ade981cb->ade981cb renderFNV=f9344dec->f9344dec frameFNV=7a95b5b5->7a95b5b5 legacyRuntimeClear=yes GameMutation=no PlayerMutation=no HudMutation=no DoomCanvasMutation=no RenderMutation=no
[JUNCTIONFLAGCLEANUP] PARK state=9 page=3 targetMap=9 junctionResident=yes nativeInitialSaveIntent=yes nativePostLoadFlagCleanup=yes initialSavePersistencePending=yes flagCleanupPending=no eventParticleCleanupPending=yes isUpdateViewPending=yes ST_PLAYING=no entities=0 monsters=0 noGameplay=yes
```

The legacy/frame FNVs above are same-build equality witnesses only; they are not
cross-build canons.

## RAM proof

Normal `esp32-cyd`:

```text
heap8=72620->72620
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Stable post-PARK heartbeat:

```text
heap=138384
heap8=72620
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

## Integrity proof

The probe does not execute the legacy writes. It records their native semantic
equivalent while proving all legacy witnesses unchanged:

```text
GameMutation=no
PlayerMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
legacyRuntimeClear=yes
shapeData == NULL
mediaTexels == NULL
```

Resident state remains exactly `bb714d80` and the PAK is closed.

## Hardware-proven PARK

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

## Next exact caller boundary after merge

Own only the immediately following event/particle cleanup:

```c
doomCanvas->numEvents = 0;
ParticleSystem_freeAllParticles(doomCanvas->particleSystem);
doomCanvas->numEvents = 0;
```

Do not bundle `doomCanvas->isUpdateView = true`, `ST_PLAYING`, `idleTime`, native
durable save storage, gameplay entities or rendering into that milestone.
