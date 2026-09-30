# Milestone — MonsterMovement ActiveSequence explicit

Date: 2026-09-30

## Boundary

```text
main = c37c66ad800603ea7d0622681a3bfaf5bab0b41d
branch = agent/esp32-consolidation-monster-movement-service-v6
hardware-tested code = c27b94e263f4ea0d445e7debeea4d836dbd59e4f
esp32-cyd CI #1069 = SUCCESS
static RAM = 45776 B
flash = 815677 B
artifact id = 11084465870
translation units = 173
active --wrap entries = 57
active EspNativeGameplayMonster-prefixed wraps = 0
```

## Change

The final monster-domain linker seam,
`--wrap=EspNativeGameplayMonsterMovement_service`, is removed.

Permanent composition is now explicit:

```text
GameplaySession_service
 -> MonsterMovementProbe_service
 -> MonsterActiveSequence_service
 -> MonsterMovementProbe_serviceMember
 -> MonsterMovement_service
 -> MovementPublish
 -> postMoveGoal / three-goal continuation
```

The movement service remains the unchanged single-candidate planner leaf.
ActiveSequence owns ordered expansion across active monsters.

## Real-CYD proof

One Sector 1 MOVE delivers four members in order:

```text
218 subtype 3: movement COMMIT + POSTMOVE COMPLETE
237 subtype 5: movement COMMIT + POSTMOVE COMPLETE
0 subtype 4: 470 -> 471 -> 470 -> 438, THREE-GOAL COMPLETE
1 subtype 4: 534 -> 535 -> 536 -> 537, THREE-GOAL COMPLETE
MONSTERACTIVESEQ COMPLETE activeCount=4 delivered=4 ordered=yes publication=per-member
```

Every live movement closes rollback before the next member. Two following
ALIVE samples are identical:

```text
heap=82704 heap8=17152 largest8=10228
heap=82704 heap8=17152 largest8=10228
```

A later real-CYD PASS_TURN supplied the complementary ranged-attack witness.
Sprite 218 / subtype 3 / weapon 15 attacked from range directly through the
attack pipeline:

```text
[MONSTERTURN] ATTACK-PROBE ... sprite=218 subtype=3 weapon=15 loops=1 ...
[MONSTERACT] DELIVER actualProbe=5 deliveredProbe=5 sprite=218 reason=4 activated=yes
[MONSTERATKVIS] ARM ... loops=1 ... gameplayMutation=no
[MONSTERATKVIS] COMPLETE ... resolution=unblocked-after-animation
[MONSTERRETAL] COMMIT ... playerHP=22->20 armor=12->10 ... rollback=closed
```

The attack probe itself rolls player/RNG state back exactly, presentation runs
without gameplay mutation, then retaliation commits only after the attack visual
completes. Several following ALIVE samples remain stable at
`heap=82704 heap8=17152 largest8=10228`.

Crucially, this genuine ranged attack emits no `RANGED-MEMBER`. That confirms
the naming boundary: `RANGED-MEMBER` is the unchanged exact-source ranged-AI
movement/repositioning branch, not the monster attack
presentation/resolution path.

## Result

The code boundary is hardware validated. There are now zero active linker wraps
whose target symbol starts with `EspNativeGameplayMonster`.

All commits after the hardware-tested code boundary must remain documentation-only.
