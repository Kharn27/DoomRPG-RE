# Native monster turn / retaliation milestone

Status: **REAL-CYD HARDWARE PASS** for the bounded stationary monster-turn family.

Hardware-tested code boundary:

```text
branch = agent/esp32-native-monster-turn
base main = 0d46418a79f66592235fa88fab15b007ccb3a8b2
code boundary = e08b8a8bf7eca8b602c32a7559f142d44e3e9965
```

Commits after that boundary are documentation-only.

## Goal

Close the first enemy-turn seam without importing legacy Entity/AI ownership.
The milestone is deliberately bounded to already-positioned, already-aligned
monsters that can attack the player directly through the native topology/LOS
view.

The permanent split is:

```text
committed player action
 -> MonsterTurn schedule
 -> native candidate / cardinal LOS recovery
 -> exact legacy-compatible attack probe with rollback
 -> conservative activation delivery
 -> transactional nonlethal PlayerState retaliation commit
 -> native redraw
```

No legacy `Game.entities`, mutable desktop monster positions, `CombatEntity*`
pointers, or map-wide legacy renderer ownership are introduced.

## Owned family

The current native turn owner handles one unambiguous stationary attacker when:

- the monster is live `type=1` and already represented by `MonsterState`;
- player and monster positions are on the native 64-unit tile centers;
- they are cardinally aligned and within the recovered weapon range;
- native tile/line/sprite tracing confirms line of sight;
- the attack weapon is in the recovered compact monster weapon table;
- the result is nonlethal for the player.

The turn is scheduled after committed MOVE, ROTATE, or PLAYER_ATTACK observations.
The existing monster `subtype` / `alternateAttack` data selects the same generic
backend; there is no dog-specific or zombie-specific retaliation executor.

Still fail-closed:

```text
multiple ambiguous attackers / activation ordering
monster movement / pathfinding / mutable positions
special subtype-10 AI
player lethal/death transition
dog-familiar damage redirection
attack animation / pain FX / damage text / sound
full legacy turn advancement orchestration
```

## Shared player mutation

Retaliation writes only the existing 52 B `EspNativeGameplayPlayerState` owner.
HP and armor are changed transactionally. If render fails or touches gameplay
RNG unexpectedly, PlayerState and RNG are restored and a recovery redraw is
attempted.

A miss commits gameplay RNG but does not mutate PlayerState. Lethal results roll
back and log `LETHAL-DEFER` until the player-death state machine has a dedicated
milestone.

## RNG boundary bug recovered during hardware test

The first live implementation snapshotted only `DoomRPG.random` (`Random_t`).
That was insufficient at the 128-byte table refill boundary.

Legacy `DoomRPG_setRand()` regenerates `Random_t.randTable` through hidden
file-static generator state (`resetRand` plus `_seed`). A speculative probe that
crossed the end of the table could therefore:

```text
probe: refill hidden generator -> consume bytes -> restore Random_t
live : refill hidden generator again -> receive a different table
```

The real CYD exposed this as a probe MISS starting with `firstRandHit=242`, then
a live replay starting from the probe's next byte (`9`) and turning into a CRIT.
`rngRollback=yes` had been true only for `Random_t`, not for the complete legacy
refill state.

### Permanent bounded repair

`esp_native_rng_replay_guard.c` wraps `DoomRPG_randNextByte()`.

At a table refill it retains the exact pre/post `Random_t` pair. If an immediate
transaction rollback restores the exact pre-refill state, the matching replay
reuses the already-generated post-refill table rather than advancing the hidden
generator a second time.

```text
owner = 280 B static bounded state
allocation = none
lease = 1000 ms
normal non-refill byte sequence = unchanged
hidden generator advance = once per logical refill
```

The guard is generic transaction infrastructure, not monster-specific logic.

## Final Hellhound hardware witness

The decisive boundary-crossing test on the real classic CYD:

```text
[MONSTERTURN] SCHEDULE n=37 reason=MOVE ...
[RNGGUARD] REFILL refill=1 leaseMs=1000 next=127->0 hiddenGenerator=advanced-once rollbackReplay=armed
[MONSTERTURN] ATTACK-PROBE ... sprite=179 subtype=1 weapon=12 ... firstRandHit=63 firstRandDamage=1 totalDamage=1 armorDamage=1 ... rngRollback=yes playerExact=yes
[MONSTERACT] DELIVER actualProbe=1 deliveredProbe=1 sprite=179 reason=1 activated=yes
[RNGGUARD] REPLAY refill=1 replay=1 leaseMs=1000 next=127->0 hiddenGenerator=untouched rollbackSafe=yes
[MONSTERRETAL] COMMIT ... sprite=179 ... firstRandHit=63 firstRandDamage=1 totalDamage=1 armorDamage=1 ... playerHP=30->29 armor=12->11 ... rollback=closed
```

The probe and live transaction are byte-for-byte identical across the refill
boundary.

A subsequent ordinary non-refill dog turn also matched exactly:

```text
probe/live firstRandHit = 149
probe/live firstRandDamage = 75
probe/live totalDamage = 1
probe/live armorDamage = 1
player = HP 29->28, armor 11->10
```

## Final Zombie hardware witness

Zombie subtype 0 exercised the same generic family with its ranged/AI decision
byte.

Normal hit:

```text
aiRand = 119
firstRandHit = 96
firstRandDamage = 45
totalDamage = 3
armorDamage = 2
player = HP 30->27, armor 14->12
probe/live = exact
```

Critical hit:

```text
aiRand = 46
firstRandHit = 11
firstRandDamage = 95
crit = 1
totalDamage = 7
armorDamage = 5
player = HP 27->20, armor 12->7
probe/live = exact
```

This confirms both a melee Hellhound and a ranged Zombie use the same generic
turn/retaliation path.

## Final RAM witness

Real-CYD normal `esp32-cyd` environment after the RNG replay guard:

```text
heap = 85000 B
heap8 = 19236 B
largest8 = 13812 B
PSRAM = none
shapeData = NULL
mediaTexels = NULL
```

The selected resident graphics-cache geometry remains unchanged.

## Merge boundary

`e08b8a8bf7eca8b602c32a7559f142d44e3e9965` is the last code commit exercised
by the real hardware for this milestone. The branch is now locked to docs-only
changes until merge.

After merge, re-read the real GitHub `main` SHA before creating the next
`agent/*` branch.
