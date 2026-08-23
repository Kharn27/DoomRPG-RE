# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #79 — post-load current-weapon self-select
main = 04e4e2269a6c70db3f3e4027717bdb36f286ce65
hardware-tested firmware = 24fb8fbf914820500d2e16815e22beb0439c9ba0
status = REAL-CYD HARDWARE PASS
```

Merged evidence: [`MAP1_NATIVE_POST_LOAD_WEAPON_SELF_SELECT.md`](MAP1_NATIVE_POST_LOAD_WEAPON_SELF_SELECT.md).

## Current merge-ready milestone

```text
branch = agent/esp32-native-post-load-initial-save-intent
base   = 04e4e2269a6c70db3f3e4027717bdb36f286ce65
hardware-tested firmware = 0da9526775b706606338045babeb89e0d6c72729
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Evidence: [`MAP1_NATIVE_POST_LOAD_INITIAL_SAVE_INTENT.md`](MAP1_NATIVE_POST_LOAD_INITIAL_SAVE_INTENT.md).

This milestone owns only the semantic boundary for the fresh Junction caller:

```c
if ((doomCanvas->loadMapID != MAP_END_GAME) &&
    (game->isLoaded == false)) {
    Game_saveState(game,
                   doomCanvas->loadMapID,
                   doomCanvas->viewX,
                   doomCanvas->viewY,
                   doomCanvas->viewAngle,
                   false);
}
```

The native path reconstructs map/view arguments from the hardware-proven
`EspPlayerView`, samples only `Game.isLoaded` read-only, and parks a compact save
intent. It does not emulate legacy Config/Player/Player2/World files.

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

Current post-GIVEMAP resident owner FNVs:

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
```

Generic `EspMapOpcodeExecutor` remains intentionally only 11/19/20.

## Hardware-proven initial-save semantic intent

Permanent files:

```text
ESP32/include/esp_post_load_initial_save_intent.h
ESP32/src/esp_post_load_initial_save_intent.c
```

Owner:

```text
EspPostLoadInitialSaveIntentState = 24 B
stateFNV = 0bf1a911
persistent heap = 0 B
```

Real-CYD state:

```text
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

Component intent:

```text
CONFIG       = requested
PLAYER2      = requested
WORLD        = requested
PLAYER_ROUTE = requested
saveFileWrite=no
savingUi=no
presentation=no
```

Input-owner proof:

```text
weaponFNV=699f3cf3
giveMapFNV=448e587d
hudClearFNV=b7383e18
viewFNV=afcdcf74
facingFNV=95aa1108
unchanged=yes
callerOrder=yes
```

Fail-closed/deferred proof:

```text
nullWeapon=1
nullView=1
nullOutput=1
inactiveWeapon=1
weaponMismatch=1
inactiveView=1
viewPending=1
loadedContextDeferred=1
invalidLoaded=1
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
heap8=72644->72644
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Same-build legacy/frame equality witnesses:

```text
gameFNV=6960d5bb->6960d5bb
playerFNV=c64e7862->c64e7862
hudFNV=b18611d2->b18611d2
canvasFNV=592d59c9->592d59c9
renderFNV=f9344dec->f9344dec
frameFNV=6a0726c1->6a0726c1
legacyRuntimeClear=yes
legacyGame_saveStateCalled=no
GameMutation=no
PlayerMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
```

Persistence debt remains explicit:

```text
legacyNewMapPresent=no
legacyNewDest=0/0
legacyNewAngle=0
routePayloadOwned=no
playerPersistence=no
worldPersistence=no
configPersistence=no
```

Stable post-PARK heartbeat:

```text
heap=138408
heap8=72644
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

## SD test incident

An earlier boot of the same firmware showed `SD=unavailable`, cascading into
ZIP/layout/render/menu startup skips. The branch had not modified the SD/build
wiring path and the new milestone cannot execute before SD initialization. The
user confirmed the microSD card was the cause; after correcting it, the same
firmware reached the full successful save-intent PARK. No SD workaround was
introduced.

## Historical SAVEGAME route correction

Opcode 27 parsing itself remains hardware-proven. The old temporary MAP_INTRO
route probe resets its sampled owner before PARK, so historical
`routeLifetimeCrossMap=yes` wording is not proof of a live owner surviving the
map handoff. A future durable save implementation must explicitly own that route.

## Probe completion semantics

Older temporary probes may set `done=1` on terminal failure. `*_isDone()` alone
is not a PASS certificate. The initial-save-intent probe revalidates all exact
predecessor owners/world state and sets its own `done=1` only after successful
PARK.

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
Game.isLoaded=false                         [NEXT after merge]
Game.isSaved=false                          [NEXT after merge]
Game.activeLoadType=0                       [NEXT after merge]
numEvents=0 / particles cleared             [deferred]
isUpdateView=true                           [deferred]
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
initialSaveDecisionPending=no
initialSavePersistencePending=yes
initialSavePending=yes   # persistence debt only
postLoadCleanupPending=yes
ST_PLAYING=no
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

The persistence debt is explicit but does not need to block later gameplay
bring-up once the save side effect has a permanent semantic owner.

## Still intentionally outside

```text
native durable save storage format
cross-map durable SAVEGAME route payload
full native player checkpoint persistence
full native world/entity persistence
post-load flag/event/particle cleanup
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

## Merge recommendation

```text
MERGE agent/esp32-native-post-load-initial-save-intent
```

Hardware-tested firmware:

```text
0da9526775b706606338045babeb89e0d6c72729
```

All commits after that tested SHA must remain documentation-only.

## Next bounded milestone after merge

Recover exact new `main`, then own only:

```c
game->isLoaded = false;
game->isSaved = false;
game->activeLoadType = 0;
```

Do not bundle queued-event/particle cleanup, `isUpdateView`, `ST_PLAYING`,
rendering or native durable save storage.
