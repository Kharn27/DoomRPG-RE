# Native checkpoint monster spatial V9 / V8 migration

## Branch and hardware boundary

```text
branch = agent/esp32-native-checkpoint-monster-spatial-v9
hardware-tested code boundary = cb9f7b3f97314524b01d52c45d9a209b7c5bcb87
esp32-cyd CI #954 = SUCCESS
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

The V8 migration above is hardware-proven. An exact SAVE-V9 -> mutate -> LOAD-V9
round-trip is not yet hardware-proven and must not be described as validated
until that specific test passes.
