# Native mixed MOVE event 74 / Yellow Key trap — real CYD PASS

This milestone records the bounded mixed MOVE family exposed by the Entrance
Yellow Key trap and the stale-owner freeze found during real gameplay.

## Git / hardware boundary

```text
main used for rebase = b77513309a38a970a5d59195ce76424a3f44a7cb
branch = agent/esp32-native-monster-turn-ordinary-completion
hardware-tested code boundary = aa32270adbb22de6666c3ad45c5d63c88fc34db4
CI = esp32-cyd #757 SUCCESS
static RAM = 45096 B
flash = 782081 B
status = REAL-CYD PASS
```

## Event 74 behavior

Entrance tile 697 / event 74 combines ordinary native script/state effects with
SHOW and line lock/open operations. The first successful traversal after the
Yellow Key pickup opens the trap: three doors expose the surrounding enemies.
The Yellow Key pickup itself remains owned by the generic pickup path.

The mixed executor is bounded, allocation-free and fail-closed outside its owned
command family. It does not restore a desktop Entity/Game world owner.

## Freeze root cause

After the trap had already fired, a later traversal resolved the same event as a
fully satisfied batch:

```text
[MOVEEVENT] MIXED-BATCH ... mutation=no rollback=0
```

The original mixed executor nevertheless kept its static owner active. Because no
mutation meant no rollback lease was needed, no later transaction-close path
released that owner. The next unrelated MOVE hit the fail-closed guard and
disabled resident gameplay.

The old diagnostic also printed `mixedOwnerActive=%u` without supplying the
corresponding vararg, so observed values such as `137` were not trustworthy
state.

## Permanent correction

At `aa32270adbb22de6666c3ad45c5d63c88fc34db4`:

- mixed/show rollback owners are armed only when `anyMutation != 0`;
- a completed no-op batch clears its temporary owners before return;
- real mutated batches retain the existing exact rollback lease;
- the BLOCK log now receives the actual mixed-owner value and distinguishes
  `stale-mixed-owner`.

No new heap owner or map-wide state was introduced.

## Real-CYD retest

The supplied hardware session moved away from tile 697 and continued through
multiple world/monster transactions:

```text
tile 697 -> 665 : committed
tile 665 -> 633 : committed
tile 633 -> 601 : committed
tile 601 -> 633 : committed
```

Combat against the released enemies also continued normally. There was no
recurrence of:

```text
[MOVEEVENT] BLOCK ... stale-mixed-owner
[RESIDENTGAMEPLAY] FAILED reason=move-commit
```

The tail remained alive at:

```text
heap=81788
heap8=16236
largest8=8692
```

The renderer also exercised its pre-existing compact-guard recovery during this
run and recovered successfully; that path is orthogonal to this milestone.
