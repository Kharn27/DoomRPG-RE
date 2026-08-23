# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #80 — post-load initial-save semantic intent
main = b669488c6f577d1004ac5a1dc742392698d66095
hardware-tested firmware = 0da9526775b706606338045babeb89e0d6c72729
status = REAL-CYD HARDWARE PASS
```

Merged evidence: [`MAP1_NATIVE_POST_LOAD_INITIAL_SAVE_INTENT.md`](MAP1_NATIVE_POST_LOAD_INITIAL_SAVE_INTENT.md).

## Current merge-ready milestone

```text
branch = agent/esp32-native-post-load-flag-cleanup
base   = b669488c6f577d1004ac5a1dc742392698d66095
hardware-tested firmware = 7f16e08f6948da121815ba669fcbbff7e061e2b7
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Evidence: [`MAP1_NATIVE_POST_LOAD_FLAG_CLEANUP.md`](MAP1_NATIVE_POST_LOAD_FLAG_CLEANUP.md).

This milestone owns only:

```c
game->isLoaded = false;
game->isSaved = false;
game->activeLoadType = 0;
```

The exact legacy writes are contiguous. The real CYD proved the incoming values
are already `0/0/0`, so this fresh-Junction path is an identity cleanup while
remaining an explicit caller-order boundary.

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
ST_PLAYING  = not reached
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
 -> 24 B fresh-map spawn projection
 -> 44 B active player/view owner
 -> 8 B post-spawn HUD dirty owner
 -> 24 B Player_setup session owner
 -> 24 B initial tile owner
 -> 24 B finishRotation orientation owner
 -> 24 B finishRotation second-tile owner
 -> 32 B durable facing owner
 -> 8 B post-load HUD-clear owner
 -> 16 B direct Junction GIVEMAP caller-order owner
 -> 8 B current-weapon self-select caller-order owner
 -> 24 B initial-save semantic intent owner
 -> 8 B post-load flag cleanup owner
```

Canonical fingerprints:

```text
levelExitStatsFNV              = bd41bcfa
playerExitAppliedFNV           = 298eaaa4
statsMenuIntentFNV             = 96afe901
catalogFNV                     = ce322e3f
transitionPreflightFNV         = 108e5c7b
committed WAIT_STATS FNV       = 66fe636a
committed READY FNV            = 0ef58ea8
committed ROLLBACK FNV         = 2dec1442
committed COMMITTED FNV        = 2c595a62
Junction spawn FNV             = ba6af4a7
packed override FNV            = e0a5110b
Junction player/view FNV       = d1131d18
packed override view FNV       = 9ed47d08
post-HUD player/view FNV       = d17fa0d1
Junction HUD refresh FNV       = 6965ee06
Player_setup semantic FNV      = 3b27c6a1
post-setup player/view FNV     = c21fba3c
Junction initial-tile FNV      = f73e28b2
post-initial-tile player FNV   = 1bd0f09b
Junction orientation FNV       = acc754a6
Junction second-tile FNV       = 09e58e0d
Junction durable-facing FNV    = 95aa1108
post-facing player/view FNV    = afcdcf74
Junction post-load HUD clear   = b7383e18
Junction post-load GIVEMAP     = 448e587d
Junction weapon self-select    = 699f3cf3
Junction initial-save intent   = 0bf1a911
Junction post-load flag cleanup= 46cb2547
```

Generic `EspMapOpcodeExecutor` remains intentionally only 11/19/20.

## Hardware-proven initial-save semantic intent

```text
EspPostLoadInitialSaveIntentState = 24 B
stateFNV=0bf1a911
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
persistentHeapBytes=0
```

Requested persistence components remain semantic debt only:

```text
CONFIG       requested
PLAYER2      requested
WORLD        requested
PLAYER_ROUTE requested
saveFileWrite=no
savingUi=no
presentation=no
routePayloadOwned=no
playerPersistence=no
worldPersistence=no
configPersistence=no
```

## Hardware-proven post-load flag cleanup

Permanent files:

```text
ESP32/include/esp_post_load_flag_cleanup_state.h
ESP32/src/esp_post_load_flag_cleanup_state.c
```

Owner:

```text
EspPostLoadFlagCleanupState = 8 B
stateFNV = 46cb2547
persistent heap = 0 B
```

Real-CYD state:

```text
isLoaded=0->0
isSaved=0->0
activeLoadType=0->0
targetMap=9
active=1
```

Semantic proof:

```text
isLoadedCleared=yes
isSavedCleared=yes
activeLoadTypeCleared=yes
legacyValues=0/0/0->0/0/0
legacyMutation=no
```

Input proof:

```text
saveIntentBytes=24
saveIntentFNV=0bf1a911
unchanged=yes
callerOrder=yes
persistenceDebtPreserved=yes
```

Fail-closed proof:

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
heap8=72620->72620
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Same-build equality witnesses only:

```text
gameFNV=6960d5bb->6960d5bb
playerFNV=c64e7862->c64e7862
hudFNV=d2deba0f->d2deba0f
canvasFNV=ade981cb->ade981cb
renderFNV=f9344dec->f9344dec
frameFNV=7a95b5b5->7a95b5b5
legacyRuntimeClear=yes
GameMutation=no
PlayerMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
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

## Probe completion semantics

Historical temporary probes may set `done=1` on terminal failure; `*_isDone()`
alone is not a PASS certificate. New downstream probes must revalidate exact
predecessor owners/world state. The flag-cleanup probe follows the new convention
and sets `done=1` only after successful PARK.

## Exact recovered caller order

```text
DoomCanvas_finishRotation()                  [hardware-proven complete]
Hud.msgCount=0                              [hardware-proven]
Hud.statBarMessage=NULL                     [hardware-proven]
Hud.logMessage[0]='\0'                     [hardware-proven]
if Junction: Game_givemap()                 [hardware-proven]
else: DoomCanvas_uncoverAutomap()
Player_selectWeapon(player, player->weapon) [hardware-proven]
conditional Game_saveState(...)             [hardware-proven semantic intent]
Game.isLoaded=false                         [hardware-proven semantic cleanup]
Game.isSaved=false                          [hardware-proven semantic cleanup]
Game.activeLoadType=0                       [hardware-proven semantic cleanup]
DoomCanvas.numEvents=0                      [NEXT after merge]
ParticleSystem_freeAllParticles(...)        [NEXT after merge]
DoomCanvas.numEvents=0                      [NEXT after merge]
DoomCanvas.isUpdateView=true                [deferred]
DoomCanvas_setState(ST_PLAYING)             [deferred]
idleTime=time+8000                          [deferred]
```

## Current hardware PARK

```text
state=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativeFacing=yes
nativeHudClear=yes
nativePostLoadGiveMap=yes
nativeWeaponSelfSelect=yes
nativeInitialSaveIntent=yes
nativePostLoadFlagCleanup=yes
initialSaveDecisionPending=no
initialSavePersistencePending=yes
flagCleanupPending=no
eventParticleCleanupPending=yes
isUpdateViewPending=yes
ST_PLAYING=no
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

## Still intentionally outside

```text
native durable save storage format
cross-map durable SAVEGAME route payload
full native player checkpoint persistence
full native world/entity persistence
queued-event / particle cleanup
isUpdateView caller write
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

## Merge recommendation

```text
MERGE agent/esp32-native-post-load-flag-cleanup
```

Hardware-tested firmware:

```text
7f16e08f6948da121815ba669fcbbff7e061e2b7
```

All commits after that tested SHA must remain documentation-only.

## Next bounded milestone after merge

Recover exact new `main`, then own only:

```c
doomCanvas->numEvents = 0;
ParticleSystem_freeAllParticles(doomCanvas->particleSystem);
doomCanvas->numEvents = 0;
```

Do not bundle `isUpdateView`, `ST_PLAYING`, `idleTime`, native durable save
storage, gameplay entities or rendering.
