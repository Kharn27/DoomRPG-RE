# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime authority.

## Git boundary — LOCKED milestone

```text
main = 65dea748ef8d2e5a6f3823676ac05ee62fb89407
branch = agent/esp32-native-monster-movement
base main = 65dea748ef8d2e5a6f3823676ac05ee62fb89407
hardware-tested code boundary = 7dd7faa3994969e43ccb27b5450ee828ade3c323
status = REAL-CYD NATIVE MONSTER POSITION + MOVEMENT PLANNER PROBE + RNG RESERVATION PASS
branch policy = LOCKED; docs-only tail only
```

`7dd7faa...` is the last code commit exercised on the real CYD. Commits after
that boundary must remain documentation-only until merge.

After merge, read the real GitHub `main` SHA again before creating the next
`agent/*` branch.

## Permanent architecture and hard invariants

```text
A NEW BSP IS NOT A NEW ENGINE.
A NEW MONSTER IS NOT A NEW COMBAT BACKEND.
A NEW PICKUP MUST NOT BECOME A NEW MINI-OWNER.
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
or runtime ZIP dependence for migrated gameplay/map data.

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

Core owner fingerprints retained as regression witnesses:

```text
mapStateFNV  = cd99b98e
scriptFNV    = f9e3d9df
lineFNV      = e5e74861
textureFNV   = f1fc1875
automapFNV   = 669b1aa7
topologyFNV  = 3f321e43
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

The global cache-reset cliff remains separate performance work. Preserve this
baseline while correctness work advances.

## Generic native monster state/combat — hardware PASS

Detailed milestone:

[`MILESTONE_NATIVE_MONSTER_COMBAT.md`](MILESTONE_NATIVE_MONSTER_COMBAT.md)

Permanent engine split:

```text
MonsterTrace
 -> CombatMath
 -> MonsterState + PlayerState
 -> MonsterCombat transaction
 -> renderer/liveness overlays
```

Entrance monster owner:

```text
30 enemies
16 B / enemy
480 B total
180 legacy-compatible RNG calls during initialization
```

The combat backend is generic for `type=1`; ordinary monsters remain data-driven
by `subtype/mType` and compact stats. Hellhound and Zombie have both been used as
hardware corpus witnesses through the same backend.

Owned player-attack semantics include persistent monster HP/armor, exact integer
hit/crit/damage math, mType resistance/weakness, transactional gameplay RNG,
pain/death/gib presentation, XP application into PlayerState, and rollback on
render failure.

## Shared native PlayerState

`EspNativeGameplayPlayerState` is the one 52 B player-facing owner for:

```text
HP / max HP
armor / max armor
defense / strength / agility / accuracy
XP / level / next-level XP
keys
credits
ammo[6]
inventory[5]
weapon bits / selected weapon
```

Combat, pickups, ammo, progression and monster retaliation share this owner. Do
not create per-feature or per-item player-state islands.

## Generic player resources / pickups — hardware PASS

Detailed milestone:

[`MILESTONE_NATIVE_PLAYER_RESOURCES.md`](MILESTONE_NATIVE_PLAYER_RESOURCES.md)

Native EntityDef metadata retains compact `{tile,type,subtype,parm}` records and
is allocated only when gameplay metadata is built.

Real-CYD Entrance witness:

```text
[ENTITYDEFTYPE] READY defs=115 metadata=115 cache=920B recordBytes=8 ...
[PLAYERRES] READY map=1 arena=c3882516 sprites=344 consumedBytes=43 playerBytes=52 ... families=3/4/5/6/16
[PLAYERRES] CORPUS map=1 arena=c3882516 pickups=114 type3=84 type4=6 type5=3 type6=17 type16=4 routes=all-generic playerOwner=shared
```

Hardware-validated families:

```text
type 3  health / armor / credits / keys
type 4  inventory
type 5  weapons
type 6  ammo
type 16 alternate ammo entries
```

HUD health/armor/weapon/ammo projection reads the same PlayerState.

## Extinguisher ammo transaction — hardware PASS

Fire removal consumes ammo from PlayerState transactionally:

```text
[ACTIONENGINE] FIRE-COMMIT ... ammoType=0 ammo=10->9 ... rollback=closed
```

The real HUD decremented with the same owner. Fire +2 XP and historical
jammed-door +1 XP remain deferred.

## Generic monster presentation — hardware PASS

```text
nonlethal hit
 -> pain visual 6 / 250 ms
 -> normal visual

ordinary non-gib death
 -> death visual 4 / 250 ms
 -> corpse visual 2, unlinked

overkill/gib death
 -> death visual 4 / 250 ms
 -> hidden + bounded native gib burst
 -> burst expires after 350 ms by world redraw
```

The gib layer is presentation-only and uses local deterministic visual RNG; it
does not consume gameplay RNG or revive legacy ParticleSystem ownership.

## Native monster turn / retaliation — hardware PASS

Detailed milestone:

[`MILESTONE_NATIVE_MONSTER_TURN.md`](MILESTONE_NATIVE_MONSTER_TURN.md)

This is the first live enemy-turn family. It is generic, not dog/zombie-specific.
A committed MOVE, ROTATE, or PLAYER_ATTACK can schedule a native turn. The
bounded executor accepts one unambiguous active `type=1` attacker with cardinal
native LOS and a recovered supported monster weapon.

Owned live consequences:

```text
stationary cardinal attacker selection
native tile / line / sprite LOS
legacy-compatible monster weapon selection
legacy-compatible AI decision byte where applicable
legacy-compatible hit / crit / damage / armor math
nonlethal HP + armor mutation in shared PlayerState
transactional native redraw
miss gameplay-RNG commit path
render/RNG failure rollback
```

Still fail-closed inside retaliation:

```text
multiple ambiguous attackers / activation ordering
special subtype-10 AI
player lethal/death transition
dog-familiar damage redirection
attack visual / player pain FX / damage text / sound
```

## Native monster position + movement planner — hardware PASS

Detailed milestone:

[`MILESTONE_NATIVE_MONSTER_MOVEMENT.md`](MILESTONE_NATIVE_MONSTER_MOVEMENT.md)

A permanent compact spatial owner now exists independently of immutable BSP
sprite coordinates:

```text
record = {spriteIndex,tileIndex,worldX,worldY}
recordBytes = 8
Entrance records = 30
payload = 240 B
initial source = native topology tile center
allocation = one load/session allocation
```

The recovered generic movement planner owns the first bounded
`Entity_aiThink`/`Entity_aiGoal_MOVE` behavior:

```text
attack LOS mask = 0x5687
movement mask = 0xff87 with legacy subtype adjustments
2-step calcPath greedy look-ahead
legacy east/west/south/north ordering
legacy north closestPathDist quirk preserved
visit choice = visitOrder[(rand & 3) % visitCount]
NO-IMMEDIATE-ATTACK and RANGED-AI producer paths
```

The current milestone is still **probe-only**: one active ordinary monster may
prepare and commit one cardinal position, fingerprint it, then rollback exactly.
No renderer publication, topology relink, interpolation, or visible monster
movement occurs yet.

Decisive real-CYD position witness:

```text
sprite=179 subtype=1 weapon=12
sourceTile=750 source=928,1504
destTile=718 delta=0,-64
tieRand=217 choice=0 mask=ff87 rngCalls=1
randomLiveUntouched=yes
positionFNV=61296c4a->10cf73aa->61296c4a
positionRollback=yes
rendererPublish=deferred topologyRelink=deferred liveMove=no mutation=no
```

## RNG refill transaction + movement reservation — hardware PASS

Hardware testing previously proved that `Random_t` alone is not the full legacy
RNG state. `DoomRPG_setRand()` regenerates the 128-byte table using hidden
file-static generator state.

The generic guard now supports two bounded replay modes:

```text
ordinary rollback lease = 1000 ms
persistent movement-probe reservation = exact live Random_t pointer + pre/post table
hidden generator = advances once per logical refill
allocation = none
current static guard owner = 284 B on 32-bit ESP32
```

The movement reservation may survive well beyond the ordinary 1000 ms lease. A
probe temporarily borrows the reserved post-refill state, restores live
`Random_t` exactly, and leaves the table reserved for the next true byte-boundary
draw.

A hardware test caught an important second-order bug: consuming the reservation
at the monster-turn **probe** was too early because that probe itself rolls RNG
back. The final code converts `PROBE-REPLAY` into the ordinary rollback lease so
live retaliation can reuse the exact same table.

Decisive final real-CYD chain:

```text
[RNGGUARD] PROBE-REFILL refill=1 ... hiddenGenerator=advanced-once ...
[MONSTERMOVE] PROBE ... tieRand=217 ... randomLiveUntouched=yes ... positionRollback=yes
[RNGGUARD] PROBE-RESTORE refill=1 liveRandomExact=yes reservation=pending ...
[RNGGUARD] PROBE-REPLAY refill=1 replay=1 leaseMs=1000 ... reservation=consumed rollbackReplay=armed sequenceExact=yes
[MONSTERTURN] ATTACK-PROBE ... firstRandHit=217 ... missProjectileRng=1 rngRollback=yes playerExact=yes
[RNGGUARD] REPLAY refill=1 replay=2 leaseMs=1000 ... hiddenGenerator=untouched rollbackSafe=yes
[MONSTERRETAL] MISS-COMMIT ... firstRandHit=217 ... gameplayRngCommitted=yes playerMutation=no
```

A subsequent ordinary retaliation in the same session matched probe/live with
`firstRandHit=203`, `firstRandDamage=213`, `damage=2`, `armorDamage=2`, committing
player `HP 30->28`, `armor 12->10`.

## Representative final RAM witness

Latest real-CYD gameplay logs at the locked code boundary:

```text
heap = 84648 B
heap8 = 18884 B
largest8 = 13812 B
shapeData = NULL
mediaTexels = NULL
PSRAM = none
```

The selected resident asset-cache baseline remains unchanged.

## Existing gameplay boundary retained

Previously hardware-validated systems remain intact:

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
generic attack frame presentation
move-event state mutation with rollback/commit
jammed-door destructible subtype-3 destruction and traversal
generic monster state + player-attack combat
generic player resources / pickup disappearance
shared PlayerState HUD projection
extinguisher ammo consumption
generic death / corpse / gib presentation
bounded generic stationary monster retaliation
compact mutable monster-position ownership
bounded legacy-compatible monster movement planner probe
persistent boundary-safe RNG reservation/replay
```

## Intentionally deferred families

Still explicit/deferred:

```text
pickup sounds/messages/got-face presentation
combat/retaliation MISS/HIT/CRIT text feedback
fire +2 XP and jammed-door +1 XP migration into PlayerState
materialized monster drops
corpse-pile trimming
live monster movement publication into topology/renderer
monster movement interpolation/animation
multiple-monster activation/movement ordering
unsupported special calcPath plane corpus
special subtype-10 AI
player death/lethal retaliation transition
monster attack/pain presentation and sound
full native turn advancement orchestration
multi-loop chaingun/plasma presentation/commit
rocket/BFG radius damage
special death consequences for subtypes 7, 8, 12, 13
Kronos-specific semantics
password input
SAVEGAME / CHANGEMAP production transition consumer
```

These are mechanical family boundaries, never an item-by-item or monster-by-
monster implementation ladder.

## CHANGEMAP remains deferred

Entrance event 1 / tile 69 remains recovered but intentionally not live yet:

```text
SAVEGAME -> /junction.bsp, targetMapId 9, savePos 992,1888 angle 64
CHANGEMAP -> /junction.bsp, targetMapId 9, showStats 1, spawnParam 0
OPENLINE -> third eligible command
```

Do not force the transition before enough native gameplay exists to complete the
map normally.

## Next direction after merge

Do **not** continue code on this locked branch.

After merge:

1. read actual GitHub `main` and exact SHA;
2. create a fresh coherent `agent/*` branch from that SHA;
3. re-read this status, `DOCUMENTATION.md`, and the movement milestone;
4. choose the next bounded gameplay family from the merged frontier.

Strong candidates now include:

```text
live monster movement publication / topology relink / renderer projection
monster attack + player-pain presentation and combat feedback
player lethal/death transition
action XP migration into PlayerState
materialized monster drops
```

Choose only after re-reading merged `main`.

## Development workflow

```text
recover true main + PORTING_STATUS + DOCUMENTATION + latest milestone
 -> choose one bounded behavior FAMILY
 -> recover exact legacy semantics
 -> implement permanent compact native API/owner
 -> keep materially unsupported families fail-closed
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
