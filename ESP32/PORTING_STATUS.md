# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port. Current GitHub `main` + this file + `DOCUMENTATION.md` + the latest relevant milestone archive override chat memory.

## Latest merged hardware baseline

```text
PR   = #99 — dynamic per-line gameplay collision
main = e0a250f0bfd6e5519298f942f4bed65c230c3652
status = MERGED
```

Merged evidence: [`MAP1_NATIVE_GAMEPLAY_DYNAMIC_LINE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_DYNAMIC_LINE_COLLISION.md).

## Current candidate milestone

```text
branch = agent/esp32-native-gameplay-select-front-tile
base   = e0a250f0bfd6e5519298f942f4bed65c230c3652
hardware-tested implementation SHA = ca5560c0eb849c8a11b21eb8c117e7a8fc4c60ff
status = REAL-CYD HARDWARE PASS
merge-ready = yes after docs-only closeout audit
```

Evidence: [`MAP1_NATIVE_GAMEPLAY_SELECT_FRONT_TILE.md`](MAP1_NATIVE_GAMEPLAY_SELECT_FRONT_TILE.md).

## Current hardware truth

Canonical fresh Junction state:

```text
resourceMapId=9 / /junction.bsp
gameplayLoadMapId=2
player=992,1888,36
angle=64 / North
tile=943
frame=ba3e5182
viewport=9206eb24
HUD bands=6c2aa46f
HUD stateFNV=4756db9c
```

Arrival-door line witness:

```text
line=35
texture=7
raw=960,1952 -> 1024,1952
flags=00000505
LOCKED bit 0x400=yes
open bit 0x40=no at baseline
entity-def lookup=305+7=312
entity type=0
linked tile=975
baseline lineStateFNV=3658710d
```

Correct fresh-neighbor collision canon:

```text
FORWARD      943->911 : CLEAR
BACK         943->975 : ENTITY / closed line 35 / type 0
STRAFE_LEFT  943->942 : WALL
STRAFE_RIGHT 943->944 : WALL
```

## Dynamic line collision — merged hardware truth

Native collision consumes `EspMapLineState` per line:

```text
closed line -> collision candidate
open line   -> skipped
close again -> collision candidate again
```

Canonical activation witness:

```text
line35 CLOSED -> BLOCKED_ENTITY
line35 OPEN   -> CLEAR
line35 CLOSE  -> exact original BLOCKED_ENTITY result
```

No `Entity_t` world is created. Legacy line link/unlink collision semantics are reproduced directly from the compact native overlay.

## SELECT front-tile routing — hardware proven

The first live gameplay SELECT boundary is now physically proven on the real CYD.

Recovered first-step legacy route:

```text
SELECT
 -> settled native dest + viewStep
 -> front tile
 -> Game_executeTile-equivalent flags 1280 / 0x500
 -> native event lookup/descriptor/current-state/filter provenance
 -> optional front-tile line-derived entity witness
 -> read-only result
```

Permanent ABI:

```text
EspNativeGameplaySelectResult = 28 B
EspMapLineTopologyRef         = 16 B
persistent heap               = 0 B
```

The observer runs before transient touch feedback and requires exact frame/heap/legacy/resident integrity. It performs no bytecode execution, door mutation, broad entity trace, render, sound or turn advance.

### Fresh North SELECT

Real CYD:

```text
front=992,1824
tile=911
event=59
commands=1
opcode=4 / UNOWNED
decision=FLAGS_MISMATCH
frame=ba3e5182 exact=yes
heap8=38924->38924
largest8=29684->29684
legacyExact=yes
residentExact=yes
packClosed=yes
```

### Arrival door SELECT — decisive new truth

After two right turns from fresh North:

```text
player=992,1888
angle=192 / South
frame=da1c4297
```

Physical SELECT produced:

```text
front=992,1952
tile=975
event=63
commands=1
```

The same tile resolves line 35:

```text
line=35
texture=7
flags=00000505
type=0
defTile=312
open=0
locked=1
linked=1
```

The event's one eligible command is:

```text
opcode=15 / EV_OPENLINE
arg1=00000023  # line 35
arg2=00000100
decision=ELIGIBLE
```

Therefore the real gameplay chain is now known exactly:

```text
SELECT on arrival door
 -> Junction tile 975
 -> event 63
 -> EV_OPENLINE(35)
 -> line 35 is LOCKED
```

This does **not** mean the door may open. Legacy door mutation independently refuses a locked line. The current milestone intentionally observes the command but performs no mutation.

### Recovery correction

Do not use `/intro.bsp` event-table bounds as Junction facts. An earlier pre-test prediction inferred tile 975 had no event from the MAP_INTRO corpus ending at tile 968. Real hardware disproved that inference because the active resident runtime is `/junction.bsp`:

```text
Junction tile975 -> event63 -> EV_OPENLINE(35)
```

The CYD log is authoritative.

### Live dialogue/interaction corpus

The user navigated around Junction and SELECTed visible scientist/computer/soldier targets. Real multi-command front-tile scripts were resolved without execution:

```text
tile878 event56 commands=18 -> EV_DIALOG / EV_CHANGESTATE / EV_NEXTSTATE
tile845 event49 commands=14 -> EV_DIALOG / EV_CHANGESTATE, plus line98 type7 unlocked
tile816 event45 commands=12 -> EV_DIALOG / EV_CHANGESTATE
```

Current zero-key/state context produced `KEY_MISMATCH` and `STATE_MISMATCH` decisions as expected. No dialogue/UI/world mutation occurred.

## Gameplay regression proof

The same hardware session continued to execute MOVE/TURN around multiple SELECT observations:

```text
canonical North frame round-trip=ba3e5182 exact
MOVE 943->911 OK
MOVE 911->879 OK
MOVE 847->846 OK
MOVE 846->847 OK
MOVE 847->815 OK
heap8=38924 stable
largest8=29684 stable
stackHighWater=860
legacyStable=yes
residentStable=yes
orientationStable=yes on MOVE
turnAdvance=no
tileDispatch=no
```

No `SELECTPROBE FAILED`, Guru Meditation or reboot was reported.

## Permanent memory / architecture invariants

```text
board       = ESP32-2432S028R classic CYD
MCU         = ESP32-D0WD-V3 dual core 240 MHz
flash       = 4 MB
PSRAM       = none
framebuffer = 160x120 RGB565 = 38400 B
shapeData   = NULL
mediaTexels = NULL
runtime ZIP = forbidden for migrated map/graphics paths
backing     = /DoomRPG-ESP32.pak
legacy Game.entities = 0
legacy Game.monsters = 0
native ST_PLAYING = reached
legacy ST_PLAYING = not reached
native PLAYING service = reached
```

Stable Junction resident canons:

```text
sourceBytes=21051
crc32=4a2c5800
sourceFNV=fefaf5ca
payload=10410 B
runtimeFNV=bc432a0f
mapFNV=8dba0bb4
scriptFNV=bc9b18ff
lineFNV=3658710d
textureStateFNV=537319ad
automapFNV=b699bd75
topologyFNV=d6e8df7d
snapshotFNV=bb714d80
```

Persistent render/storage baseline remains inherited:

```text
small exact cache <=1024 B=yes
shared exact 2048 B tail cache=yes
large retained slots=3
legacy cross-block wall guard=16 B BSS
shapeData=NULL
mediaTexels=NULL
```

## Hardware-proven native chain

```text
native map transition/residency
 -> spawn/player/view ownership
 -> native ST_PLAYING + PLAYING service
 -> sparse graphics catalog
 -> walls + textured planes
 -> BSP-visible billboards + glows
 -> native HUD
 -> calibrated touch intent
 -> TURN_LEFT/TURN_RIGHT
 -> FORWARD/BACK/STRAFE
 -> static WALL collision
 -> compact sprite-entity collision
 -> closed line-derived collision
 -> dynamic per-line open/close collision topology
 -> viewport-only gameplay recomposition
 -> persistent bounded render-resource owner
 -> SELECT dest+viewStep front-tile routing
 -> event/current-state/filter provenance
 -> front-tile line-derived entity witness
```

Still intentionally absent:

```text
actual SELECT bytecode execution
actual EV_OPENLINE/CLOSELINE gameplay mutation
locked-door refusal result owner
unlock/key pickup gameplay ownership
post-MOVE tile event execution
actual Game_advanceTurn semantics
visual door animation consumer
weapon switching execution
PASS_TURN execution
MENU/AUTOMAP gameplay execution
broad entity/combat SELECT fallback
live dialogue rendering from SELECT
entity/monster activation and AI
facing refresh after gameplay actions
first-person weapon overlay
native durable save storage
sound playback
```

Generic `EspMapOpcodeExecutor` remains limited to opcodes `11/19/20` and fail-closes unsupported opcodes.

## Current hardware PARK

```text
legacyState=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativeST_PLAYING=yes
nativePlayingService=yes
nativeGraphicsCatalog=yes
nativeFirstFrame=yes
texturedPlanes=yes
nativeBaseBillboards=yes
glowCompanions=yes
nativeHud=yes
nativeInput=yes
nativeTurnDispatch=yes
nativeMovementDispatch=yes
nativeGameplayViewportHotPath=yes
static wall collision=yes
compact linked sprite-entity collision=yes
closed line-entity collision=yes
dynamic opened-line collision=yes / per-line open skip
SELECT front-tile observer=yes
SELECT runFlags=0x500
front event provenance=yes
front line provenance=yes
arrivalDoor event63/EV_OPENLINE35=yes
player-operated door=no
Game_advanceTurn=no
postMoveTileDispatch=no
facingRefresh=deferred
shapeData=NULL
mediaTexels=NULL
```

## Recommended next bounded milestone

The next boundary is now concrete:

```text
real SELECT event command
 -> support only EV_OPENLINE / EV_CLOSELINE family
 -> resolve target EspMapLineState
 -> legacy LOCKED guard first
 -> locked => refuse with zero mutation
 -> unlocked => bounded native open/close mutation
 -> existing collision consumer immediately follows line open state
 -> fail closed for every unrelated opcode
```

Do not add sound, animation, broad entity trace, unrelated script families or fake key ownership in the same milestone.

A successful open/close mutation witness should use a real unlocked Junction line/event discovered from the corpus. Do not bypass line 35's lock bit just to make the arrival door move.

## Merge recommendation

```text
REAL-CYD HARDWARE PASS
hardware-tested implementation SHA = ca5560c0eb849c8a11b21eb8c117e7a8fc4c60ff
MERGE-READY = YES after docs-only closeout audit
```

Every commit after `ca5560c...` must remain documentation-only for this hardware certificate to stay valid. After merge, recover the exact new `main` SHA and reread this file, `DOCUMENTATION.md`, and [`MAP1_NATIVE_GAMEPLAY_SELECT_FRONT_TILE.md`](MAP1_NATIVE_GAMEPLAY_SELECT_FRONT_TILE.md) before creating the next `agent/*` branch.
