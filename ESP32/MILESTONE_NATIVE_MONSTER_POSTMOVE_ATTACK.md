# Native monster post-move same-turn attack — real CYD PASS

This milestone closes one precise native monster-AI sequencing gap on the classic
ESP32-2432S028R path: after a committed one-step live monster move, the supported
legacy `Entity_aiMoveToGoal()` family can immediately attack in the same monster
turn when the destination is cardinally adjacent to the player and the trace is
clear.

## Git / hardware boundary

```text
branch = agent/esp32-native-monster-postmove-attack
base main = 98256de72f2f0d4640b7533122492b8ff1535c8b
hardware-tested code boundary = f017aff03f93dce7dd66cac91136cc01ad9fe20c
status = REAL-CYD ONE-STEP MONSTER POST-MOVE SAME-TURN ATTACK PASS
branch policy = LOCKED; docs-only tail only
```

The exact code boundary exercised on the real classic CYD is `f017aff0...`.
Commits after it must remain documentation-only until merge.

Normal GitHub Actions `esp32-cyd` run `33959809286` / run #119 completed
successfully on this SHA. Job `101289595476` (`PlatformIO esp32-cyd`) built the
classic CYD firmware and uploaded artifacts successfully.

CI is compile/link evidence only. The serial logs below are from the real classic
CYD and remain the runtime authority.

## Exact legacy boundary recovered

The earlier recovery note grouped subtypes `1`, `4`, `5`, `13` too broadly.
The true `Entity_aiMoveToGoal()` goal counts are:

```text
subtype 1 / 5  -> i = 1
subtype 4 / 13 -> i = 3
```

All four families can reach the legacy post-move attack gate, but subtype `4/13`
may attempt up to three same-turn movement goals before that gate. Native movement
currently owns and publishes one live move at a time, so this milestone owns only
the exact `i=1` family (`1/5`). Subtypes `4/13` remain fail-closed until a bounded
multi-step movement milestone owns their full same-turn goal sequence.

For the owned one-step family, the post-move attack gate requires:

```text
- a live move was successfully committed and rendered;
- subtype is 1 or 5;
- the committed destination is exact in MonsterPosition + topology;
- destination is cardinally adjacent to the settled player;
- distance^2 <= 4096 (one 64-unit tile);
- trace to the player is clear;
- weapon contract is the owned one-step melee / single-loop family;
- the attack probe can be rolled back exactly before live retaliation replay.
```

The movement trigger is also bounded: the new post-move goal is consumed only for
the legacy `NO-IMMEDIATE-ATTACK` movement path. The ranged `RANGED-AI` branch is
not blindly converted into a post-move attack.

## Native composition contract

The new permanent API is:

```c
int EspNativeGameplayMonsterTurn_postMoveGoal(
    struct DoomRPG_s* doomRpg,
    uint16_t spriteIndex,
    uint16_t sourceTile,
    uint16_t destTile);
```

It is called only after `EspNativeGameplayMonsterMovementPublish_afterProbe()` has
successfully committed the live position/topology transaction.

Composition is:

```text
existing MonsterTurn schedules player action
 -> existing movement planner probe
 -> existing live movement publication + RNG commit
 -> post-move goal probe on committed destination
 -> existing activation gate delivers the new attack probe
 -> existing MonsterAttackVisual consumes it
 -> existing MonsterRetaliation replays/commits combat RNG + PlayerState
```

The post-move owner does **not** create another scheduled monster turn and does not
introduce another damage backend. It reuses the existing `rollMonsterAttack()`
probe semantics and restores both `Random_t` and PlayerState before publishing the
attack probe. The already hardware-proven retaliation owner remains the only live
player-damage/RNG commit path.

## Real-CYD Hellhound witness

Entrance Hellhound `sprite=179`, subtype 1, primary melee attack (`weapon=12`,
`alt=0`, `loops=1`) was activated and then advanced by PASS_TURN.

First move — not yet adjacent, therefore no attack:

```text
[MONSTERMOVELIVE] COMMIT trigger=NO-IMMEDIATE-ATTACK sprite=179
    tile=750->718 pos=928,1504->928,1440 rngCalls=1
    randomCommitted=yes ... rollback=closed
[MONSTERPOSTMOVE] COMPLETE reason=PASS_TURN sprite=179 subtype=1
    tile=750->718 distance2=16384 adjacentCardinal=no
    attack=no mutation=no rngConsumed=0
```

Second move — destination becomes exactly one tile away:

```text
[MONSTERMOVELIVE] COMMIT trigger=NO-IMMEDIATE-ATTACK sprite=179
    tile=718->686 pos=928,1440->928,1376 rngCalls=1
    randomCommitted=yes ... rollback=closed
[MONSTERPOSTMOVE] ATTACK-PROBE reason=PASS_TURN sprite=179 subtype=1 mType=1
    tile=718->686 weapon=12 alt=0 loops=1 goalStep=1/1 distance2=4096
    adjacentCardinal=yes trace=clear hitLoops=1
    totalDamage=3 armorDamage=3 crit=1 rngCalls=2
    playerHP=30->27 armor=8->5
    playerFNV=f58f97ce->f58f97ce rng=f71b27b7->f71b27b7
    rngRollback=yes playerExact=yes sameTurn=yes
    movementAlreadyCommitted=yes gameplayMutation=no
[MONSTERACT] DELIVER actualProbe=1 deliveredProbe=1 sprite=179 reason=4 activated=yes
[MONSTERATKVIS] ARM probe=1 reason=PASS_TURN sprite=179 subtype=1 alt=0 loops=1
    visual=1 fixedAnim=yes leaseMs=150 ... rngExact=yes gameplayMutation=no
[MONSTERRETAL] COMMIT probe=1 reason=PASS_TURN sprite=179 subtype=1 weapon=12
    alt=0 loops=1 hitLoops=1 totalDamage=3 armorDamage=3 crit=1
    playerHP=30->27 armor=8->5
    playerFNV=f58f97ce->6452db32 rng=f71b27b7->1611ad51
    presented=1 message="Crit! 6 damage!" redFlash=b800/500ms
    passMessage=legacy-superseded rollback=closed turn=closed
[MONSTERATKVIS] EXPIRE probe=1 sprite=179 visual=1->idle leaseMs=150
    presented=1 rngExact=yes gameplayMutation=no
```

Critically, there is no additional `[MONSTERTURN] SCHEDULE` between the committed
`718->686` movement and `[MONSTERPOSTMOVE] ATTACK-PROBE`. The move and attack are
therefore one native monster turn, matching the recovered legacy sequencing.

The first `750->718` move is also an important negative witness: the same subtype
and trigger do **not** attack before the exact adjacent destination is reached.

## RNG / transaction witness

The first movement in the supplied session also crossed the byte-table boundary
through the existing reservation owner:

```text
[RNGGUARD] PROBE-REFILL ... next=127->0 ... reservation=until-live-replay
[MONSTERMOVERNG] ARM trigger=NO-IMMEDIATE-ATTACK ... prepared=1
[RNGGUARD] PROBE-COMMIT ... bytes=1 ... sequenceExact=yes
[MONSTERMOVERNG] COMMIT trigger=NO-IMMEDIATE-ATTACK rngCalls=1
    reservation=consumed-by-live-move randomLive=advanced-exactly
```

The post-move attack probe itself left gameplay state unchanged before the proven
retaliation replay:

```text
playerFNV=f58f97ce->f58f97ce
rng=f71b27b7->f71b27b7
rngRollback=yes
playerExact=yes
gameplayMutation=no
```

The retaliation then consumed the same recovered two attack RNG bytes and became
the sole live commit owner.

## Memory witness

The real-CYD session remained stable before and after movement, same-turn attack,
feedback expiry and later player combat:

```text
heap = 86524
heap8 = 20792
largest8 = 18420
shapeData = NULL
mediaTexels = NULL
```

No PSRAM is present. Audio remains deferred.

## Deliberately deferred after this milestone

```text
subtype 4 / 13 three-goal same-turn movement chain
monster movement interpolation / animation
multiple-live-monster activation and ordering
unsupported special calcPath plane corpus
special subtype-10 AI
three-shot / multi-loop monster attack presentation
monster projectile visuals
monster attack message / sound
player pain face / shake / sound
player lethal/death transition
```

A natural next behavior milestone is the exact `i=3` same-turn goal chain for
subtypes `4/13`, but it must own all required movement steps and RNG ordering
rather than approximating the legacy behavior after one published step.
