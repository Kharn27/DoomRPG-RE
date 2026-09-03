# Native monster movement planner milestone

Status: **REAL-CYD HARDWARE PASS** for the bounded native monster spatial owner,
legacy-compatible movement planner probe, and persistent RNG refill reservation.

Hardware-tested code boundary:

```text
branch = agent/esp32-native-monster-movement
base main = 65dea748ef8d2e5a6f3823676ac05ee62fb89407
code boundary = 7dd7faa3994969e43ccb27b5450ee828ade3c323
```

Commits after that boundary are documentation-only.

## Goal

Recover the first permanent native monster-position/movement layer without yet
publishing live enemy movement into renderer or topology state.

The bounded split is:

```text
MonsterState + topology
 -> compact mutable MonsterPosition owner
 -> recovered legacy movement trigger / path planner
 -> temporary cardinal position commit
 -> exact rollback
 -> renderer/topology publication still deferred
```

This milestone deliberately proves ownership and behavior before enabling visible
monster movement.

## Permanent monster spatial owner

Each native enemy now has one compact mutable position record:

```text
EspNativeGameplayMonsterPositionRecord
  spriteIndex : uint16
  tileIndex   : uint16
  worldX      : uint16
  worldY      : uint16
record size   : 8 B
```

Entrance has 30 enemies, therefore:

```text
records = 30
payload = 240 B
allocation = one 8-bit-capable allocation at map/session initialization
```

Initial positions are seeded from the already-owned native topology tile and
converted to the canonical 64-unit tile center. Immutable BSP sprite x/y are not
treated as live runtime position.

The position API is transactional. A probe can prepare one exact cardinal
`+/-64` move, commit it into the position owner, fingerprint it, then rollback to
the exact previous record/fingerprint.

Renderer publication and topology relinking remain intentionally deferred.

## Recovered legacy movement semantics

The planner recovers the relevant `Entity_aiThink` / `Entity_aiGoal_MOVE`
behavior instead of inventing an ESP32-specific pathfinder.

Owned now:

```text
attack LOS mask = 0x5687
movement mask   = 0xff87
subtype 4/13    => clear 0x0c00
subtype 6/7     => clear 0x0800
subtype 10      => clear 0x0400 (special AI itself still deferred)
cardinal tests  = east, west, south, north
calcPath        = bounded 2-step greedy squared-distance look-ahead
visit choice    = visitOrder[(rand & 3) % visitCount]
directions      = 0 north, 1 south, 2 east, 3 west
```

The legacy north quirk is preserved: when north becomes the new best candidate,
visit order is replaced but `closestPathDist` is not updated.

The service accepts only one unambiguous alive linked active ordinary enemy. If
multiple eligible active candidates exist, activation order is not guessed and
the movement path fails closed.

Two producer paths are distinguished:

```text
NO-IMMEDIATE-ATTACK
RANGED-AI
```

The first does not consume the ranged AI decision byte. The second replays the
legacy `rand >= 217` movement decision when the monster was attack-ready but the
turn producer deferred movement.

## Real-CYD movement witness

Hellhound sprite 179 exercised the no-immediate-attack path:

```text
[MONSTERMOVE] PROBE trigger=NO-IMMEDIATE-ATTACK n=36 reason=1
  sprite=179 subtype=1 weapon=12
  sourceTile=750 source=928,1504
  target=928,1440
  destTile=718 delta=0,-64
  immediateAttack=no
  visitCount=1 tieRand=217 choice=0 mask=ff87 rngCalls=1
  randomLiveUntouched=yes
  positionFNV=61296c4a->10cf73aa->61296c4a
  positionRollback=yes
  rendererPublish=deferred topologyRelink=deferred
  liveMove=no mutation=no
```

This proves that the permanent spatial owner is writable and exactly restorable
on the real classic CYD while no visible/live movement is yet published.

## RNG table-boundary reservation

The movement planner initially reached `nextRand==127` and correctly failed
closed because its local probe must not regenerate the hidden legacy RNG table
and then abandon that hidden generator advance.

The generic RNG replay guard therefore gained a persistent exact reservation:

```text
pre-refill Random_t
post-refill Random_t
exact live Random_t pointer
reservation survives beyond the ordinary 1000 ms rollback lease
```

At a movement probe boundary:

1. the hidden generator advances exactly once to materialize the post-refill
   table, unless an exact cached table already exists;
2. the movement probe temporarily borrows that post-refill `Random_t`;
3. the movement service copies it into its own local probe RNG and does not
   consume the live state;
4. live `Random_t` is restored byte-for-byte to the pre-refill state;
5. the reserved post-refill table persists until the next real byte-boundary
   draw on that exact live RNG owner.

The guard remains bounded static state with no allocation. With two 132 B
`Random_t` snapshots plus the pointer/counters/flags, the current guard owner is
284 B on the 32-bit ESP32 target.

## Boundary bug found and repaired during hardware test

A first reservation implementation consumed the persistent reservation when the
monster-turn **probe** performed the first real boundary draw. That was still one
transaction too early: the turn probe then rolled `Random_t` back, causing live
retaliation to generate a second hidden table and receive different bytes.

The real CYD exposed the mismatch directly:

```text
probe firstRandHit=87 firstRandDamage=101
live  firstRandHit=124 firstRandDamage=130
```

The final repair converts `PROBE-REPLAY` into the ordinary 1000 ms rollback lease
instead of discarding the cached pre/post pair. The live retaliation can then
replay the exact same refill after the turn probe rollback.

## Decisive final RNG witness

Final real-CYD boundary sequence on code boundary `7dd7faa...`:

```text
[RNGGUARD] PROBE-REFILL refill=1 next=127->0
  hiddenGenerator=advanced-once reservation=until-live-replay

[MONSTERMOVE] PROBE ... tieRand=217 ...
  randomLiveUntouched=yes
  positionFNV=61296c4a->10cf73aa->61296c4a
  positionRollback=yes

[RNGGUARD] PROBE-RESTORE refill=1
  liveRandomExact=yes reservation=pending

[RNGGUARD] PROBE-REPLAY refill=1 replay=1 leaseMs=1000 next=127->0
  hiddenGenerator=untouched reservation=consumed
  rollbackReplay=armed sequenceExact=yes

[MONSTERTURN] ATTACK-PROBE ... sprite=179 weapon=12
  firstRandHit=217 firstCalcHit=206
  hitLoops=0 missProjectileRng=1 rngCalls=2
  rngRollback=yes playerExact=yes

[RNGGUARD] REPLAY refill=1 replay=2 leaseMs=1000 next=127->0
  hiddenGenerator=untouched rollbackSafe=yes

[MONSTERRETAL] MISS-COMMIT ... sprite=179 weapon=12
  firstRandHit=217 firstCalcHit=206
  rngCalls=2 missProjectileRng=1
  gameplayRngCommitted=yes playerMutation=no
```

The same logical refill is generated only once. Movement borrows it, the turn
probe consumes/reverts it, and live retaliation consumes the same table without
a second hidden-generator advance.

A subsequent ordinary player-attack retaliation in the same session also matched
probe/live exactly:

```text
firstRandHit = 203
firstRandDamage = 213
totalDamage = 2
armorDamage = 2
player HP 30 -> 28
armor 12 -> 10
```

This confirms the boundary repair does not disturb subsequent normal RNG
sequencing.

## Final RAM witness

Real-CYD normal `esp32-cyd` environment at the locked code boundary:

```text
heap = 84648 B
heap8 = 18884 B
largest8 = 13812 B
PSRAM = none
shapeData = NULL
mediaTexels = NULL
```

The selected resident graphics-cache geometry remains unchanged.

## Still deferred

This milestone does **not** enable visible/live monster movement.

Still fail-closed or deferred:

```text
publishing monster position into renderer/topology
actual monster interpolation/animation
multiple-monster movement ordering
special subtype-10 AI
unsupported special calcPath plane corpus
legacy fallback attack when no movement path exists
player lethal/death transition
dog-familiar redirection
monster attack / player pain presentation
sound / combat text feedback
```

## Merge boundary

`7dd7faa3994969e43ccb27b5450ee828ade3c323` is the last code commit exercised
by the real hardware for this milestone. The branch is locked to documentation-
only changes until merge.

After merge, re-read the real GitHub `main` SHA before creating the next
`agent/*` branch.
