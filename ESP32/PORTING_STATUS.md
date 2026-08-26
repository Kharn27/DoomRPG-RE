# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port. Current GitHub `main` + this file + `DOCUMENTATION.md` + the latest relevant milestone archive override chat memory.

## Latest merged hardware baseline

```text
PR   = #98 — closed line-derived gameplay collision
main = 3b17a400c35338e434fab16ae0c2a3a63ab47e3e
status = MERGED
```

Merged evidence: [`MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md).

## Current candidate milestone

```text
branch = agent/esp32-native-gameplay-dynamic-line-collision
base   = 3b17a400c35338e434fab16ae0c2a3a63ab47e3e
hardware-tested implementation SHA = 52ddbf979e33f99be27c8344eb4e0572ac4d0547
status = REAL-CYD HARDWARE PASS
merge-ready = yes after docs-only closeout audit
```

Evidence: [`MAP1_NATIVE_GAMEPLAY_DYNAMIC_LINE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_DYNAMIC_LINE_COLLISION.md).

## Current hardware truth

Canonical fresh Junction state remains:

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

## Dynamic line collision — hardware proven

The closed-line milestone previously failed closed whenever `EspMapLineState.openCount != 0`.

The permanent collision path now consumes the compact line overlay per line:

```text
closed line -> collision candidate
open line   -> skipped
close again -> collision candidate again
```

This reproduces the legacy entity topology without constructing `Entity_t` or `entityDb`. Wall ordering, line geometry/link nudges, reverse line scan, trace mask `0xf287`, sprite-entity collision and the 16 B collision result ABI remain unchanged.

Activation uses line 35 only as a reversible collision-consumer witness:

```text
CLOSED -> BLOCKED_ENTITY
OPEN   -> CLEAR
CLOSE  -> byte-identical BLOCKED_ENTITY result
```

The first implementation SHA `429a86d2d84cbbedc807046491d06db1e37b9474` contained a contradictory probe guard: it intentionally changed line-state FNV while simultaneously requiring the entire resident snapshot, which contains that FNV, to remain byte-identical. Real hardware caught this and correctly blocked native gameplay activation.

Fix SHA `52ddbf979e33f99be27c8344eb4e0572ac4d0547` changes only the witness rule: while OPEN the resident snapshot may differ only in `lineStateFNV1a`; after CLOSE the complete snapshot must match exactly again.

Post-fix real-CYD evidence shows normal MOVE and TURN execution continuing after activation, which can only happen after the dynamic-line witness succeeds:

```text
[MOVE] OK ... tile=591->559 ...
[MOVE] OK ... tile=591->623 ...
[MOVE] OK ... tile=623->655 ...
[MOVE] OK ... tile=655->687 ...
[MOVE] OK ... tile=746->778 ...
[TURN] OK ...
```

Representative integrity witnesses from the supplied run:

```text
heap8=38928 stable
largest8=29684 stable
stackHighWater=860
legacyStable=yes
residentStable=yes
orientationStable=yes on MOVE
turnAdvance=no
tileDispatch=no
```

The supplied post-fix excerpt starts after activation and does not contain the exact OPEN-state FNV. Do not invent it.

## Important SELECT / door boundary

There is **no player-operated door yet**.

Current SELECT remains input-only:

```text
action=SELECT
consumedBy=probe
dispatch=observer
gameplay=no
```

The dynamic-line witness opens/closes line 35 during activation and restores it before gameplay. It is not a user-visible door action.

Recovered legacy `Game_performDoorEvent()` semantics begin with:

```text
if line.flags & 0x400 (LOCKED)
    return false
```

Therefore the next milestone must not make colored/locked Junction doors open by directly toggling the native bit. Lock/key/event semantics must be recovered before a real door is declared playable.

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

Persistent render/storage baseline remains inherited from merged PR #97/#98:

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
```

Still intentionally absent:

```text
SELECT gameplay dispatch
front-tile interaction semantics
lock/key gameplay ownership
post-move tile event execution
actual Game_advanceTurn semantics
EV_OPENLINE/CLOSELINE triggered by real gameplay
visual door animation consumer
weapon switching execution
PASS_TURN execution
MENU/AUTOMAP gameplay execution
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
player-operated door=no
SELECT gameplay=no
Game_advanceTurn=no
Game_executeTile=no
facingRefresh=deferred
```

## Recommended next bounded milestone

Recover **native SELECT front-tile routing and lock-aware interaction intent** before enabling a visible door open.

The preferred boundary is:

```text
SELECT at current cardinal facing
 -> identify canonical front tile / front line / tile event
 -> recover legacy 1280 interaction flag semantics
 -> expose compact native interaction result/intent
 -> preserve LOCKED fail-closed behavior
 -> no fake key ownership
 -> no broad legacy Game_executeEvent
```

Only after that boundary is hardware-proven should a dedicated milestone trigger native `EV_OPENLINE/EV_CLOSELINE` and then a later visual door-animation consumer.

## Merge recommendation

```text
REAL-CYD HARDWARE PASS
hardware-tested implementation SHA = 52ddbf979e33f99be27c8344eb4e0572ac4d0547
MERGE-READY = YES after docs-only closeout audit
```

All commits after `52ddbf9...` must remain documentation-only for this hardware certificate to stay valid. After merge, recover the exact new `main` SHA and reread this file, `DOCUMENTATION.md`, and [`MAP1_NATIVE_GAMEPLAY_DYNAMIC_LINE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_DYNAMIC_LINE_COLLISION.md) before creating the next `agent/*` branch.
