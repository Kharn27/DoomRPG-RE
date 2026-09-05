# Native monster attack visual — real CYD PASS

This milestone adds a bounded, presentation-only monster attack pose owner on the
classic ESP32-2432S028R path.

## Git / hardware boundary

```text
branch = agent/esp32-native-monster-attack-visual
base main = 19c3fd3b6ebd66530e92d24a43ec8f09a9c4f803
hardware-tested code boundary = a1df0a5ac031b4a0dad6f4da609dd25c6c450007
status = REAL-CYD SINGLE-LOOP MONSTER ATTACK VISUAL PASS
branch policy = LOCKED; docs-only tail only
```

The exact tested code boundary is `a1df0a5a...`. Commits after it must remain
documentation-only until merge.

Normal GitHub Actions `esp32-cyd` run `33958515531` / run #110 completed
successfully on that SHA. Job `101286108924` (`PlatformIO esp32-cyd`) built the
classic CYD firmware and uploaded artifacts successfully.

CI is compile/link evidence only. The serial logs below are from the real classic
CYD and remain the runtime authority.

## Legacy contract recovered

`Combat_performAttack()` selects the monster attack visual frame from the same
alternate-attack bit used by the weapon table:

```text
alternateAttack == 0 -> attackFrame = 1
alternateAttack != 0 -> attackFrame = 5
```

`Combat_monsterSeq()` holds the active monster attack frame for 150 ms.

This milestone owns only the bounded `NUMSHOTS == 1` presentation family.
Three-shot / multi-loop presentation remains fail-closed.

## Native contract

`EspNativeGameplayMonsterAttackVisual` observes the already-proven
`EspNativeGameplayMonsterTurn` attack probe and owns only a temporary visual
lease:

```text
primary single-loop   -> visual 1
alternate single-loop -> visual 5
lease                 -> 150 ms after successful physical presentation
expiry                -> redraw idle
```

The owner:

- does not mutate `EspNativeGameplayMonsterState`;
- does not mutate BSP sprite data;
- does not mutate topology or monster position;
- does not own or recalculate retaliation damage;
- snapshots/guards gameplay RNG around render calls;
- retries expiry rather than restoring a stale framebuffer under touch feedback;
- promotes only the currently leased attack sprite to `SPRITE_FIXED_ANIM`;
- leaves pain/death/corpse presentation precedence unchanged.

Projectiles, attack text, player-pain animation, shake and sound remain deferred.

## Real-CYD primary-frame witness

Entrance Hellhound `sprite=179`, subtype 1, primary attack:

```text
[MONSTERTURN] ATTACK-PROBE reason=PASS_TURN sprite=179 subtype=1 mType=1 tile=686 weapon=12 alt=0 loops=1 ... rngRollback=yes playerExact=yes ... mutation=no
[MONSTERACT] DELIVER actualProbe=1 deliveredProbe=1 sprite=179 reason=4 activated=yes
[MONSTERATKVIS] ARM probe=1 reason=PASS_TURN sprite=179 subtype=1 alt=0 loops=1 visual=1 fixedAnim=yes leaseMs=150 ... presented=1 rngExact=yes immutableSprite=yes retaliation=continues ... gameplayMutation=no
[MONSTERRETAL] COMMIT probe=1 ... playerHP=30->29 armor=8->7 ... rollback=closed ...
[MONSTERATKVIS] EXPIRE probe=1 sprite=179 visual=1->idle leaseMs=150 ... presented=1 rngExact=yes gameplayMutation=no
```

A second consecutive Hellhound attack repeated the same `ARM -> retaliation ->
EXPIRE` sequence without rollback or expiry retry.

## Real-CYD alternate-frame witness

Entrance Hellhound `sprite=114`, subtype 1, alternate attack:

```text
[MONSTERACT] ACTIVE sprite=114 subtype=1 tile=233 distance=2 source=forward-visible activeCount=3 persistence=map-session ...
[MONSTERMOVELIVE] COMMIT ... sprite=114 tile=233->265 pos=608,480->608,544 ... rollback=closed
[MONSTERTURN] ATTACK-PROBE reason=PASS_TURN sprite=114 subtype=1 mType=1 tile=265 weapon=13 alt=1 loops=1 ... rngRollback=yes playerExact=yes ... mutation=no
[MONSTERACT] DELIVER actualProbe=6 deliveredProbe=6 sprite=114 reason=4 activated=yes
[MONSTERATKVIS] ARM probe=6 reason=PASS_TURN sprite=114 subtype=1 alt=1 loops=1 visual=5 fixedAnim=yes leaseMs=150 ... presented=1 rngExact=yes immutableSprite=yes retaliation=continues ... gameplayMutation=no
[MONSTERRETAL] COMMIT probe=6 ... playerHP=19->17 armor=9->7 ... rollback=closed ...
[MONSTERATKVIS] EXPIRE probe=6 sprite=114 visual=5->idle leaseMs=150 ... presented=1 rngExact=yes gameplayMutation=no
```

The user also visually confirmed that the monster now visibly attacks rather than
only applying retaliation damage.

## Additional real-CYD witness

Zombie `sprite=106`, subtype 0, primary pistol attack also exercised frame 1:

```text
[MONSTERTURN] ATTACK-PROBE ... sprite=106 subtype=0 weapon=2 alt=0 loops=1 ...
[MONSTERATKVIS] ARM ... sprite=106 subtype=0 alt=0 loops=1 visual=1 ... presented=1 rngExact=yes ...
[MONSTERRETAL] COMMIT ... rollback=closed ...
[MONSTERATKVIS] EXPIRE ... sprite=106 visual=1->idle ... rngExact=yes gameplayMutation=no
```

## Memory witness

Repeated movement, combat, pickup feedback and both attack-frame families remained
stable on the real board at:

```text
heap = 86524
heap8 = 20792
largest8 = 18420
shapeData = NULL
mediaTexels = NULL
```

No PSRAM is present.

## Newly exposed next boundary

The hardware test exposed a separate AI sequencing gap that is intentionally not
folded into this presentation milestone.

Legacy `Entity_aiMoveToGoal()` performs a post-interpolation goal step. For
subtypes `1`, `4`, `5` and `13`, when the monster finishes its movement adjacent
and cardinally aligned to the player with clear trace, it may call
`Entity_attack()` in the same monster turn.

The native movement owner currently publishes the destination immediately and
ends the movement transaction without this post-move goal/attack step. Therefore
a Hellhound can move adjacent and require a later player turn before attacking.

This is the preferred next bounded milestone:

```text
legacy post-move goal semantics
 -> exact supported subtype gate
 -> destination-adjacent/cardinal + trace check
 -> same-turn attack probe after committed move
 -> existing attack-visual + retaliation owners consume that probe
 -> no duplicated damage or visual semantics
```

A ranged Zombie witness also confirmed an important negative: when legacy ranged
AI chooses the movement branch (for example `aiRand >= 217`) and the move
succeeds, it is valid for that turn to move without attacking. The next milestone
must therefore recover the exact post-move subtype semantics rather than blindly
making every moved monster attack.

## Deferred after this milestone

```text
three-shot / multi-loop monster attack presentation
projectile visuals
monster attack message text
monster attack sound playback
player pain face / shake / sound
player lethal/death transition
monster movement interpolation animation
multiple-live-monster ordering
special subtype-10 AI
```
