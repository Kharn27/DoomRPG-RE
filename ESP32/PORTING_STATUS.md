# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #77 — native post-load HUD clear
main = 56c4211a91e6a95763dd4cc215ef40de6c10a98b
hardware-tested firmware = 469abe119fbc401d812c21f96d94fd8aaae06ff3
status = REAL-CYD HARDWARE PASS
```

Merged evidence: [`MAP1_NATIVE_POST_LOAD_HUD_CLEAR.md`](MAP1_NATIVE_POST_LOAD_HUD_CLEAR.md).

## Current merge-ready milestone

```text
branch = agent/esp32-native-post-load-givemap
base   = 56c4211a91e6a95763dd4cc215ef40de6c10a98b
hardware-tested firmware = 511156120bd877367d13ffa4b98ed6815005bc3c
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Evidence: [`MAP1_NATIVE_POST_LOAD_GIVEMAP.md`](MAP1_NATIVE_POST_LOAD_GIVEMAP.md).

The same tested boot reached the complete downstream Junction chain. Because
MAP_INTRO SAVEGAME waits on `Esp32Map1GiveMapProbe_isDone()`, and the GIVEMAP
probe sets `done=1` only after its full successful audit/PARK, this progression
also proves the refactored historical opcode-9 path did not regress.

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

Junction source/residency:

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

Pre-GIVEMAP Junction resident owner FNVs:

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

Post-GIVEMAP Junction mutable owner FNVs:

```text
map      = 8dba0bb4
automap  = b699bd75
snapshot = bb714d80
```

Non-target owners remain unchanged:

```text
runtime  = bc432a0f
script   = bc9b18ff
line     = 3658710d
texture  = 537319ad
topology = d6e8df7d
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

Generic `EspMapOpcodeExecutor` remains intentionally only 11/19/20. Other real
opcode families keep their dedicated native semantic boundaries.

## Hardware-proven direct Junction GIVEMAP

Exact caller operation:

```c
Game_givemap(doomCanvas->game);
```

Exact legacy semantics represented natively:

```text
all lines without 0x20: set reveal 0x80
all map sprites: set reveal 0x10000000
all BIT_AM_ENTRANCE tiles: add BIT_AM_VISITED
```

Permanent shared direct API:

```text
EspMapGiveMapDirectResult = 12 B
EspMapAutomapState_planGiveMapDirect()
EspMapAutomapState_applyGiveMapDirect()
```

Historical event wrapper ABI remains:

```text
EspMapGiveMapResult = 20 B
EspMapAutomapState_applyGiveMapCommand()
```

Caller-order owner:

```text
EspPostLoadGiveMapState = 16 B
persistent heap = 0 B
stateFNV = 448e587d
```

Real-CYD target/mutation counts:

```text
lineTargets=198
spriteTargets=48
entranceTargets=15
linesMutated=198
spritesMutated=48
tilesMutated=15
```

Semantic proof:

```text
allTargetsRevealed=yes
idempotentPlan=yes
nonTargetOwnersUnchanged=yes
```

The second pure direct plan reports zero pending mutations.

Fail-closed proof:

```text
nullHud=1
nullOutput=1
inactiveHud=1
uncleared=1
targetMap=1
gameplayMap=1
loadType=1
plannerNull=1
prepareAtomic=yes
postActivePrepare=1
repeat=1
repeatAtomic=yes
```

RAM proof on normal `esp32-cyd`:

```text
heap8=72700->72700
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Same-build legacy/frame equality witnesses from the supplied complete boot tail:

```text
gameFNV=d073b2d5->d073b2d5
playerFNV=c64e7862->c64e7862
hudFNV=b18611d2->b18611d2
canvasFNV=18faeffd->18faeffd
renderFNV=f9344dec->f9344dec
frameFNV=c56f998b->c56f998b
legacyRuntimeClear=yes
legacyGame_givemapCalled=no
```

Stable post-PARK heartbeat:

```text
heap=138464
heap8=72700
largest8=34804
SD=ready
VIDEO=ready
CORE=ready
```

## Historical opcode-9 regression proof

The direct GIVEMAP refactor also backs `EV_GIVEMAP` on MAP_INTRO. The same tested
boot proves that old path completed successfully by strict probe gating:

```text
Esp32Map1GiveMapProbe_service()
  attempted=1 before audit
  any FAILED path returns with done=0
  done=1 only after successful PARK

Esp32Map1SaveRouteProbe_service()
  returns until Esp32Map1GiveMapProbe_isDone()

Esp32Map1ChangeMapProbe_service()
  returns until Esp32Map1SaveRouteProbe_isDone()
```

The observed downstream Junction probes therefore cannot occur in a boot where
the historical GIVEMAP audit failed. This is a control-flow regression proof from
the exact tested firmware, not a fabricated Serial line.

## Exact recovered caller order

```text
DoomCanvas_finishRotation()                  [hardware-proven complete]
Hud.msgCount=0                              [hardware-proven]
Hud.statBarMessage=NULL                     [hardware-proven]
Hud.logMessage[0]='\0'                     [hardware-proven]
if Junction: Game_givemap()                 [hardware-proven]
else: DoomCanvas_uncoverAutomap()
Player_selectWeapon(current weapon)         [NEXT after merge]
initial Game_saveState when !isLoaded       [deferred]
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

## Still intentionally outside

```text
actual HUD/gameplay rendering
weapon restore/select ownership
initial post-load save
post-load flag/event/particle cleanup
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

## Merge recommendation

```text
MERGE agent/esp32-native-post-load-givemap
```

Hardware-tested firmware:

```text
511156120bd877367d13ffa4b98ed6815005bc3c
```

All commits after that tested SHA must remain documentation-only.

## Next bounded milestone after merge

Recover the exact new `main` SHA, then audit and port only:

```c
Player_selectWeapon(player, player->weapon);
```

Do not bundle initial save, load cleanup, `ST_PLAYING`, gameplay entities or
rendering into that milestone.