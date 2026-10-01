# ESP32 — native subtype 4/13 three-goal multi-loop monster attack

Date: 2026-10-01

## Boundary

```text
branch = agent/esp32-retire-legacy-particle-startup-v22
code = 5943974dcf1b5bd1c142e4665340f51fb6fbb19b
current main in lineage = 72e351f8d26f4c3ac22d766a086de6646bbaf77b
CI = #1253 SUCCESS
static RAM = 44936 B
linked Flash = 761949 B
firmware.bin = 762320 B
artifact = 11191212285
artifact digest = sha256:42b26ff060341d02dbc37954006df9825dc7a12463e7168ed9f09cc68de02966
firmware sha256 = 0e4cc7b4dee946faaa04a26462f1ae01914bb68251c52412d6f3467b2002ba20
ELF sha256 = ed1350736fd0f855a1e9f62963e9e9332056cf9c45e28f291a4f0edc65ba4481
```

## Goal

Retire the explicit `multi-loop-attack-family-deferred` boundary reached after
the hardware-proven subtype-4/13 three-goal movement chain, without enabling a
new combat architecture.

The legacy reference increments the three-goal family frameTime after each
completed goal and jumps directly to frameTime 3 when cardinal distance reaches
one tile. At the attack gate, subtype 4/13 uses the existing three-shot
`monsterWpInfo` family.

## Native design

The implementation keeps one permanent attack pipeline:

```text
ThreeGoalTurn
  -> MonsterTurn three-loop prospective probe
  -> MonsterActivation delivery
  -> MonsterAttackVisual 3-loop cadence
  -> MonsterRetaliation exact replay/commit
```

No legacy Entity/Combat ownership is reintroduced. The ThreeGoal owner publishes
only after exact committed position/topology checks, cardinal trace validation,
three-loop weapon validation and rollback-exact prospective combat.

A one-probe-in-flight rule closes ordering ambiguity. If another post-move
attacker becomes ready before the previous probe is delivered, publication
fails closed rather than overwriting the producer identity.

## Real-CYD proof

The loaded Sector 1 save naturally activates two subtype-4 monsters. One reaches
the player after its second movement goal:

```text
[MONSTER3GOAL] COMMIT sprite=1 goal=2/3 tile=538->506 rngCalls=1 ...
[MONSTER3ATTACK] ATTACK-PROBE reason=MOVE sprite=1 subtype=4 mType=4 tile=538->506 weapon=13 alt=0 loops=3 goalStep=2/3 distance2=4096 adjacentCardinal=yes trace=clear hitLoops=2 firstRandHit=253 firstCalcHit=245 firstCritLimit=12 firstRandDamage=0 totalDamage=2 armorDamage=2 crit=0 rngCalls=6 missProjectileRng=1 playerHP=33->31 armor=23->21 ... rngRollback=yes playerExact=yes ...
[MONSTER3GOAL] ATTACK-PROBE ... frameTime=2->3 goal=2/3 ... shortcut=yes remainingGoals=skipped loops=3 ...
```

The activation filter delivers exactly that producer:

```text
[MONSTERACT] DELIVER actualProbe=1 deliveredProbe=1 sprite=1 reason=1 activated=yes
```

The visual owner then runs the full cadence:

```text
[MONSTERATKVIS] ARM ... loops=3 shot=1/3 phase=attack ...
[MONSTERATKVIS] STEP ... shot=1/3 phase=idle nextShot=2 ...
[MONSTERATKVIS] STEP ... shot=2/3 phase=attack ...
[MONSTERATKVIS] STEP ... shot=2/3 phase=idle nextShot=3 ...
[MONSTERATKVIS] STEP ... shot=3/3 phase=attack ...
[MONSTERATKVIS] STEP ... shot=3/3 phase=final-idle ...
[MONSTERATKVIS] COMPLETE ... loops=3 ...
```

Final resolution exactly matches the probe:

```text
[MONSTERRETAL] COMMIT probe=1 reason=MOVE sprite=1 subtype=4 mType=4 weapon=13 alt=0 loops=3 hitLoops=2 firstRandHit=253 firstCalcHit=245 firstCritLimit=12 firstRandDamage=0 totalDamage=2 armorDamage=2 crit=0 ... rngCalls=6 combatRngCalls=6 missProjectileRng=1 playerHP=33->31 armor=23->21 ... rng=39420bce->1a4b8634 ... rollback=closed
```

The physical red damage flash and top-bar `4 damage!` message also complete.

## Memory / invariants

Repeated ALIVE lines before and after the sequence remain:

```text
heap=116288 heap8=50364 largest8=38900
```

Session readiness keeps:

```text
shapeData=0x0
mediaTexels=0x0
```

## Next observed defect

The same real-CYD session exposes a separate presentation-only bug while
standing on a type-10 fire hazard and pressing PASS_TURN repeatedly:

```text
[HAZARDPASS] COMMIT ... hp=15->14 armor=4->2 ...
[HAZARDPASS] COMMIT ... hp=14->13 armor=2->0 ...
```

Damage feedback and red flash repeat correctly, but the bottom HUD digits do
not repaint until a later movement redraw. PlayerState itself is correct. The
next bounded fix should therefore reuse the existing wrapped current-player HUD
overlay and repaint only the HUD bands during hazard PASS_TURN, preserving the
existing rollback edge before MonsterTurn acceptance.
