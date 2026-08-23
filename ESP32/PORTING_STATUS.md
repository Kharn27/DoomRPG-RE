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

The native caller chain is hardware-proven through the three HUD message-channel
resets immediately after `DoomCanvas_finishRotation()`.

## Current hardware candidate

```text
branch = agent/esp32-native-post-load-givemap
base   = 56c4211a91e6a95763dd4cc215ef40de6c10a98b
status = HARDWARE CANDIDATE — NOT YET CYD-PROVEN
```

Candidate: [`MAP1_NATIVE_POST_LOAD_GIVEMAP.md`](MAP1_NATIVE_POST_LOAD_GIVEMAP.md).

This milestone owns only the next exact Junction caller operation:

```c
Game_givemap(doomCanvas->game);
```

Weapon reselection, initial save, load/event/particle cleanup, view dirtying and
`ST_PLAYING` remain explicitly deferred.

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

Junction resident owner FNVs before direct caller GIVEMAP:

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

Generic `EspMapOpcodeExecutor` remains intentionally only 11/19/20. Other real
opcode families keep their dedicated native semantic boundaries.

## Last hardware-proven caller boundary

`EspHudPostLoadClearState`:

```text
bytes=8
stateFNV=b7383e18
messageCount=0
statBarMessagePresent=0
logMessageLength=0
cleared=1
active=1
targetMap=9
gameplayLoadMapId=2
loadType=0
persistent heap=0 B
```

Inputs remain:

```text
PlayerView FNV=afcdcf74
Facing FNV=95aa1108
finishRotationComplete=yes
```

Latest normal-env RAM/integrity proof:

```text
snapshotFNV=bc9071e9->bc9071e9
automapFNV=0b2ae445->0b2ae445
heap8=72732->72732
largest8=34804->34804
persistentHeapBytes=0
packClosed=yes
```

Same-build witnesses from that tested firmware:

```text
gameFNV=d073b2d5->d073b2d5
playerFNV=c64e7862->c64e7862
hudFNV=b18611d2->b18611d2
canvasFNV=70a8ad15->70a8ad15
renderFNV=f9344dec->f9344dec
frameFNV=9eb7ce0f->9eb7ce0f
```

These equality hashes are same-build witnesses, not cross-build canons.

## Exact recovered caller order

```text
DoomCanvas_finishRotation()                  [hardware-proven complete]
Hud.msgCount=0                              [hardware-proven]
Hud.statBarMessage=NULL                     [hardware-proven]
Hud.logMessage[0]='\0'                     [hardware-proven]
if Junction: Game_givemap()                 [CURRENT CANDIDATE]
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

Exact direct `Game_givemap()` semantics:

```text
all lines without 0x20: set reveal 0x80
all map sprites: set reveal 0x10000000
all BIT_AM_ENTRANCE tiles: add BIT_AM_VISITED
```

## Current direct-GIVEMAP implementation boundary

Existing permanent owners are reused:

```text
EspMapAutomapState -> line/sprite reveal bits
EspMapState        -> BIT_AM_VISITED
```

The hardware-proven event wrapper remains ABI-compatible:

```text
EspMapGiveMapResult = 20 B
EspMapAutomapState_applyGiveMapCommand()
```

New shared event-independent primitive:

```text
EspMapGiveMapDirectResult = 12 B candidate
EspMapAutomapState_planGiveMapDirect()
EspMapAutomapState_applyGiveMapDirect()
```

The old opcode-9 path keeps descriptor/opcode/removal validation, then delegates
only the world mutation to the same direct primitive. The normal firmware still
runs the historical `[MAPGIVEMAPPROBE]`, so this refactor receives same-build
hardware regression coverage before the Junction transition.

New caller-order owner:

```text
ESP32/include/esp_post_load_givemap_state.h
ESP32/src/esp_post_load_givemap_state.c
EspPostLoadGiveMapState = 16 B candidate
persistent heap = 0 B
```

Pure preparation requires the exact HUD-clear owner plus exact untouched
Junction runtime/world identity:

```text
targetMap=9
gameplayLoadMapId=2
loadType=0
sourceBytes=21051
sourceCrc32=4a2c5800
runtimeFNV=bc432a0f
mapStateFNV=c5cdfc04
automapFNV=0b2ae445
```

Route applies direct GIVEMAP exactly once and parks the target/mutation counts.
No legacy `Game_givemap()` call occurs.

## Candidate hardware acceptance

New probe:

```text
ESP32/include/native_junction_post_load_givemap_probe.h
ESP32/src/native_junction_post_load_givemap_probe.c
```

The real CYD must establish the unknown Junction values:

```text
EspPostLoadGiveMapState FNV
line/sprite/entrance target counts
line/sprite/tile mutation counts
post-GIVEMAP mapStateFNV
post-GIVEMAP automapFNV
post-GIVEMAP resident snapshotFNV
same-build legacy/frame/RAM witnesses
```

Required semantics:

```text
pure prepare atomic
all eligible lines revealed
all map sprites revealed
all entrance tiles visited
second direct plan has zero mutations
runtime/script/line/texture/topology unchanged
map and automap FNVs changed from initial canon
payload/entity counts unchanged
HUD-clear b7383e18 unchanged
PlayerView afcdcf74 unchanged
Facing 95aa1108 unchanged
PAK closed
heap/largest delta=0
legacy Game/Player/Hud/DoomCanvas/Render/frame unchanged
ST_PLAYING=no
entities=0
monsters=0
```

## Current hardware PARK before candidate

```text
state=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativeFacing=yes
nativeHudClear=yes
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

Successful candidate PARK should change only the native map/automap state and add:

```text
nativePostLoadGiveMap=yes
Game_givemapPending=no
weaponReselectPending=yes
initialSavePending=yes
postLoadCleanupPending=yes
ST_PLAYING=no
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

## Next action

Build/flash normal `esp32-cyd` and return the complete `[JUNCTIONGIVEMAP]` block.
Also preserve the earlier `[MAPGIVEMAPPROBE]` PASS in the same firmware.

Do not mark this branch merge-ready until real CYD proves the direct Junction
world mutation, idempotence, invariants and RAM/legacy integrity.
