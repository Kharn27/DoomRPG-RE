# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #83 — post-load view invalidation
main = 4b5a9a368fbe4ee7938b2e3d11218b312d631f47
hardware-tested firmware = 25976e82976bf7ed78b0506640db62bd0779ec5f
status = REAL-CYD HARDWARE PASS
```

Merged evidence: [`MAP1_NATIVE_POST_LOAD_VIEW_INVALIDATION.md`](MAP1_NATIVE_POST_LOAD_VIEW_INVALIDATION.md).

## Current merge-ready milestone

```text
branch = agent/esp32-native-post-load-playing-transition
base   = 4b5a9a368fbe4ee7938b2e3d11218b312d631f47
hardware-tested firmware = afda93f0a28af5c34620fef2ac3354a24b3f91f5
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Evidence: [`MAP1_NATIVE_POST_LOAD_PLAYING_TRANSITION.md`](MAP1_NATIVE_POST_LOAD_PLAYING_TRANSITION.md).

This milestone owns the complete relevant fresh-Junction semantics of:

```c
DoomCanvas_setState(doomCanvas, ST_PLAYING);
```

without calling legacy `DoomCanvas_setState()` or `DoomCanvas_drawSoftKeys()` and without switching the legacy loop into `DoomCanvas_playingState()`.

The real CYD has now reached **native ST_PLAYING**. Legacy `DoomCanvas.state` remains deliberately parked at `ST_INTRO` until native gameplay/render dispatch exists.

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
```

Canonical fingerprints:

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
```

Generic `EspMapOpcodeExecutor` remains intentionally only 11/19/20.

## Hardware-proven native ST_PLAYING transition

Permanent files:

```text
ESP32/include/esp_post_load_playing_transition_state.h
ESP32/src/esp_post_load_playing_transition_state.c
```

Owner:

```text
EspPostLoadPlayingTransitionState = 12 B
stateFNV=73bc9acd
persistentHeapBytes=0
```

Real-CYD state:

```text
state=9->3
monstersTurn=0
displaySoftKeys=0
restoreSoftKeys=0->0
skipCheckState=0->1
softKeyIntent=1
softKeyPresentationDeferred=0
targetMap=9
active=1
```

`softKeyIntent=1` is the permanent `Menu/Map` intent. Because `displaySoftKeys=0` on the classic CYD, the legacy `DoomCanvas_drawSoftKeys("Menu", "Map")` call would not enter its drawing/mutation body. Thus the call request is semantically present but has no visible presentation debt on this path.

Semantic proof:

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

Strict predecessor proof:

```text
viewInvalidationBytes=4
viewInvalidationFNV=4561c3c1
unchanged=yes
callerOrder=yes
isUpdateView=1->1
particleTopologyCanonical=yes
activeList=0
freeList=64
totalPool=64
```

Fail-closed proof:

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

Resident integrity:

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

Normal-env RAM proof:

```text
heap8=72552->72552
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

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

## Why legacy state remains ST_INTRO

Legacy `DoomCanvas_run()` dispatches `ST_PLAYING` directly into `DoomCanvas_playingState()`, which reaches `Render_render()` and other legacy gameplay/render state. That runtime is intentionally absent on ESP32. Therefore this milestone proves the permanent native state transition while leaving the temporary legacy canvas parked at state 9.

This is not a rollback or a pending transition: `ST_PLAYINGPending=no`. The native owner is authoritative for the recovered transition; only native gameplay/renderer dispatch is still pending.

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
DoomCanvas.numEvents=0                      [hardware-proven semantic cleanup]
ParticleSystem_freeAllParticles(...)        [hardware-proven semantic cleanup]
DoomCanvas.numEvents=0                      [hardware-proven semantic cleanup]
DoomCanvas.isUpdateView=true                [hardware-proven semantic owner]
DoomCanvas_setState(ST_PLAYING)             [hardware-proven native semantic owner]
idleTime=time+8000                          [NEXT after merge]
```

## Current hardware PARK

```text
legacyState=9 / ST_INTRO
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
idleTime=time+8000
native PLAYING loop/input dispatch
full native entity/monster gameplay
native gameplay renderer / first gameplay presentation
sound playback
```

## Probe completion semantics

Historical temporary probes may set `done=1` on terminal failure. `*_isDone()` alone is not a PASS certificate. New downstream probes must revalidate exact predecessor owners/world state. The current ST_PLAYING probe sets `done=1` only after successful PARK.

## Merge recommendation

```text
MERGE agent/esp32-native-post-load-playing-transition
```

Hardware-tested firmware:

```text
afda93f0a28af5c34620fef2ac3354a24b3f91f5
```

All commits after that tested SHA must remain documentation-only.

## Next bounded milestone after merge

Recover exact new `main`, then own only:

```c
doomCanvas->idleTime = doomCanvas->time + 8000;
```

Keep native gameplay dispatch and renderer activation separate. Once that final post-load caller write is hardware-proven, the fresh-Junction `DoomCanvas_loadMedia()` caller tail is semantically complete and the next architectural boundary is a native PLAYING loop/render path that consumes `EspPostLoadPlayingTransitionState` without entering legacy `DoomCanvas_playingState()`.
