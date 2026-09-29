# Native checkpoint monster spatial V9 / V8 migration

## Branch and hardware boundary

```text
branch = agent/esp32-native-checkpoint-monster-spatial-v9
hardware-tested code boundary = 7b3efeb8d590c027b94f08ac7c31c886938709d4
esp32-cyd CI #980/#981 = SUCCESS
```

The branch contains the complete prior
`agent/esp32-native-junction-exit-census` history through
`49ad2fe9d63f454611f6d41a729e62905f456251`.

## REAL-CYD PASS: existing V8 checkpoint recovery

The real classic CYD resumed an existing V8 checkpoint created after the yellow
key/card trigger had revealed hidden monsters. V8 persisted script state and
logical `MonsterState`, but did not persist mutable monster topology or
positions. The trigger therefore stayed consumed after LOAD while its revealed
monsters returned to initial hidden/unlinked BSP topology.

The compatibility path now reconstructs only one-shot topology effects for
which V8 carries durable execution evidence:

```text
removed-command bit == 1
AND command has REMOVE-if-handled
AND opcode is SHOW or HIDE
```

Historical monster movement is deliberately not guessed.

Hardware result:

```text
checkpoint LOAD reaches READY / 100%
monsters revealed by the yellow-card trigger are present after LOAD
normal gameplay resumes
```

The earlier LOAD abort at `checkpoint monster position view` was also fixed by
publishing the live `MonsterPosition.records` pointer through the view.

## V9 spatial checkpoint boundary

The branch adds a bounded V9 record containing:

```text
MonsterState
monster topology: linked/unlinked, tile, link order, visual/alive bits
MonsterPosition
MonsterActivation order
```

Cross-owner identity/fingerprint mismatches fail closed. No pointer-heavy legacy
entity graph is serialized and no `shapeData` / `mediaTexels` owner is
introduced.

A real-CYD SAVE-V9 attempt exposed one cross-owner validation bug: logical
monster death is authoritative in `MonsterState`, while the raw compact
topology may still retain ALIVE/LINKED bits that are masked by the combat
overlay. Requiring raw topology ALIVE to equal logical alive therefore rejected
valid current state. The final code accepts the intentional logical-dead overlay
while still rejecting the inverse inconsistency (logically alive with raw
topology ALIVE cleared), and adds stage-specific capture diagnostics.

Hardware result at the final code boundary:

```text
SAVE V9 succeeds on the real classic CYD
previous V8 checkpoint remains readable
yellow-card revealed monsters remain present after V8 LOAD
```

## REAL-CYD PASS: final V9 enemy + destructible spatial round-trip

Code review identified two remaining V9 ownership holes after the first SAVE
PASS:

1. an EV_SHOW blocker enemy can be topology-dead while MonsterState still says
   alive because gameplay death side effects are deliberately deferred;
2. EV_SHOW can also remove a deterministic destructible, while the first V9
   topology snapshot contained enemies only.

The final bounded correction keeps the live gameplay owners unchanged while
making the checkpoint image coherent:

- a topology-dead enemy is reconciled to logical-dead in the snapshot copy
  before the MonsterState checkpoint FNV is calculated;
- the V9 topology snapshot contains both enemies and destructibles;
- the provisional enemy-only V9 produced by earlier branch commits remains
  readable through the existing consumed SHOW/HIDE replay compatibility path.

Entrance hardware witness on the real classic CYD:

```text
SAVE:
version=9
bytes=5444
monsters=30
topology=43
positions=30
activation=0
world=monster-spatial-exact-v9

LOAD:
[MAPCHECKPOINTTOPO] RESTORE
  tracked=43
  enemies=30
  destructibles=13
  scope=enemy+destructible-v9
  stateFNV=7a4b0217

[NATIVESAVE] V9-SPATIAL-STAGE
  monsters=30
  topologyTracked=43
  monsterFNV=dcda5880
  topologyFNV=7a4b0217
  positionFNV=a369df86
  activationFNV=a91415b7
  legacyReplay=0/0/0
  exact=yes

[ENGINESESSION] READY
  shapeData=0x0
  mediaTexels=0x0
```

The user performed a new V9 SAVE, then a V9 LOAD from that exact file. The
checkpoint restored the full 43-record enemy+destructible topology and the
30-record monster position owner, reached READY, released the loading
presentation, and resumed resident gameplay. This closes the two review findings
and hardware-validates the final V9 spatial checkpoint round-trip at
`7b3efeb8d590c027b94f08ac7c31c886938709d4`.

V8 migration, V9 SAVE, and V9 LOAD are now real-CYD PASS at the bounded scope
above.
