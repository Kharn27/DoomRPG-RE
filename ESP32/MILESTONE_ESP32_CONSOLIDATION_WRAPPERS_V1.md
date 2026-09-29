# Milestone — ESP32 structural consolidation: wrapper cleanup v1

Date: 2026-09-29

## Boundary

```text
main = 3de74fc1899ea619874b9f2bce8fb3679016c1a4
branch = agent/esp32-consolidation-dead-wrap-cleanup
hardware-tested code head = 4731d8265e90da19dc6d911739c4bf5574117a4d
esp32-cyd CI #1015 attempt 2 = SUCCESS
static RAM = 45784 B
flash = 816517 B
artifact id = 11039313210
```

## Consolidation method

The previous branch briefly concatenated eight post-load state files into one
large translation unit while keeping nearly all separate APIs and ownership.
The observed stack failure reproduced after that merge was reverted and was
fixed independently by moving indexed-BMP scratch off loopTask stack. The new
rule is therefore architectural rather than mechanical:

```text
remove obsolete composition
 -> replace native->native linker interception with explicit APIs
 -> group files only where ownership genuinely becomes shared
 -> reduce legacy implementation units family by family
```

## Audit

At the merged base:

```text
ESP32 C/C++ translation units = 175
legacy src/*.c units compiled = 17
active linker --wrap entries = 65
native Esp* --wrap entries = 50
```

The compiled legacy units are Combat, CombatEntity, DoomCanvas, DoomRPG,
Entity, EntityDef, EntityMonster, Game, Hud, Menu, MenuItem, MenuSystem,
ParticleSystem, Player, Render, Weapon and Z_Zip. DoomCanvas/DoomRPG are built
from generated ESP32-patched copies.

## First cleanup

`esp_native_gameplay_monster_movement.c` still contained dormant
`__wrap_EspNativeGameplayMonsterRetaliation_service/reset` functions although
the production link no longer used the corresponding `--wrap` flags.
`esp_native_gameplay_monster_movement_wrap_compat.c` existed only to provide
their historical `__real_*` symbols.

Commit `4731d8265e90da19dc6d911739c4bf5574117a4d` removes that footer, its
unused include and the compatibility translation unit. No active runtime wrap
is changed. The ESP32 source count becomes 174.

CI RAM and flash are identical to the merged base, supporting the expectation
that these sections were already dead in the final linked image.

## Real-CYD proof

The user reports normal behavior. The supplied Sector 1 trace includes:

```text
[MONSTERCOMBAT] COMMIT ... sprite=218 ... hp=2->0 ...
[MONSTERTURN] SCHEDULE ... reason=PLAYER_ATTACK ...
[MONSTERACTIVESEQ] COMPLETE ...
[RESIDENTGAMEPLAY] MOVE ... tile=508->507 ... committed=yes
[MONSTERTURN] SCHEDULE ... reason=MOVE ...
[MONSTERACTIVESEQ] COMPLETE ...
[ALIVE] uptime=99208 ms ...
```

Rendering, weapon presentation, hit FX, action feedback and facing-label updates
also continue normally. This cleanup is therefore hardware validated.

## Next step on this same branch

The next bounded candidate removes the active native linker wrapper around
`EspNativeGameplayMonsterPosition_prepareCardinalMove`. Its semantics are
activation gating plus capture of the prepared position pair for publication.
Those semantics should move behind an explicit movement-activation API while the
lower-level MonsterPosition owner remains independent.

That next code must preserve RNG cadence, commit/rollback, topology publication
and activation ordering, and requires a separate real-CYD test before being
called hardware validated.


## Active-wrap replacement: hardware finding and correction

Candidate `3d15ba1393da6883f0af2903699d687c2e1e64fc` removed the active linker
`--wrap=EspNativeGameplayMonsterPosition_prepareCardinalMove` and replaced the
ordinary movement planner call with the explicit
`EspNativeGameplayMonsterMovementActivation_prepareCardinalMove()` boundary.

The real CYD proved that ordinary multi-active movement still worked: sprites
218 and 237 each passed `MONSTERMOVEACT ALLOW`, committed live movement, and
the active sequence completed in order. After SHOW event 51 exposed two subtype
4 monsters, all four active members were serviced in order.

The same trace also exposed a missed dependency on the historical linker wrap.
Subtype-4 first goals committed, but their goal-2 continuation emitted:

```text
[MONSTER3GOAL] PLAN ... publish=pending
[MONSTERMOVELIVE] DEFER cause=probe-sequence-or-capture-mismatch
[MONSTER3GOAL] DEFER ... cause=continuation-not-published
```

This is a hardware regression, so `3d15ba...` is NOT a hardware-pass boundary.

Root cause: `esp_native_gameplay_monster_three_goal_turn.c` had its own direct
call to `EspNativeGameplayMonsterPosition_prepareCardinalMove()`. The removed
linker wrapper had implicitly supplied activation gating + publication capture
for that call too. Ordinary movement was converted to the explicit boundary,
but the three-goal continuation was initially missed.

Commit `aa7cb5c778264e1bb61d442d1c9864c09e6f37a3` routes that continuation
through the same explicit movement-activation boundary. The fix changes only
the include and prepare call site; planner logic, RNG, commit/rollback and
publication logic are unchanged.

Expected hardware witness for the corrected candidate:

```text
MONSTER3GOAL PLAN ... goal=2/3 ... publish=pending
MONSTERMOVELIVE COMMIT ...
MONSTER3GOAL COMMIT ... goal=2/3 ...
```

and, when geometry permits, the same sequence for goal 3/3. Until observed on
the real CYD, the authoritative hardware-tested code boundary remains
`4731d8265e90da19dc6d911739c4bf5574117a4d`.


A follow-up source-family audit after the correction scanned all 15
`ESP32/src/*monster*.c|cpp` units. The only remaining textual occurrences of
`EspNativeGameplayMonsterPosition_prepareCardinalMove` are the permanent
movement-activation composition boundary and the lower-level position owner's
own implementation. No monster-domain caller bypasses the explicit boundary.


## Corrected active-wrap replacement — REAL-CYD PASS

The corrected code boundary is
`aa7cb5c778264e1bb61d442d1c9864c09e6f37a3`. The later branch head
`25cfb5f9f09c95cff2a6edc716ea1f51c518d432` contains documentation only
relative to that code and builds successfully in esp32-cyd CI #1020.

```text
CI #1020 = SUCCESS
static RAM = 45784 B
flash = 816517 B
artifact id = 11041812545
active --wrap entries = 64
```

The real classic CYD now proves the exact continuation path that failed in the
first candidate. After event 51 makes both subtype-4 monsters visible, the
active set grows to four. Ordinary members 218 and 237 commit first, then both
subtype-4 members execute complete same-turn three-goal movement.

For sprite 0:

```text
first goal: tile 436 -> 468 COMMIT
goal 2/3: 468 -> 469 COMMIT
goal 3/3: 469 -> 470 COMMIT
MONSTER3GOAL COMPLETE
```

For sprite 1:

```text
first goal: tile 564 -> 532 COMMIT
goal 2/3: 532 -> 533 COMMIT
goal 3/3: 533 -> 534 COMMIT
MONSTER3GOAL COMPLETE
```

Every continuation is preceded by
`MONSTERMOVEACT ALLOW ... position-preflight-captured`, proving that the
explicit replacement supplies the same activation gate and publication capture
previously supplied implicitly by the linker wrapper.

The turn closes with:

```text
[MONSTERACTIVESEQ] COMPLETE turn=2 reason=1 activeCount=4 delivered=4
sameMonsterTurn=yes ordered=yes publication=per-member multiAttack=deferred
[ALIVE] uptime=340952 ms ...
```

No `probe-sequence-or-capture-mismatch` remains. RNG replay remains one byte
per successful continuation, topology relink publishes each move, and rollback
closes after every live commit.

Therefore removal of
`--wrap=EspNativeGameplayMonsterPosition_prepareCardinalMove` is hardware
validated on the real classic CYD.


## Explicit movement reset composition — partial REAL-CYD validation

Candidate `cb45792af62d8ad0946dc4d477b288ef92aecf3a` removes
`--wrap=EspNativeGameplayMonsterMovementProbe_reset` and moves the exact reset
order into the permanent `EspNativeGameplayMonsterMovementProbe_reset()` API:

```text
ThreeGoalTurn_reset
MovementPublish_reset
MonsterMovement_reset
MonsterPosition_reset
```

CI #1022 succeeds with 45784 B static RAM and 816509 B flash. The active linker
wrap count is 63.

The real CYD validates the pre-reset side and the checkpoint reset/restore path:

```text
before SAVE:
  activeCount=4 delivered=4 ordered=yes
  subtype-4 goals 2/3 and 3/3 commit for sprites 0 and 1

SAVE V9:
  monsters=50
  topology=65
  positions=50
  activation=4

LOAD:
  RESIDENTRESET ... after=0/0/0/0/0/0/0 empty=1
  MONSTERSTATE STAGE-RESTORE ... exact saved FNV
  MAPCHECKPOINTTOPO RESTORE ... exact saved FNV
  MONSTERPOS STAGE-RESTORE ... exact saved FNV
  MONSTERACT RESTORE activeCount=4 orderCount=4 ... selection=clear
  MONSTERSTATE RESTORE
  MONSTERPOS RESTORE
  MONSTERMOVE READY
  MONSTERACTIVESEQ READY
  ENGINESESSION READY ... shapeData=0x0 mediaTexels=0x0
```

This proves that the explicit reset sequence does not break checkpoint teardown,
V9 spatial restoration, owner reinitialization, cache priming or session resume.

The supplied trace ends immediately after `ENGINESESSION READY`, before any
post-LOAD monster movement is serviced. Therefore `cb45792...` is NOT yet a
complete hardware-pass boundary. One post-LOAD MOVE that reaches
`MONSTERMOVEACT` / `MONSTERMOVELIVE` (and preferably a subtype-4
`MONSTER3GOAL` continuation) is still required to prove the reset consumers
restart cleanly.
