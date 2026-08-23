# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #78 — direct Junction post-load Game_givemap
main = 4737b016d02615b8435cf84909fe3c251b6d338b
hardware-tested firmware = 511156120bd877367d13ffa4b98ed6815005bc3c
status = REAL-CYD HARDWARE PASS
```

Merged evidence: [`MAP1_NATIVE_POST_LOAD_GIVEMAP.md`](MAP1_NATIVE_POST_LOAD_GIVEMAP.md).

The native load caller is hardware-proven through direct Junction
`Game_givemap()` and remains parked before `Player_selectWeapon()`.

## Current hardware candidate

```text
branch = agent/esp32-native-post-load-weapon-self-select
base   = 4737b016d02615b8435cf84909fe3c251b6d338b
status = HARDWARE CANDIDATE — NOT YET CYD-PROVEN
```

Candidate: [`MAP1_NATIVE_POST_LOAD_WEAPON_SELF_SELECT.md`](MAP1_NATIVE_POST_LOAD_WEAPON_SELF_SELECT.md).

This milestone owns only:

```c
Player_selectWeapon(player, player->weapon);
```

Exact recovered legacy implementation:

```c
if (player->weapon != i) {
    DoomCanvas_updateViewTrue(player->doomRpg->doomCanvas);
}
player->weapon = i;
```

At this caller `i == player->weapon`, so the view-update branch is not taken and
the assignment is identity. No ammo/inventory/weapon-gameplay owner is introduced.

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

Pre-GIVEMAP Junction resident owners:

```text
runtime  = bc432a0f
map      = c5cdfc04
script   = bc9b18ff
line     = 3658710d
texture  = 537319ad
automap  = 0b2ae445
topology = d6e8df7d
snapshot = bc9071e9
```

Current post-GIVEMAP Junction resident owners:

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
```

Generic `EspMapOpcodeExecutor` remains intentionally only 11/19/20.

## Last hardware-proven caller boundary: direct Junction GIVEMAP

Permanent shared primitive:

```text
EspMapGiveMapDirectResult = 12 B
EspMapAutomapState_planGiveMapDirect()
EspMapAutomapState_applyGiveMapDirect()
```

Caller-order owner:

```text
EspPostLoadGiveMapState = 16 B
stateFNV = 448e587d
persistent heap = 0 B
```

Real-CYD state:

```text
lineTargets=198
spriteTargets=48
entranceTargets=15
linesMutated=198
spritesMutated=48
tilesMutated=15
targetMap=9
gameplayLoadMapId=2
loadType=0
active=1
```

World transition:

```text
mapStateFNV  c5cdfc04 -> 8dba0bb4
automapFNV   0b2ae445 -> b699bd75
snapshotFNV  bc9071e9 -> bb714d80
allTargetsRevealed=yes
idempotentPlan=yes
nonTargetOwnersUnchanged=yes
```

Normal-env RAM proof:

```text
heap8=72700->72700
largest8=34804->34804
persistentHeapBytes=0
```

Historical MAP_INTRO opcode-9 regression is transitively hardware-proven by the
strict probe chain: downstream SAVEGAME/CHANGEMAP/Junction stages cannot run
unless the GIVEMAP probe reached its successful `done=1` PARK.

## Current weapon self-select implementation boundary

Permanent files:

```text
ESP32/include/esp_post_load_weapon_select_state.h
ESP32/src/esp_post_load_weapon_select_state.c
```

Candidate owner:

```text
EspPostLoadWeaponSelectState = 8 B
persistent heap = 0 B
```

Fields:

```text
weaponBefore
requestedWeapon
weaponAfter
viewInvalidationRequested
targetMapId
gameplayLoadMapId
loadType
active
```

Pure preparation requires:

```text
post-load GIVEMAP owner exact 198/48/15 mutation state
map identity 9 / gameplayLoadMapId 2 / loadType 0
sourceBytes=21051
sourceCrc32=4a2c5800
runtimeFNV=bc432a0f
post-GIVEMAP mapStateFNV=8dba0bb4
post-GIVEMAP automapFNV=b699bd75
current weapon in legacy range 0..11
```

The API deliberately has no separate requested-weapon parameter: this milestone
represents only the exact self-selection callsite. A real weapon change remains
fail-closed/outside scope.

Temporary probe:

```text
ESP32/include/native_junction_post_load_weapon_select_probe.h
ESP32/src/native_junction_post_load_weapon_select_probe.c
```

Hardware must establish the current weapon and 8 B state FNV; neither is copied
from reset-time assumptions.

Acceptance requires:

```text
weaponBefore=requestedWeapon=weaponAfter
viewInvalidationRequested=0
selfSelect=yes
identityAssignment=yes
updateViewBranchTaken=no
legacy Player.weapon unchanged
legacy DoomCanvas.isUpdateView unchanged
post-load GIVEMAP FNV=448e587d unchanged
HUD-clear FNV=b7383e18 unchanged
PlayerView FNV=afcdcf74 unchanged
Facing FNV=95aa1108 unchanged
snapshotFNV=bb714d80 unchanged
mapFNV=8dba0bb4 unchanged
automapFNV=b699bd75 unchanged
all other resident owners unchanged
PAK closed
heap/largest delta=0
legacy Game/Player/Hud/DoomCanvas/Render/frame unchanged
legacy Player_selectWeapon not called
ST_PLAYING=no
entities=0
monsters=0
```

Fail-closed tests cover null/inactive preceding owner, wrong map/load context,
wrong GIVEMAP counts, invalid weapon, pure-prepare atomicity, post-active order
and repeat-route atomicity.

## Exact recovered caller order

```text
DoomCanvas_finishRotation()                  [hardware-proven complete]
Hud.msgCount=0                              [hardware-proven]
Hud.statBarMessage=NULL                     [hardware-proven]
Hud.logMessage[0]='\0'                     [hardware-proven]
if Junction: Game_givemap()                 [hardware-proven]
else: DoomCanvas_uncoverAutomap()
Player_selectWeapon(player, player->weapon) [CURRENT CANDIDATE]
if !game->isLoaded: Game_saveState(1,1,1)   [deferred]
Game.isLoaded=false                         [deferred]
Game.isSaved=false                          [deferred]
Game.activeLoadType=0                       [deferred]
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
finishRotationComplete=yes
Game_givemapPending=no
weaponReselectPending=yes
initialSavePending=yes
postLoadCleanupPending=yes
ST_PLAYING=no
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

Successful candidate PARK adds only:

```text
nativeWeaponSelfSelect=yes
weaponReselectPending=no
initialSavePending=yes
postLoadCleanupPending=yes
ST_PLAYING=no
```

## Still intentionally outside

```text
real weapon-change gameplay ownership
initial post-load save
post-load flag/event/particle cleanup
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

## Next action

Build/flash normal `esp32-cyd` from the current candidate branch and return the
complete `[JUNCTIONWEAPON]` Serial block. Promote only after real-CYD PASS.

## Next bounded milestone after PASS + merge

Recover exact new `main`, then audit only:

```c
if (!game->isLoaded) {
    Game_saveState(game, 1, 1, 1);
}
```

Do not bundle load cleanup, `ST_PLAYING`, gameplay entities or rendering.
