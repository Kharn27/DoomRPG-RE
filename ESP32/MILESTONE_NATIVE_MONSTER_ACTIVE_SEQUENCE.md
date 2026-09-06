# Milestone — native active-monster sequence on real CYD

This record locks the multi-active-monster ordering and regression boundary reached
on `agent/esp32-native-monster-three-goal-turn`.

Repository state wins over chat history. Serial logs from the real classic CYD are
the final runtime authority.

## Git / build boundary

```text
main at branch creation = e6aae5d3a0d3c6564a3f4147b3be71c422dda5d3
branch = agent/esp32-native-monster-three-goal-turn
hardware-tested code boundary = 8c3bee5ebba013fead9273f10f1a1c3bd015eeda
status = REAL-CYD MULTI-ACTIVE MONSTER SEQUENCE + DOOR/ATTACK REGRESSION PASS
branch policy = LOCKED; documentation-only tail after this SHA
```

GitHub Actions normal `esp32-cyd` run `34047884638` / run #149 completed with
`success` on the exact tested SHA. The workflow built the classic CYD firmware
and produced the normal firmware artifact.

CI is compile/link evidence only. The hardware session below is the behavioral
witness.

## Permanent behavior now hardware-owned

The native renderer admission point now mirrors the legacy activation boundary:
an admitted, non-hidden, linked/alive enemy becomes active when its BSP-visible
sprite reaches rendering. Activation persists for the map session and records
first-activation order only; activation itself does not consume gameplay RNG.

A no-immediate-attack MonsterTurn is expanded over the active list in that order.
Each member reuses the existing proven movement planner and live publication
transaction. The important ordering contract is:

```text
member N plan
 -> member N RNG commit
 -> member N MonsterPosition commit
 -> member N topology relink
 -> member N renderer publication
 -> only then plan member N+1
```

This preserves the legacy active-list sequencing model instead of planning every
monster from one stale world snapshot.

The current bounded owner still deliberately excludes different families:

```text
special subtype-10 AI = deferred
simultaneous attack-ready multi-monster ordering = deferred
ranged >=217 expansion remains on previous single-candidate boundary
movement interpolation = deferred
multi-loop monster attack presentation = deferred
```

## Real-CYD two-Hellhound witness

The final supplied hardware session on `8c3bee5e...` physically exercised two
active subtype-1 Hellhounds, sprites `89` and `114`.

Both are activated by renderer visibility without touching gameplay RNG:

```text
[MONSTERACT] ACTIVE sprite=89 subtype=1 ... activationOrder=1 ... gameplayRng=untouched
[MONSTERACT] ACTIVE sprite=114 subtype=1 ... activationOrder=2 ... gameplayRng=untouched
```

On one PASS_TURN, sprite 89 commits first and sprite 114 plans from the already
updated world state, then commits before the turn closes:

```text
[MONSTERMOVELIVE] COMMIT ... sprite=89 tile=263->264 ... rngCalls=1 randomCommitted=yes
[MONSTERACTIVESEQ] MEMBER ... sprite=89 ... publication=committed-before-next

[MONSTERMOVELIVE] COMMIT ... sprite=114 tile=233->265 ... rngCalls=1 randomCommitted=yes
[MONSTERACTIVESEQ] MEMBER ... sprite=114 ... publication=committed-before-next

[MONSTERACTIVESEQ] COMPLETE ... activeCount=3 delivered=2 ... ordered=yes publication=per-member
```

The next PASS_TURN repeats the same sequential publication contract from the new
positions. This confirms that the second dog does not plan against the stale
pre-turn topology/MonsterPosition state.

## Death removes one member from subsequent delivery

The player then attacks sprite 89. The existing generic type-1 combat backend
commits its death:

```text
[MONSTERCOMBAT] COMMIT ... sprite=89 ... hp=6->0 ... alive=1->0 ... rollback=closed
```

The same player-attack turn subsequently delivers movement only for sprite 114:

```text
[MONSTERMOVELIVE] COMMIT ... sprite=114 tile=233->232 ... randomCommitted=yes
[MONSTERACTIVESEQ] MEMBER ... sprite=114 ... publication=committed-before-next
[MONSTERACTIVESEQ] COMPLETE ... delivered=1 ... ordered=yes
```

Later PASS_TURNs continue moving 114 alone. This is the required mutable-world
interaction: the active sequence does not keep delivering a dead monster merely
because it was activated earlier in the map session.

## Door + attack passthrough regression closure

The final code fix restored two behaviors that had regressed while composing the
multi-monster wrappers.

A regular door still performs the established four-frame moving-line transaction:

```text
[DOORANIM] ARM ... frames=4 moving=3 ...
[DOORANIM] FRAME 1/4 ... geometry=moving ...
[DOORANIM] FRAME 2/4 ... geometry=moving ...
[DOORANIM] FRAME 3/4 ... geometry=moving ...
[DOORANIM] FRAME 4/4 ... geometry=stable ...
[DOORANIM] COMPLETE transitions=1 frames=4 state=stable transaction=committed
```

After the door is open, SELECT still passes through to enemy combat rather than
being swallowed by the dynamic-line composition layer. Sprite 89 is selected,
its attack animation renders, and the generic combat transaction commits.

This regression closure is part of the exact tested SHA and is therefore included
in the locked hardware boundary.

## RNG / mutable-state evidence

The multi-monster sequence keeps live RNG transactional. Each successful movement
probe first rolls back its temporary position mutation exactly, then the live
publisher consumes the reserved tie byte only on commit.

Representative hardware evidence:

```text
sprite=89 ... positionRollback=yes ... randomLiveUntouched=yes
[MONSTERMOVELIVE] COMMIT ... rngCalls=1 randomCommitted=yes

sprite=114 ... positionRollback=yes ... randomLiveUntouched=yes
[MONSTERMOVELIVE] COMMIT ... rngCalls=1 randomCommitted=yes
```

The publication order is therefore also the RNG order.

## Three-goal owner on this branch

This branch also contains the bounded subtype `4/13` three-goal owner recovered
from the legacy `Entity_aiMoveToGoal()` `i=3` path. Continuation goals target the
settled player destination, consume one visit-choice byte per successful goal,
and reuse the existing movement publication transaction. The code continues to
fail closed on unsupported special calcPath-plane crossings and on the separate
three-shot/multi-loop attack family.

The final regression session recorded in this file exercises the subtype-1
multi-active sequence, door animation and combat passthrough. It does not by
itself add a new subtype-4/13 hardware witness; do not infer one from the binary
having been flashed.

## RAM / invariants

The supplied session remained stable after repeated door frames, multi-monster
movement, combat, death publication and later PASS_TURNs:

```text
heap = 86252
heap8 = 20520
largest8 = 18420
shapeData = NULL
mediaTexels = NULL
```

No PSRAM is used. Audio remains deferred.

## Locked result

`8c3bee5e...` is the exact code boundary exercised on the real classic CYD.
Commits after it on this branch must be documentation-only. Before merge, compare
the tested SHA to branch head and verify that only Markdown recovery/milestone
files changed.

After the user announces the merge, read the true `main` SHA again and create the
next `agent/*` from that exact commit. Do not continue code on this locked branch.
