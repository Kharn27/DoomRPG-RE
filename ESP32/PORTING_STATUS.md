# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime authority.

## Git boundary — LOCKED milestone

```text
main = 6e07187f60a27e197189a47f2cbc7ff4e338cfec
branch = agent/esp32-native-full-gameplay
base main = 6e07187f60a27e197189a47f2cbc7ff4e338cfec
hardware-tested code boundary = feae39c768105b8851a77dab1afa4b52bec231dd
first docs-only PASS record = 7bb06628a8a33d7fae2e4bdb29f7f1916f0cd6a9
status = REAL-CYD JAMMED-DOOR DESTRUCTION COMPLETE
branch policy = LOCKED; docs-only tail only
```

`feae39c...` is the code actually exercised on the real CYD. Commits after that
boundary must remain documentation-only until this branch is merged.

After merge, read the real GitHub `main` SHA again before creating the next
`agent/*` branch. Do not continue gameplay code from this locked branch.

## Permanent architecture and hard invariants

```text
A NEW BSP IS NOT A NEW ENGINE.
```

Production path:

```text
/DoomRPG-ESP32.pak
 -> native parsers/catalog
 -> compact immutable EspMapRuntime
 -> small explicit mutable owners
 -> native event/action/gameplay
 -> native renderer
```

Hard invariants:

```text
board       = ESP32-2432S028R classic CYD
MCU         = ESP32-D0WD-V3 dual core 240 MHz
flash       = 4 MB
PSRAM       = none
framebuffer = 160x120 RGB565 = 38400 B
shapeData   = NULL
mediaTexels = NULL
backing     = /DoomRPG-ESP32.pak
```

Do not reintroduce map-wide legacy texels, desktop pointer-heavy world ownership,
or runtime ZIP dependence for migrated map data.

## Entrance canonical witness

```text
resourceMapId = 1
resource = /intro.bsp
name = Entrance
sourceBytes = 21823
crc32 = 623f34e4
sourceFNV = d5cc751f
runtime arena = 14095 B
runtimeFNV = c3882516
resident payload = 17891 B
spawn tile = 904
spawn direction = 64
spawn position = 544,1824
nodes = 223
lines = 480
sprites = 344
events = 93
byteCodes = 265
strings = 94
native topology entities = 220
enemies = 30
destructibles = 13
```

Owner fingerprints still used as regression witnesses:

```text
mapStateFNV  = cd99b98e
scriptFNV    = f9e3d9df
lineFNV      = e5e74861
textureFNV   = f1fc1875
automapFNV   = 669b1aa7
topologyFNV  = 3f321e43
```

Generic initial-session witness:

```text
targetMapId = 1
gameplayLoadMapId = 1
angle = 64
graphics textures = 33
graphics sprites = 45 -> 46 after dependency closure
catalog storage = 3120 B
catalog FNV = 29ffc14a
initial world frame = 71ca7465
HUD hp = 30/30
HUD armor = 0/20
HUD weapon = 2
HUD ammo = 8
```

## Selected resident asset-cache baseline

Current hardware-selected cache remains unchanged:

```text
owner = 23592 B
payload = 19456 B (19 KiB)
range records = 288
range record = 12 B
resident entry slots = 24
large exact range = 2048 B
```

`ResidentRangeRecord` stores 32-bit `nameHash` / `relativeOffset` plus bounded
16-bit `length` / `dataOffset`. The selected owner is an implementation baseline,
not a permanent architecture law.

Representative selected-baseline RAM witnesses remain approximately:

```text
CACHE_PRE  heap8 ~= 55100 largest8=36852
CACHE_POST heap8 ~= 27100 largest8=14324
early gameplay heap8 ~= 24400
post-lazy/dialog ownership heap8 ~= 21-24 KiB depending on corpus
largest8 = 14324 across normal gameplay witnesses
```

The global cache-reset cliff remains visible and is not fixed by this gameplay
milestone. Warm sprite phases can be tens of milliseconds; a cold/recycled
working set can still cost hundreds of milliseconds. `PlatformVideo_present()`
remains about 34-35 ms and is not the primary stutter source.

## Native gameplay boundary hardware validated

The following behavior has been exercised on the real CYD with normal
`esp32-cyd` firmware:

```text
TURN_LEFT / TURN_RIGHT
FORWARD / BACK / STRAFE
native collision/topology
SELECT event-first routing
EV_SHOW / EV_HIDE / EV_UNLOCK
EV_OPENLINE / EV_CLOSELINE
EV_DIALOG / EV_DIALOGNOBACK
EV_FORCEMESSAGE / EV_NOTE
state ops 11 / 19 / 20
regular door open/close animation
mutable line texture variants
native idle weapon rendering
generic weapon attack frame 1 -> idle frame 0
weapon pickup eType=5
empty/human Action feedback
adjacent extinguisher fire removal with rollback-safe presentation
move-event state mutation with rollback/commit
jammed-door destructible subtype-3 axe route
```

### EntityDef metadata catalog

The old type-only 817 B tile-index catalog was replaced by compact sorted
`{tile,type,subtype}` metadata records:

```text
capacity = 128 records
record = 4 B
owner = 512 B
definitions parsed = 115
```

This recovers roughly 305 B permanent RAM while exposing exact subtype metadata
needed by bounded destructible and future monster routing.

Real-CYD boot witness:

```text
[ENTITYDEFTYPE] READY defs=115 metadata=115 cache=512B ...
```

## Jammed-door destructible milestone — COMPLETE

Detailed milestone record: [`MILESTONE_NATIVE_JAMMED_DOOR.md`](MILESTONE_NATIVE_JAMMED_DOOR.md).

Recovered legacy contract for the Entrance damaged door:

```text
target tile = 686
line = 201
EntityDef type = 12
EntityDef subtype = 3
required weapon = axe / weapon 0
range = 1 tile
death event = event 72
runFlags = 0x00000100
event 72 eligible command = EV_OPENLINE
EV_OPENLINE arg1 = 0x000000c9 = line 201
XP consequence = +1 (still deferred)
```

The subtype-3 destructible does not own persistent HP like the power-coupling
subtype. At the tested initial accuracy/range, legacy hit calculation is 259,
which is above the byte RNG maximum and therefore guaranteed while still
consuming the legacy RNG byte.

Permanent route properties:

```text
exact target family only
fail closed for other destructibles
Random_t snapshot/rollback preserved
line mutation transactional
source event must resolve exactly to matching EV_OPENLINE
no legacy Game/Entity/Render world mutation
XP/sound/turn consequences remain explicit deferred work
```

### Real-CYD final PASS

The hardware-tested code boundary is:

```text
feae39c768105b8851a77dab1afa4b52bec231dd
ESP32: snap jammed door destruction
```

Final CYD witness:

```text
[ACTIONENGINE] TRACE ... tile=686 target=line line=201 type=12 subtype=3 route=JAMMED_DOOR_CLEARED
[DESTRUCTIBLE] ARM ... event=72 global=201 weapon=0 distance=1 runFlags=00000100 hitCalc=259
[DESTRUCTIBLE] HIT ... rand=0 calc=259 guaranteed=yes open=0->1 rngConsumed=1
[DESTRUCTIBLE] COMMIT ... line=201 event=72 open=0->1 xp=1-deferred rollback=closed
```

Crucially, the corrected subtype-3 route produces no `DOORANIM` sequence. The
legacy subtype does not allocate a destruction animation; the line state snaps
open and the normal renderer immediately observes it:

```text
[DYNAMICLINES] ... open=1 adaptedReads=2 animatedReads=0 ...
```

Collision was then hardware-proven against that new state:

```text
MOVE tile 654 -> 686 committed=yes
MOVE tile 686 -> 718 committed=yes
```

The second move also executed the next native state event transactionally:

```text
ENTER tile=718 status=SCRIPT_STATE_OK opcode=11
stateEvent=54 state=0->1 mutation=yes
[MOVEEVENT] COMMIT ... rollbackLease=closed
```

This proves the destroyed-door state is not presentation-only: rendering,
collision and subsequent event routing all consume the committed native world
state correctly.

No stack canary, reboot, renderer failure, rollback failure, or legacy
`shapeData` / `mediaTexels` allocation occurred.

## Next exact gameplay witness: dog behind line 201

Immediately after traversing the destroyed door, the real CYD identifies the
adjacent dog exactly:

```text
sprite index = 179
tile = 750
type = 1
subtype = 1
distance = 1
weapon = 0 (axe)
route = ENEMY_COMBAT_DEFERRED
```

Current intentional stop:

```text
[ACTIONENGINE] BACKEND-DEFER ...
family=monster-combat
reason=native-monster-hp+attack-state-not-owned
mutation=no
```

This is the next preferred bounded milestone after merge. Targeting is already
correct; the missing frontier is durable native monster combat ownership and
exact combat consequences.

## Next milestone direction — native monster combat

After this branch is merged:

1. read the true GitHub `main` and exact SHA;
2. branch fresh from that SHA;
3. recover the legacy combat contract for the exact dog corpus first;
4. design a compact mutable native monster owner keyed by immutable runtime
   sprite/entity identity;
5. preserve exact RNG consumption and rollback;
6. implement axe hit/damage against the adjacent dog before generalizing;
7. model alive/pain/death presentation without creating desktop `Entity_t` /
   `CombatEntity_t` graphs per monster;
8. keep ammo, generic ranged weapons, monster AI/turn attack, XP, sound and
   special monster families fail-closed until each has an explicit contract;
9. preserve `shapeData == NULL`, `mediaTexels == NULL` and the selected cache
   owner unchanged initially.

Preferred architecture:

```text
immutable EspMapRuntime / native entity metadata
        +
compact EspNativeMonsterState mutable owner
        +
transactional native combat executor
        +
bounded native visual state
```

Do not revive the legacy pointer-heavy world as the permanent solution.

## Other important deferred correctness families

Still intentionally separate:

```text
generic monster HP/damage/death beyond the first bounded dog case
monster AI / retaliation / turn advance
ammo consumption and ammo pickups
player HP/armor/stat pickups
inventory/key pickup ownership and CHECK_KEY
XP application / level progression
sound consequences
remote extinguisher miss/no-effect transaction
EV_GIVEMAP
EV_PASSWORD
EV_SAVEGAME persistence consumer
EV_CHECK_KEY
EV_CHANGEMAP live transition/stats handoff
```

### CHANGEMAP corpus already recovered

Entrance event 1 / tile 69 is known exactly and remains intentionally deferred
until gameplay prerequisites are sufficient:

```text
cmd0 EV_SAVEGAME
  target = /junction.bsp
  targetMapId = 9
  save position = 992,1888
  save angle = 64

cmd1 EV_CHANGEMAP
  target = /junction.bsp
  targetMapId = 9
  showStats = 1
  spawnParam = 0

cmd2 EV_OPENLINE
```

Do not force the level transition before the required gameplay systems make the
level completable normally.

## Renderer/cache interpretation

The selected cache is stable enough for correctness work but still exhibits
isolated recycle stalls. A representative destructible run showed a cold attack
frame with roughly:

```text
sprite phase ~= 729 ms
78 physical sprite reads
cache rebuilt to ~= 6 KiB / 54 records
```

The same semantic transaction still committed correctly. Treat this as a
performance witness, not a gameplay correctness failure.

Hot Serial diagnostics also inflate aggregate `totalUs`; phase-local counters are
more useful for cache diagnosis. Do not optimize `PlatformVideo_present()` first.

## Development workflow

```text
recover true main + PORTING_STATUS + DOCUMENTATION + latest milestone
 -> choose one bounded behavior family
 -> recover exact legacy semantics
 -> implement permanent compact native API/owner
 -> keep unsupported cases fail-closed
 -> commit + push agent/*
 -> test normal esp32-cyd on real CYD
 -> Serial is hardware truth
 -> fix failures and push without inventing results
 -> after PASS, docs-only tail
 -> declare merge-ready
```

Never merge to `main` without explicit user request. After the user announces a
merge, re-read true `main`, recover its exact SHA, and branch the next milestone
from that SHA.