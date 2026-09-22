# Milestone — Resident gameplay polish through Automap gate

## Scope

This milestone records the real-CYD validation reached on
`agent/esp32-native-display-touch-polish` before the first gameplay progression
gate that requires native Automap.

The hardware-tested code head is:

```text
d532ede998c23fcac7e73feefe66d5ce8c755ac9
```

Normal `esp32-cyd` CI run #522 / `35765245689` passed with:

```text
RAM:   44832 B / 327680 B
Flash: 728653 B / 1310720 B
```

The canonical static-RAM boundary therefore remains exactly 44,832 B.

## Real-CYD PASS — moved monster corpse projection

A monster that moved away from its immutable BSP spawn and then died now keeps
its corpse at the mutable native death position. The renderer no longer falls
back to immutable BSP x/y solely because death clears live/link topology bits.

The user explicitly confirmed that enemy corpses remain correctly positioned.

## Real-CYD PASS — EV_PASSWORD input and continuation

Entrance event 46 / tile 403 reaches native opcode 10 `EV_PASSWORD`.

The invalid-code path was observed end-to-end:

```text
[ACTION] SELECT ... status=PASSWORD_READY ... event=46
[PASSWORD] OPEN ... digits=4 ... continuation=dialog-pause resumeOpcode=26 resumeCmd=1
[PASSWORD] KEY ...
[PASSWORD] SUBMIT ... result=invalid continuation=blocked
[ACTIONFEEDBACK] PAINT ... text="Invalid code!"
[RESIDENTGAMEPLAY] PASSWORD-CLOSE ... result=invalid continuation=blocked
```

The user then found and entered the real code and confirmed that the intended
door unlocked. This validates the behavioral chain:

```text
EV_PASSWORD (10)
 -> native keypad
 -> correct-code continuation
 -> EV_DIALOGNOBACK (26) pause boundary
 -> subsequent native event continuation
 -> door unlock
```

The late full-HUD repaint added after the invalid-code visual leak is a
presentation fix on this branch, but no separate explicit invalid-code retest is
claimed in this milestone.

## Real-CYD PASS — monster movement cannot cross shoot-through bars

A ranged zombie previously committed:

```text
[MONSTERMOVELIVE] COMMIT ... tile=271->272 ...
```

through a barred wall that allows attacks through it but must remain movement
solid.

Legacy recovery showed that `Entity_aiGoal_MOVE()` correctly uses trace mask
`0xFF87`; the native mask was not the bug. The divergence was that legacy
`Game_trace()` walks the source cell and then the destination cell, while the
native movement planner had inspected only destination cells for linked
line/special-plane blockers.

Both native movement planners now trace the source side as well:

- generic monster movement;
- three-goal continuation movement.

The attack trace mask remains separate, so this correction does not turn the
bars into an opaque projectile wall.

The user retested the real game and confirmed the corrected behavior works on
the classic CYD.

## Previously validated display/door work retained

The same branch also retains the already documented hardware PASS for:

- full-width 160x120 animated intro with centered 156-pixel story text;
- pure two-line secret-door SELECT transaction on Entrance lines 471/470;
- V6 checkpoint-resume behavior inherited from main.

The later legacy-parity addition `Found Secret!` + 5 XP + deferred sound 5133
has not yet received its own real-CYD replay after being added. It remains a
candidate and is not claimed as hardware PASS here.

## Progression boundary reached

With the validated resident gameplay fixes in place, the user advanced through
Entrance until the game explicitly requires consulting the Automap.

Native Automap presentation/input is not yet implemented, so this is the next
intentional gameplay frontier rather than a failure of the validated milestone.

## Next bounded milestone — native Automap

Recover the exact legacy Automap behavior, then build the smallest permanent
native route around the existing compact `EspMapAutomapState`.

The first milestone should stay bounded to:

```text
existing compact reveal state
 -> native automap open/close input ownership
 -> 160x120 presentation using current immutable map geometry
 -> current-player marker/facing
 -> revealed/eligible line projection
 -> no world/monster turn while automap owns input
 -> GIVEMAP interaction only where required by recovered legacy semantics
```

Do not pull CHANGEMAP into the Automap milestone. Once the Automap progression
gate is passed on hardware, resume the dedicated Entrance level-exit /
CHANGEMAP validation separately.

## Result

**Resident gameplay polish through the Automap progression gate: REAL-CYD PASS.**

Static RAM remains 44,832 B and the next bounded gameplay milestone is native
Automap.
