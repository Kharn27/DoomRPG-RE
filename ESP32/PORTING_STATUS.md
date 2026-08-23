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

## Current hardware candidate

```text
branch = agent/esp32-native-post-load-initial-save-intent
base   = 04e4e2269a6c70db3f3e4027717bdb36f286ce65
status = HARDWARE CANDIDATE — NOT YET CYD-PROVEN
```

Candidate: [`MAP1_NATIVE_POST_LOAD_INITIAL_SAVE_INTENT.md`](MAP1_NATIVE_POST_LOAD_INITIAL_SAVE_INTENT.md).

The candidate owns only the semantic boundary for:

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

It does **not** emulate legacy Config/Player/Player2/World files. It captures the
caller request in a compact native intent and records persistence/presentation as
explicitly deferred side effects.

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
```

Generic `EspMapOpcodeExecutor` remains intentionally only 11/19/20.

## Last hardware-proven caller boundary: weapon self-select

```text
EspPostLoadWeaponSelectState = 8 B
stateFNV = 699f3cf3
persistent heap = 0 B
weaponBefore=2
requestedWeapon=2
weaponAfter=2
viewInvalidationRequested=0
selfSelect=yes
identityAssignment=yes
updateViewBranchTaken=no
```

Input owners remained:

```text
giveMapFNV=448e587d
hudClearFNV=b7383e18
viewFNV=afcdcf74
facingFNV=95aa1108
```

Resident stayed exactly `bb714d80`; normal-env RAM stayed
`heap8=72684`, `largest8=34804`, both delta zero.

## Current initial-save intent design

### Exact legacy orchestration recovered

`Game_saveState(game, mapId, x, y, angle, z)` currently performs:

```text
Saving... UI / present
Game_saveConfig(game, z)
if !z: Game_savePlayerState("Player2", current map, x, y, angle)
Game_saveWorldState(game)
if !z: Game_savePlayerState("Player", pending route or Junction fallback)
```

The current caller uses `z=false`.

The four persistence components are represented as a compact mask:

```text
0x01 CONFIG
0x02 PLAYER2
0x04 WORLD
0x08 PLAYER_ROUTE
0x0f ALL
```

### Why persistence is deferred

Legacy `Game_saveWorldState()` traverses pointer-heavy `Entity_t` topology while
the ESP32 native path intentionally has no legacy entity graph. Native player
ownership is also not yet complete. Copying these legacy file formats would make
temporary desktop architecture permanent on a no-PSRAM target.

The permanent ESP32 boundary therefore owns the request only:

```text
EspPostLoadInitialSaveIntentState = 24 B candidate
persistent heap = 0 B
```

Fields:

```text
viewX/viewY/viewAngle as int32
mapId
isLoadedBefore
saveMode
saveRequired
componentMask
persistenceDeferred
presentationDeferred
active
reserved[4]
```

Current already-proven native caller arguments are:

```text
mapId=9
viewX=992
viewY=1888
viewAngle=64
```

The probe samples only legacy `Game.isLoaded` read-only. Live legacy
`DoomCanvas.view*`/`loadMapID` are not native source-of-truth because the native
path deliberately did not apply legacy placement writes.

### Current candidate gates

The permanent owner requires:

```text
weapon self-select semantic owner active and self-select exact
targetMapId=9
gameplayLoadMapId=2
loadType=0
PlayerView active/spawnApplied
all PlayerView follow-up pending bits consumed
isLoadedBefore=0
runtimeFNV=bc432a0f
mapFNV=8dba0bb4
automapFNV=b699bd75
```

`isLoadedBefore=1` is explicitly returned as `LOADED_CONTEXT_DEFERRED`; restore
semantics are not silently generalized from this fresh-load milestone.

### Hardware candidate acceptance

Expected state semantics:

```text
stateBytes=24
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

The real CYD establishes the new 24 B state FNV.

Required unchanged input owners:

```text
weaponFNV=699f3cf3
giveMapFNV=448e587d
hudClearFNV=b7383e18
viewFNV=afcdcf74
facingFNV=95aa1108
```

Required unchanged resident:

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
packClosed=yes
```

Required side-effect proof:

```text
saveFileWrite=no
savingUi=no
presentation=no
legacy Game_saveState not called
heap/largest delta=0
frame unchanged
legacy Game/Player/Hud/DoomCanvas/Render unchanged
ST_PLAYING=no
legacy Game.entities=0
legacy Game.monsters=0
```

## Historical SAVEGAME route correction

Opcode 27 parsing itself remains hardware-proven. However, the old temporary
`native_map1_save_route_probe.c` resets its probe-local sampled route before its
PARK. Therefore the old diagnostic wording `routeLifetimeCrossMap=yes` must not
be interpreted as a live owner instance surviving the resident handoff.

A future durable save implementation must explicitly own that cross-map route.
The current initial-save intent records the `PLAYER_ROUTE` component request but
does not fabricate route payload that is no longer live.

## Probe completion semantics

Older temporary probes may set `done=1` on terminal failures. `*_isDone()` alone
is not a PASS certificate. Every downstream candidate must revalidate exact
owners/world state.

The new initial-save-intent probe changes convention locally: its `done=1` is set
only after successful final PARK.

## Exact recovered caller order

```text
DoomCanvas_finishRotation()                  [hardware-proven complete]
Hud.msgCount=0                              [hardware-proven]
Hud.statBarMessage=NULL                     [hardware-proven]
Hud.logMessage[0]='\0'                     [hardware-proven]
if Junction: Game_givemap()                 [hardware-proven]
else: DoomCanvas_uncoverAutomap()
Player_selectWeapon(player, player->weapon) [hardware-proven]
conditional Game_saveState(...)             [CURRENT INTENT CANDIDATE]
Game.isLoaded=false                         [next after PASS/merge]
Game.isSaved=false                          [next after PASS/merge]
Game.activeLoadType=0                       [next after PASS/merge]
numEvents=0 / particles cleared             [deferred]
isUpdateView=true                           [deferred]
DoomCanvas_setState(ST_PLAYING)             [deferred]
idleTime=time+8000                          [deferred]
```

## Current hardware PARK before candidate

```text
state=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativeFacing=yes
nativeHudClear=yes
nativePostLoadGiveMap=yes
nativeWeaponSelfSelect=yes
Game_givemapPending=no
weaponReselectPending=no
initialSavePending=yes
postLoadCleanupPending=yes
ST_PLAYING=no
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

Candidate successful PARK adds:

```text
nativeInitialSaveIntent=yes
initialSaveDecisionPending=no
initialSavePersistencePending=yes
postLoadCleanupPending=yes
ST_PLAYING=no
```

The persistence debt is explicit but does not need to block native gameplay
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

## Next test

Build/flash normal `esp32-cyd` from the current candidate branch and return the
complete `[JUNCTIONSAVEINTENT]` Serial block. Promote only after real-CYD PASS.

## Next bounded milestone after PASS + merge

Recover exact new `main`, then own only:

```c
game->isLoaded = false;
game->isSaved = false;
game->activeLoadType = 0;
```

Do not bundle event/particle cleanup, `isUpdateView`, `ST_PLAYING`, rendering or
native durable save storage.
