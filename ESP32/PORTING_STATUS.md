# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime authority.

## Git boundary — LOCKED milestone

```text
main = 563804b09fda67ba06516c8dc13585a1125a4bb0
branch = agent/esp32-native-dog-combat
base main = 563804b09fda67ba06516c8dc13585a1125a4bb0
hardware-tested code boundary = e56bfcf86489f5b0f9ae10deb29a73fabf098756
status = REAL-CYD GENERIC NATIVE MONSTER COMBAT PASS
branch policy = LOCKED; docs-only tail only
```

`e56bfcf...` is the code exercised on the real CYD. Commits after that boundary
must remain documentation-only until this branch is merged.

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

The global cache-reset cliff remains visible and is not fixed by this gameplay
milestone. Preserve this baseline while correctness work advances.

## Generic native monster-state initialization — hardware PASS

Entrance monster initialization is now a permanent compact owner rather than a
legacy `Entity_t`/`CombatEntity_t` graph.

```text
[MONSTERSTATE] READY arena=c3882516 enemies=30 ownerBytes=480 recordBytes=16 enemyDefs=38 rngCalls=180 rng=e19e2f15->76e68ad6 stateFNV=ff52899c noLegacyEntity=yes packOpen=0
[MONSTERSTATE] WITNESS sprite=179 defTile=20 subtype=1 mType=1 hp=6/6 armor=2/2 def=10 str=12 agi=10 acc=10 alt=0 alive=1
```

Canonical owner:

```text
30 enemies
16 B / enemy
480 B total on Entrance
180 exact legacy RNG calls during initialization
```

## Generic native monster combat — COMPLETE hardware milestone

Detailed milestone record:

[`MILESTONE_NATIVE_MONSTER_COMBAT.md`](MILESTONE_NATIVE_MONSTER_COMBAT.md)

Permanent modules:

```text
esp_native_gameplay_monster_state.*   compact mutable enemy HP/armor/stats/liveness
esp_native_gameplay_monster_trace.*   generic forward target trace
esp_native_gameplay_combat_math.*     shared hit/crit/damage + weapon/mType tables
esp_native_gameplay_player_state.*    shared compact player owner
esp_native_gameplay_monster_combat.*  transaction orchestration + visual/liveness overlay
```

The combat backend is generic for `type=1`; ordinary monsters are data-driven by
`subtype/mType`. The dog was only the first corpus witness, not a permanent
special route.

The shared `EspNativeGameplayPlayerState` is 52 B and is the intended owner for:

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

This owner is deliberately shared by combat, future pickups, keys and progression.
Do not create per-item or per-feature player mini-owners.

### Real-CYD witness A — Hellhound subtype 1

First hit, same persistent monster owner:

```text
[MONSTERCOMBAT] ARM seq=85 sprite=179 tile=750 subtype=1 mType=1 weapon=0 distance=1 hp=6/6 armor=2/2 def=10 agi=10 playerAcc=16 playerStr=12 backend=generic-type1 rng=pending mutation=no rollback=armed
[MONSTERCOMBAT] ROLL seq=85 ... firstRandHit=86 firstCalcHit=293 firstCritLimit=14 firstRandDamage=5 totalDamage=3 armorDamage=0 crit=0 rngCalls=3
[MONSTERCOMBAT] COMMIT seq=85 ... hp=6->3 armor=2->2 alive=1->1 ... visual=pain6/250ms ... xp=0-applied ... rollback=closed
```

Second hit kills the same record:

```text
[MONSTERCOMBAT] ARM seq=86 sprite=179 ... hp=3/6 armor=2/2 ...
[MONSTERCOMBAT] ROLL seq=86 ... firstRandHit=197 ... firstRandDamage=182 totalDamage=10 armorDamage=1 crit=0 rngCalls=3
[MONSTERCOMBAT] COMMIT seq=86 ... hp=3->0 armor=2->1 alive=1->0 ... visual=gib-hidden+unlink ... xp=5-applied level=1->1 levelUps=0 dropRoll=value/33ff5932 ... rollback=closed
```

This proves persistent HP mutation, pain presentation, lethal liveness change,
XP mutation and renderer commit on a real CYD.

### Real-CYD witness B — Zombie subtype 0

A second subtype used the exact same backend and was killed in one hit:

```text
[MONSTERCOMBAT] ARM seq=107 sprite=106 tile=424 subtype=0 mType=0 weapon=0 distance=1 hp=7/7 armor=5/5 def=17 agi=16 playerAcc=16 playerStr=12 backend=generic-type1 rng=pending mutation=no rollback=armed
[MONSTERCOMBAT] ROLL seq=107 ... firstRandHit=70 firstCalcHit=217 firstCritLimit=10 firstRandDamage=80 totalDamage=11 armorDamage=1 crit=0 rngCalls=4
[MONSTERCOMBAT] COMMIT seq=107 ... hp=7->0 armor=5->4 alive=1->0 ... visual=death4+unlink ... xp=6-applied level=1->1 levelUps=0 dropRoll=value/02bcbb60 ... rollback=closed
```

Subsequent SELECTs still show the older compatibility `[ACTIONENGINE] TRACE`
against raw sprite 106, but no new `[MONSTERCOMBAT] ARM` occurs because native
combat correctly sees `alive=0`. This is compatibility-log noise, not a liveness
bug; the zombie is already dead after the first axe hit.

## Combat semantics now owned

For the currently enabled direct single-target family:

```text
generic type=1 target trace
persistent HP / armor
player accuracy / strength from PlayerState
legacy integer hit calculation
legacy crit threshold
legacy integer damage / armor split
mType resistance/weakness table
transactional RNG
pain visual state 6
normal death state 4 + unlink
gib-hidden + unlink where recovered
permanent XP application
permanent level/XP owner
RNG-capable level-up path
full RNG + monster + player rollback if render fails
```

Ordinary monsters must stay table/data-driven. Do not add per-zombie, per-dog,
per-imp or per-pinky combat handlers.

## Intentionally deferred combat families

Mechanically distinct families still remain explicit and fail-closed/deferred:

```text
enemy retaliation / native monster AI turn
actual sound playback
corpse-pile trimming
materialized monster drops
fully enabled ammo-consuming weapon transaction
multi-loop presentation/commit for chaingun/plasma
radius damage for rocket/BFG
special death consequences for subtypes 7, 8, 12, 13
Kronos-specific hit/teleport semantics where applicable
```

These are family boundaries, not a monster-by-monster implementation ladder.

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
weapon pickup eType=5
adjacent extinguisher fire removal
move-event state mutation with rollback/commit
jammed-door destructible subtype-3 destruction and traversal
```

Jammed door still keeps its historical +1 XP deferred; that old route has not yet
been migrated into the new permanent player progression owner.

## Next milestone after merge — generic resources/pickups + ammo weapons

Do **not** continue code on this locked branch after the hardware PASS.

After merge:

1. read actual GitHub `main` and exact SHA;
2. create a fresh coherent `agent/*` branch;
3. recover exact legacy semantics for player-facing pickup families;
4. route health, armor, credits, keys, ammo, inventory and weapon acquisition
   through the one `EspNativeGameplayPlayerState` owner;
5. enable standard ammo-consuming direct-fire weapons through the existing
   table-driven combat backend;
6. validate multiple distinct pickup categories and multiple distinct weapons as
   corpus witnesses for one generic resource engine;
7. keep radius-damage weapons and materially different scripted effects
   fail-closed until their family owner exists.

```text
NO per-casque ladder.
NO per-gourde ladder.
NO per-monster ladder.
```

## CHANGEMAP remains deferred

Entrance event 1 / tile 69 is already recovered but intentionally not live yet:

```text
SAVEGAME -> /junction.bsp, targetMapId 9, savePos 992,1888 angle 64
CHANGEMAP -> /junction.bsp, targetMapId 9, showStats 1, spawnParam 0
OPENLINE -> third eligible command
```

Do not force the transition before enough native gameplay exists to complete the
map normally.

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
