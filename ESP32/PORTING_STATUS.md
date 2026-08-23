# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #76 — native durable facing
main = 3ab143110a1f44ebb44bc130d12d1844f3ae73ca
hardware-tested firmware = 660c797e2168260a861c185fae9e812769b46156
status = REAL-CYD HARDWARE PASS
```

Merged evidence: [`MAP1_NATIVE_DURABLE_FACING.md`](MAP1_NATIVE_DURABLE_FACING.md).

`DoomCanvas_finishRotation()` is semantically complete natively through its
final durable facing query.

## Current merge-ready milestone

```text
branch = agent/esp32-native-post-load-hud-clear
base   = 3ab143110a1f44ebb44bc130d12d1844f3ae73ca
hardware-tested firmware = 469abe119fbc401d812c21f96d94fd8aaae06ff3
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Evidence: [`MAP1_NATIVE_POST_LOAD_HUD_CLEAR.md`](MAP1_NATIVE_POST_LOAD_HUD_CLEAR.md).

This milestone hardware-proves only the first three caller writes immediately
after `DoomCanvas_finishRotation()`:

```text
Hud.msgCount=0
Hud.statBarMessage=NULL
Hud.logMessage[0]='\0'
```

The native owner represents those semantics without mutating the legacy `Hud_t`.
Junction `Game_givemap()`, weapon reselection, initial save, post-load cleanup,
view dirtying and `ST_PLAYING` remain explicitly deferred.

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
snapshotFNV=bc9071e9
payload=10410 B
actual heap=10540 B
entities=30
enemies=0
destructibles=3
```

Junction resident owner FNVs:

```text
runtime  = bc432a0f
map      = c5cdfc04
script   = bc9b18ff
line     = 3658710d
texture  = 537319ad
automap  = 0b2ae445
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
```

Generic `EspMapOpcodeExecutor` remains intentionally only 11/19/20. All real
MAP_INTRO opcode families have dedicated native boundaries.

## Hardware-proven post-load HUD clear boundary

Permanent owner:

```text
ESP32/include/esp_hud_post_load_clear_state.h
ESP32/src/esp_hud_post_load_clear_state.c
EspHudPostLoadClearState = 8 B
persistent heap = 0 B
```

Real-CYD state:

```text
stateFNV=b7383e18
messageCount=0
statBarMessagePresent=0
logMessageLength=0
cleared=1
active=1
targetMap=9
gameplayLoadMapId=2
loadType=0
```

Hardware-proven inputs remain unchanged:

```text
PlayerView FNV=afcdcf74
Facing FNV=95aa1108
finishRotationComplete=yes
```

Semantic legacy-field witness:

```text
legacyMsgCount=0->0
legacyStatBarPresent=0->0
legacyLogFirst=0->0
legacyHudUntouched=yes
```

Fail-closed proof:

```text
nullView=1
nullFacing=1
nullOutput=1
inactiveView=1
inactiveFacing=1
facingMismatch=1
loadType=1
order=1
prepareAtomic=yes
repeat=1
repeatAtomic=yes
```

Resident/automap proof:

```text
snapshotFNV=bc9071e9->bc9071e9
unchanged=yes
automapFNV=0b2ae445->0b2ae445
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes
Game_givemapDeferred=yes
```

Latest normal-env RAM proof:

```text
heap8=72732->72732
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Same-build equality witnesses:

```text
gameFNV=d073b2d5->d073b2d5
playerFNV=c64e7862->c64e7862
hudFNV=b18611d2->b18611d2
canvasFNV=70a8ad15->70a8ad15
renderFNV=f9344dec->f9344dec
frameFNV=9eb7ce0f->9eb7ce0f
legacyRuntimeClear=yes
GameMutation=no
PlayerMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
```

These equality FNVs are same-build witnesses, not cross-build canons.

Stable heartbeat after the milestone:

```text
heap=138496
heap8=72732
largest8=34804
SD=ready
VIDEO=ready
CORE=ready
```

## Exact recovered caller order after finishRotation

From current `src/DoomCanvas.c`:

```text
DoomCanvas_finishRotation()                  [hardware-proven complete]
Hud.msgCount=0                              [hardware-proven]
Hud.statBarMessage=NULL                     [hardware-proven]
Hud.logMessage[0]='\0'                     [hardware-proven]
if Junction: Game_givemap()                 [next]
else: DoomCanvas_uncoverAutomap()
Player_selectWeapon(current weapon)         [deferred]
initial Game_saveState when !isLoaded       [deferred]
Game.isLoaded=false                         [deferred]
Game.isSaved=false                          [deferred]
Game.activeLoadType=0                       [deferred]
numEvents=0 / particles cleared             [deferred]
isUpdateView=true                           [deferred]
DoomCanvas_setState(ST_PLAYING)             [deferred]
idleTime=time+8000                          [deferred]
```

Recovered direct `Game_givemap()` semantics:

```text
all non-hidden lines: flags |= 0x80
all map sprites: info |= 0x10000000
all BIT_AM_ENTRANCE tiles: add BIT_AM_VISITED
```

Those semantics already map to compact native automap/map-state owners. Direct
caller-side `Game_givemap()` remains the next separate milestone.

## Current hardware PARK

```text
state=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativePlayerView=yes
nativeInitialTile=yes
nativeOrientation=yes
nativeSecondTile=yes
nativeFacing=yes
nativeHudClear=yes
facingPending=no
finishRotationComplete=yes
Game_givemapPending=yes
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
actual HUD rendering / renderer dirty consumption
direct caller-side Junction Game_givemap
weapon restore/select ownership when disabledWeapons!=0
initial post-load save
post-load flag/particle cleanup
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

`shapeData == NULL` and `mediaTexels == NULL` remain mandatory.

## Next bounded milestone after merge

After merge, recover the exact new `main` SHA and implement only direct
caller-side Junction `Game_givemap()` against the native automap/map-state
owners. Do not bundle weapon selection, save, cleanup, `ST_PLAYING`, entity
reconstruction or rendering into that milestone.

## Merge recommendation

```text
MERGE agent/esp32-native-post-load-hud-clear
```

Hardware-tested firmware:

```text
469abe119fbc401d812c21f96d94fd8aaae06ff3
```

Every later commit on this branch must remain documentation-only unless another
firmware is flashed.
