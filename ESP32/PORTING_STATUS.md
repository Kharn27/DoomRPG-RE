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

## Current merge-ready milestone

```text
branch = agent/esp32-native-post-load-weapon-self-select
base   = 4737b016d02615b8435cf84909fe3c251b6d338b
hardware-tested firmware = 24fb8fbf914820500d2e16815e22beb0439c9ba0
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Evidence: [`MAP1_NATIVE_POST_LOAD_WEAPON_SELF_SELECT.md`](MAP1_NATIVE_POST_LOAD_WEAPON_SELF_SELECT.md).

This milestone owns only:

```c
Player_selectWeapon(player, player->weapon);
```

Recovered legacy behavior:

```c
if (player->weapon != i) {
    DoomCanvas_updateViewTrue(player->doomRpg->doomCanvas);
}
player->weapon = i;
```

At this caller `i == player->weapon`, so the view-update branch is not taken and
the assignment is identity. The real CYD hardware-proved current weapon `2`.

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

Post-GIVEMAP/current Junction resident owners:

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

## Hardware-proven current-weapon self-selection

Permanent files:

```text
ESP32/include/esp_post_load_weapon_select_state.h
ESP32/src/esp_post_load_weapon_select_state.c
```

Owner:

```text
EspPostLoadWeaponSelectState = 8 B
persistent heap = 0 B
stateFNV = 699f3cf3
```

Real-CYD semantic state:

```text
weaponBefore=2
requestedWeapon=2
weaponAfter=2
viewInvalidationRequested=0
targetMap=9
gameplayLoadMapId=2
loadType=0
active=1
selfSelect=yes
identityAssignment=yes
updateViewBranchTaken=no
legacyWeapon=2->2
legacyIsUpdateView=1->1
```

Input-owner proof:

```text
giveMapFNV=448e587d
hudClearFNV=b7383e18
viewFNV=afcdcf74
facingFNV=95aa1108
unchanged=yes
callerOrder=yes
```

Fail-closed proof:

```text
nullGiveMap=1
nullOutput=1
inactiveGiveMap=1
targetMap=1
gameplayMap=1
loadType=1
count=1
invalidWeapon=1
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
heap8=72684->72684
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Same-build legacy/frame equality witnesses:

```text
gameFNV=d073b2d5->d073b2d5
playerFNV=c64e7862->c64e7862
hudFNV=b18611d2->b18611d2
canvasFNV=d6d1b92a->d6d1b92a
renderFNV=f9344dec->f9344dec
frameFNV=ee9d9dbc->ee9d9dbc
legacyRuntimeClear=yes
legacyPlayer_selectWeaponCalled=no
```

Stable post-PARK heartbeat:

```text
heap=138448
heap8=72684
largest8=34804
SD=ready
VIDEO=ready
CORE=ready
```

## Probe completion semantics recovery note

Repository audit confirms temporary probes currently use `probeState.done` as a
terminal-attempt marker; some failure paths also set `done=1`. Therefore a
`*_isDone()` gate alone is **not** a PASS certificate and must not be used as a
standalone transitive regression proof.

Downstream stages must revalidate the exact preceding owner/world boundary. The
weapon self-select hardware PASS is valid because the complete successful
`[JUNCTIONWEAPON]` block was observed and the probe independently validated all
required predecessor fingerprints and invariants before PARK.

The previous documentation claim that historical opcode-9 GIVEMAP was proven
solely because later probes were reached is retired. Any future historical-path
regression claim must be backed by direct output or by a success-only signal.

## Exact recovered caller order

```text
DoomCanvas_finishRotation()                  [hardware-proven complete]
Hud.msgCount=0                              [hardware-proven]
Hud.statBarMessage=NULL                     [hardware-proven]
Hud.logMessage[0]='\0'                     [hardware-proven]
if Junction: Game_givemap()                 [hardware-proven]
else: DoomCanvas_uncoverAutomap()
Player_selectWeapon(player, player->weapon) [hardware-proven]
if !game->isLoaded: Game_saveState(1,1,1)   [NEXT after merge]
Game.isLoaded=false                         [deferred]
Game.isSaved=false                          [deferred]
Game.activeLoadType=0                       [deferred]
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
finishRotationComplete=yes
Game_givemapPending=no
weaponReselectPending=no
initialSavePending=yes
postLoadCleanupPending=yes
ST_PLAYING=no
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
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

## Merge recommendation

```text
MERGE agent/esp32-native-post-load-weapon-self-select
```

Hardware-tested firmware:

```text
24fb8fbf914820500d2e16815e22beb0439c9ba0
```

All commits after that tested SHA must remain documentation-only.

## Next bounded milestone after merge

Recover exact new `main`, then audit only:

```c
if (!game->isLoaded) {
    Game_saveState(game, 1, 1, 1);
}
```

Do not bundle load cleanup, `ST_PLAYING`, gameplay entities or rendering.
