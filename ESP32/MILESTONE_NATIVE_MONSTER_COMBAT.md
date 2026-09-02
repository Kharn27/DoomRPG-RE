# Milestone — Generic native monster combat

## Git / hardware boundary

```text
base main = 563804b09fda67ba06516c8dc13585a1125a4bb0
branch = agent/esp32-native-dog-combat
hardware-tested code boundary = e56bfcf86489f5b0f9ae10deb29a73fabf098756
status = REAL-CYD PASS
```

Despite the historical branch name, the final engine is **not dog-specific**.
The dog was only the first hardware corpus witness.

## Goal

Land the first permanent native monster-combat family without reviving the
legacy desktop entity graph.

Required architecture:

```text
immutable EspMapRuntime / EntityDef metadata
        +
compact mutable MonsterState
        +
generic MonsterTrace
        +
shared CombatMath tables
        +
compact shared PlayerState
        +
transactional MonsterCombat orchestration
```

Ordinary monster differences must be represented by data (`subtype`, `mType`,
stats, weapon metadata), not by per-monster executors.

## Compact monster initialization

Entrance contains 30 native enemy records.

```text
record = 16 B
owner = 480 B
legacy-compatible RNG calls = 180
```

Real-CYD witness:

```text
[MONSTERSTATE] READY arena=c3882516 enemies=30 ownerBytes=480 recordBytes=16 enemyDefs=38 rngCalls=180 rng=e19e2f15->76e68ad6 stateFNV=ff52899c noLegacyEntity=yes packOpen=0
[MONSTERSTATE] WITNESS sprite=179 defTile=20 subtype=1 mType=1 hp=6/6 armor=2/2 def=10 str=12 agi=10 acc=10 alt=0 alive=1
```

This validates map-load enemy randomization without permanent `Entity_t` /
`CombatEntity_t` ownership.

## Permanent engine modules

```text
ESP32/include/esp_native_gameplay_monster_state.h
ESP32/src/esp_native_gameplay_monster_state.c

ESP32/include/esp_native_gameplay_monster_trace.h
ESP32/src/esp_native_gameplay_monster_trace.c

ESP32/include/esp_native_gameplay_combat_math.h
ESP32/src/esp_native_gameplay_combat_math.c

ESP32/include/esp_native_gameplay_player_state.h
ESP32/src/esp_native_gameplay_player_state.c

ESP32/include/esp_native_gameplay_monster_combat.h
ESP32/src/esp_native_gameplay_monster_combat.c
```

`PlayerState` is 52 B and is intentionally the single native destination for
player combat stats, XP/progression and future pickups/resources.

## Legacy behavior recovered

The native path reproduces the relevant integer behavior from the legacy combat
implementation:

- hit roll and range decay;
- crit threshold;
- weapon strength roll;
- armor split;
- `mType` resistance/weakness multipliers;
- pain/death RNG consumption;
- XP before later death/drop RNG consequences;
- level-up capable progression owner;
- one full `Random_t` transaction boundary.

Presentation ownership remains native:

- pain visual = state 6 for 250 ms;
- normal corpse = state 4 + unlinked liveness;
- recovered adjacent overkill/gib case = hidden + unlinked;
- no legacy monster/entity mutation is required.

## Real-CYD PASS — witness A: Hellhound subtype 1

Initial canonical witness:

```text
sprite=179
tile=750
subtype=1
mType=1
hp=6/6
armor=2/2
def=10
str=12
agi=10
acc=10
```

### Hit 1

```text
[MONSTERCOMBAT] ARM seq=85 sprite=179 tile=750 subtype=1 mType=1 weapon=0 distance=1 hp=6/6 armor=2/2 def=10 agi=10 playerAcc=16 playerStr=12 backend=generic-type1 rng=pending mutation=no rollback=armed
[MONSTERCOMBAT] ROLL seq=85 sprite=179 subtype=1 mType=1 weapon=0 distance=1 worldDist=4096 loops=1 hitLoops=1 firstRandHit=86 firstCalcHit=293 firstCritLimit=14 firstRandDamage=5 totalDamage=3 armorDamage=0 crit=0 rngCalls=3
[MONSTERCOMBAT] COMMIT seq=85 sprite=179 subtype=1 hp=6->3 armor=2->2 alive=1->1 monsterFNV=ff7d6e20->21afda01 playerFNV=d60a9866->d60a9866 ammo=0->0 visual=pain6/250ms attackSound=5136-deferred consequenceSound=5089-deferred xp=0-applied level=0->0 levelUps=0 dropRoll=unused/00000000 dropMaterialize=deferred corpseTrim=deferred turnAdvance=deferred AI=deferred rollback=closed
```

### Hit 2 / kill

```text
[MONSTERCOMBAT] ARM seq=86 sprite=179 tile=750 subtype=1 mType=1 weapon=0 distance=1 hp=3/6 armor=2/2 def=10 agi=10 playerAcc=16 playerStr=12 backend=generic-type1 rng=pending mutation=no rollback=armed
[MONSTERCOMBAT] ROLL seq=86 sprite=179 subtype=1 mType=1 weapon=0 distance=1 worldDist=4096 loops=1 hitLoops=1 firstRandHit=197 firstCalcHit=293 firstCritLimit=14 firstRandDamage=182 totalDamage=10 armorDamage=1 crit=0 rngCalls=3
[MONSTERCOMBAT] COMMIT seq=86 sprite=179 subtype=1 hp=3->0 armor=2->1 alive=1->0 monsterFNV=21afda01->63e46bde playerFNV=d60a9866->2b032c86 ammo=0->0 visual=gib-hidden+unlink attackSound=5136-deferred consequenceSound=5091-deferred xp=5-applied level=1->1 levelUps=0 dropRoll=value/33ff5932 dropMaterialize=deferred corpseTrim=deferred turnAdvance=deferred AI=deferred rollback=closed
```

This proves HP persistence across multiple attacks and a complete generic lethal
transaction on hardware.

## Real-CYD PASS — witness B: Zombie subtype 0

A different subtype/mType uses the same engine:

```text
[MONSTERCOMBAT] ARM seq=107 sprite=106 tile=424 subtype=0 mType=0 weapon=0 distance=1 hp=7/7 armor=5/5 def=17 agi=16 playerAcc=16 playerStr=12 backend=generic-type1 rng=pending mutation=no rollback=armed
[MONSTERCOMBAT] ROLL seq=107 sprite=106 subtype=0 mType=0 weapon=0 distance=1 worldDist=4096 loops=1 hitLoops=1 firstRandHit=70 firstCalcHit=217 firstCritLimit=10 firstRandDamage=80 totalDamage=11 armorDamage=1 crit=0 rngCalls=4
[MONSTERCOMBAT] COMMIT seq=107 sprite=106 subtype=0 hp=7->0 armor=5->4 alive=1->0 monsterFNV=63e46bde->6b9831fb playerFNV=2b032c86->d4746106 ammo=0->0 visual=death4+unlink attackSound=5136-deferred consequenceSound=5107-deferred xp=6-applied level=1->1 levelUps=0 dropRoll=value/02bcbb60 dropMaterialize=deferred corpseTrim=deferred turnAdvance=deferred AI=deferred rollback=closed
```

The zombie dies in one hit. Later SELECT taps still produce the older raw
compatibility action trace, but no new `[MONSTERCOMBAT] ARM` occurs because the
native record is already `alive=0`. This is expected log noise and confirms the
combat liveness filter is effective.

## Hardware RAM witness

Representative post-combat runtime:

```text
heap=86432
heap8=20668
largest8=14324
```

No PSRAM, no stack canary, no reboot, no renderer rollback failure, and no
`shapeData` / `mediaTexels` allocation were observed.

## Explicit deferred families

The milestone does **not** claim complete Doom RPG combat. Remaining work is
partitioned by real mechanical family:

```text
enemy AI / retaliation / turn advancement
sound playback
corpse-pile trimming
materialized drops
ammo-consuming direct-fire transaction
chaingun/plasma multi-loop presentation/commit
rocket/BFG radius damage
special death behavior: subtypes 7, 8, 12, 13
Kronos-specific teleport effects where applicable
```

These must not become per-monster development ladders.

## Next direction

After merge, the next bounded family should use the already-landed shared
`PlayerState` to generalize player resources and pickups:

```text
health + armor + credits + keys + ammo + inventory + weapons
        -> one shared PlayerState
        -> one generic pickup/resource executor
        -> enable standard ammo-consuming direct-fire combat
```

Hardware validation should deliberately exercise several different pickup types
and several different weapons to prove the generic engine boundary.
