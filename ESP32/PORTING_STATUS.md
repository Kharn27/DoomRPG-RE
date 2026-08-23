# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #84 — native ST_PLAYING transition
main = 0a2cf860e074b19240f50fc65822710ab8d505bb
hardware-tested firmware = afda93f0a28af5c34620fef2ac3354a24b3f91f5
status = REAL-CYD HARDWARE PASS
```

Merged evidence: [`MAP1_NATIVE_POST_LOAD_PLAYING_TRANSITION.md`](MAP1_NATIVE_POST_LOAD_PLAYING_TRANSITION.md).

## Current merge-ready milestone

```text
branch = agent/esp32-native-post-load-idle-time
base   = 0a2cf860e074b19240f50fc65822710ab8d505bb
hardware-tested firmware = 1349ed314487bcade159ce92c6ad9c27b75735d5
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Evidence: [`MAP1_NATIVE_POST_LOAD_IDLE_TIME.md`](MAP1_NATIVE_POST_LOAD_IDLE_TIME.md).

This milestone owns only the final successful fresh-map caller write:

```c
doomCanvas->idleTime = doomCanvas->time + 8000;
```

The successful fresh-Junction `DoomCanvas_loadMedia()` caller tail is now semantically complete. The next architectural boundary is native PLAYING loop/input/render dispatch; do not switch legacy `DoomCanvas.state` to `ST_PLAYING`.

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
 -> post-load idle-time owner
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
```

The idle-time owner FNV is intentionally **not** a cross-boot canon because it includes the live uptime. The hardware-tested run produced `stateFNV=d6e95f57` at `timeBefore=4600`; future valid boots may differ while preserving `idleTimeAfter-timeBefore=8000`.

Generic `EspMapOpcodeExecutor` remains intentionally only 11/19/20.

## Hardware-proven native ST_PLAYING transition

```text
EspPostLoadPlayingTransitionState = 12 B
stateFNV=73bc9acd
persistentHeapBytes=0
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
```

Legacy `DoomCanvas.state` remains parked at `ST_INTRO` so the legacy loop cannot enter `DoomCanvas_playingState()` / `Render_render()`.

## Hardware-proven post-load idle time

Permanent files:

```text
ESP32/include/esp_post_load_idle_time_state.h
ESP32/src/esp_post_load_idle_time_state.c
```

Stable semantic owner:

```text
EspPostLoadIdleTimeState = 16 B
persistentHeapBytes=0
idleTimeAfter = timeBefore + 8000
targetMap=9
active=1
```

Real-CYD tested run:

```text
timeBefore=4600
idleTimeBefore=0
idleTimeAfter=12600
delta=8000
stateFNV=d6e95f57   # run-specific witness, not cross-boot canon
```

Semantic proof:

```text
nativeST_PLAYING=yes
loadTailComplete=yes
idleDeadlineOwned=yes
legacyTime=4600->4600
legacyIdleTime=0->0
legacyMutation=no
gameplayDispatch=no
rendering=no
presentation=no
```

Strict predecessor proof:

```text
playingBytes=12
playingFNV=73bc9acd
unchanged=yes
callerOrder=yes
nativeState=3
particleTopologyCanonical=yes
activeList=0
freeList=64
totalPool=64
```

Fail-closed proof:

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
heap8=72540->72540
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Same-build equality witnesses only:

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

Stable post-PARK heartbeat:

```text
heap=138304
heap8=72540
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

## Exact recovered fresh-map caller tail

Every successful fresh-Junction caller statement below is now hardware-proven semantically:

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
DoomCanvas.idleTime=DoomCanvas.time+8000    [hardware-proven native semantic owner]
return true                                 [caller tail complete]
```

## Current hardware PARK

```text
legacyState=9 / ST_INTRO
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
native PLAYING loop/input dispatch
full native entity/monster gameplay
native gameplay renderer / first gameplay presentation
sound playback
```

## Probe completion semantics

Historical temporary probes may set `done=1` on terminal failure. `*_isDone()` alone is not a PASS certificate. New downstream probes must revalidate exact predecessor owners/world state. The current idle-time probe sets `done=1` only after successful PARK.

## Merge recommendation

```text
MERGE agent/esp32-native-post-load-idle-time
```

Hardware-tested firmware:

```text
1349ed314487bcade159ce92c6ad9c27b75735d5
```

All commits after that tested SHA must remain documentation-only.

## Next bounded milestone after merge

Recover exact new `main`, then leave post-load caller recovery behind.

The next architecture milestone should introduce the smallest permanent **native PLAYING loop/dispatch boundary** that consumes `EspPostLoadPlayingTransitionState` and `EspPostLoadIdleTimeState` while keeping legacy `DoomCanvas.state=ST_INTRO`, legacy entities/monsters zero, `shapeData/mediaTexels == NULL`, and the legacy renderer dormant.

Do not bundle full gameplay or a complete renderer. First establish a bounded native PLAYING service/dispatch owner and prove its first no-input/no-gameplay iteration on the real CYD; rendering can then advance as a separate bounded milestone.
