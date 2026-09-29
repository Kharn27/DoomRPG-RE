# Milestone — ESP32 structural consolidation: wrapper cleanup v1

Date: 2026-09-29

## Boundary

```text
main = 8d5bbaf5445fb4557bbf4f66df6aa6d692bb9a56
branch = agent/esp32-consolidation-monster-wraps-v4
hardware-tested code head = 3f9b862bcbca3d2217efe0b388e3c9914d1d4b23
esp32-cyd CI #1056 = SUCCESS
static RAM = 45776 B
flash = 815609 B
artifact id = 11052390593
translation units = 173
active --wrap entries = 59
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

## Historical next candidate (completed below)

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

and, when geometry permits, the same sequence for goal 3/3. At that point in
the investigation, before the corrected hardware run below, the authoritative
hardware-tested code boundary still remained
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


## Explicit MovementProbe reset composition — REAL-CYD PASS

Candidate `cb45792af62d8ad0946dc4d477b288ef92aecf3a` is now fully hardware
validated.

The first post-LOAD MOVE starts the movement probe counter at `n=1`, proving
the reset owner was actually reinitialized rather than merely surviving restore.
The restored active set then runs all four members in order:

```text
sprite 218 subtype 3: live COMMIT
sprite 237 subtype 5: live COMMIT
sprite 0 subtype 4: first goal + goal 2/3 COMMIT + goal 3/3 COMMIT + COMPLETE
sprite 1 subtype 4: first goal + goal 2/3 COMMIT + goal 3/3 COMMIT + COMPLETE
MONSTERACTIVESEQ COMPLETE activeCount=4 delivered=4 ordered=yes
ALIVE uptime=454330 ms
```

All continuation probes pass `MONSTERMOVEACT ALLOW`, all publications close
their rollback/topology transaction, and no stale pre-load movement or
three-goal state leaks into the restored session.

Therefore removal of
`--wrap=EspNativeGameplayMonsterMovementProbe_reset` is hardware validated on
the real classic CYD.


## Explicit synthetic MovementView publication — REAL-CYD PASS

Commit `560e54bd2d32fe1f5d704cd9ef0d3737c57f765b` removes
`--wrap=EspNativeGameplayMonsterMovement_view`.

Previously, the three-goal owner temporarily intercepted the global movement
view and returned `syntheticMovementView` while the existing publisher
validated a continuation. That implicit global dependency is replaced by the
explicit permanent API:

```text
EspNativeGameplayMonsterMovementPublish_afterProbeWithView(...)
```

Normal movement still calls the ordinary publisher, which reads the real
movement owner. Only the bounded three-goal continuation passes its synthetic
view explicitly. The temporary `syntheticMovementActive` interception state
and the `__wrap/__real` Movement_view pair are gone.

Build witness:

```text
esp32-cyd CI #1028 = SUCCESS
static RAM = 45784 B
flash = 816509 B
artifact id = 11043841537
active --wrap entries = 62
```

Real-CYD witness, sprite 0 subtype 4:

```text
MONSTER3GOAL ARM chain=1
first goal 470 -> 471 COMMIT
goal 2/3: PLAN 471 -> 439, MONSTERMOVELIVE COMMIT, MONSTER3GOAL COMMIT
goal 3/3: PLAN 439 -> 440, MONSTERMOVELIVE COMMIT, MONSTER3GOAL COMMIT
MONSTER3GOAL COMPLETE
```

Real-CYD witness, sprite 1 subtype 4:

```text
MONSTER3GOAL ARM chain=2
first goal 534 -> 535 COMMIT
goal 2/3: PLAN 535 -> 536, MONSTERMOVELIVE COMMIT, MONSTER3GOAL COMMIT
goal 3/3: PLAN 536 -> 537, MONSTERMOVELIVE COMMIT, MONSTER3GOAL COMMIT
MONSTER3GOAL COMPLETE
```

The same turn closes with:

```text
MONSTERACTIVESEQ COMPLETE turn=1 reason=1 activeCount=4 delivered=4
sameMonsterTurn=yes ordered=yes publication=per-member multiAttack=deferred
ALIVE stable through uptime=55893 ms
```

No publication mismatch appears. Activation capture, one-byte continuation RNG,
position rollback, topology relink and renderer publication all remain intact.
Therefore the Movement_view linker interception is fully hardware validated.

## Explicit MonsterTurn post-move composition — REAL-CYD PASS

Commit `2976cf9f157fa3dfd1649efaaec77986450648cc` removes the active
`--wrap=EspNativeGameplayMonsterTurn_postMoveGoal`.

The old linker wrapper lived in ThreeGoalTurn and delegated non-4/13 monsters
through `__real_EspNativeGameplayMonsterTurn_postMoveGoal`. Ownership is now
explicit: `EspNativeGameplayMonsterTurn_postMoveGoal` dispatches subtype 4/13
to `EspNativeGameplayMonsterThreeGoalTurn_postMoveGoal`; all other families
stay on the ordinary implementation. The dispatch occurs before ordinary
turn-owner sync/probe accounting, preserving the previous wrapper ordering.

Build witness:

```text
esp32-cyd CI #1036 = SUCCESS
static RAM = 45784 B
flash = 816545 B
artifact id = 11044812987
active --wrap entries = 61
translation units = 174
```

The real classic CYD validates this after V9 LOAD. The first post-load movement
probe starts at `n=1`, and one MOVE services all four active monsters in order.

```text
sprite 218 subtype 3: MONSTERMOVELIVE COMMIT + MONSTERPOSTMOVE COMPLETE
sprite 237 subtype 5: MONSTERMOVELIVE COMMIT + MONSTERPOSTMOVE COMPLETE

sprite 0 subtype 4:
  first goal 470 -> 471 COMMIT
  goal 2/3 471 -> 439 COMMIT
  goal 3/3 439 -> 440 COMMIT
  MONSTER3GOAL COMPLETE

sprite 1 subtype 4:
  first goal 534 -> 535 COMMIT
  goal 2/3 535 -> 536 COMMIT
  goal 3/3 536 -> 537 COMMIT
  MONSTER3GOAL COMPLETE
```

Every subtype-4 continuation passes `MONSTERMOVEACT ALLOW`; each committed
continuation reports `rngCalls=1`, publishes topology/position and closes
rollback before the next member. The turn closes with:

```text
[MONSTERACTIVESEQ] COMPLETE turn=1 reason=1 activeCount=4 delivered=4
sameMonsterTurn=yes ordered=yes publication=per-member multiAttack=deferred
[ALIVE] uptime=49239 ms heap=82696 heap8=17144 largest8=10228
```

Therefore removal of
`--wrap=EspNativeGameplayMonsterTurn_postMoveGoal` is hardware validated on
the real classic CYD.

## Obsolete MonsterState view witness retirement — REAL-CYD PASS

After the previous four semantic wrapper removals were merged, the next audit
starts from exact main `da8c3632162ad8dc7a0a83e7c398d815a0fbfea2`.
Three-goal witness code is now historical instrumentation rather than permanent
runtime behavior.

`esp_native_gameplay_monster_three_goal_witness.c` contained only:

```text
__real_EspNativeGameplayMonsterState_view()
__wrap_EspNativeGameplayMonsterState_view()
local topology/read helpers
MONSTER3GOAL WITNESS / WITNESS-DEFER / CENSUS printf diagnostics
```

The wrapper always returned the same real `MonsterState_view` pointer and did
not mutate monster state, RNG, topology, rendering or turn sequencing. Commit
`57da58cb23a9de923a2c1bc3cd158e5b5818a7b0` removes its linker
`--wrap`; commit `b2c22ee699213a04429669c6b3ca63c479918d1f`
deletes the 119-line witness translation unit entirely.

Build witness:

```text
esp32-cyd CI #1049 = SUCCESS
static RAM = 45776 B
flash = 815609 B
artifact id = 11045807444
translation units = 173
active --wrap entries = 60
```

Compared with merged main, this removes one translation unit and one linker
interception while saving 8 B static RAM and 936 B flash.

The real classic CYD validates the permanent gameplay path unchanged. One MOVE
after the existing Sector 1 save executes the same ordered four-monster turn:

```text
sprite 218 subtype 3:
  first movement COMMIT
  MONSTERPOSTMOVE COMPLETE

sprite 237 subtype 5:
  first movement COMMIT
  MONSTERPOSTMOVE COMPLETE

sprite 0 subtype 4:
  first goal 470 -> 471 COMMIT
  goal 2/3 471 -> 439 COMMIT
  goal 3/3 439 -> 440 COMMIT
  MONSTER3GOAL COMPLETE

sprite 1 subtype 4:
  first goal 534 -> 535 COMMIT
  goal 2/3 535 -> 536 COMMIT
  goal 3/3 536 -> 537 COMMIT
  MONSTER3GOAL COMPLETE

MONSTERACTIVESEQ COMPLETE turn=1 reason=1 activeCount=4 delivered=4
sameMonsterTurn=yes ordered=yes publication=per-member multiAttack=deferred
ALIVE uptime=56336 ms heap=82704 heap8=17152 largest8=10228
```

All subtype-4 continuation moves still pass `MONSTERMOVEACT ALLOW`; each live
commit consumes exactly one movement RNG byte, publishes position/topology and
closes rollback. The removed `MONSTER3GOAL WITNESS/CENSUS` diagnostics no
longer appear, while the permanent `READY/ARM/PLAN/COMMIT/COMPLETE` chain
remains present.

Therefore `--wrap=EspNativeGameplayMonsterState_view` and its dedicated
diagnostic translation unit are fully retired and hardware validated.

## Explicit HUB/automap action-feedback composition — REAL-CYD PASS

After the previous five wrapper retirements were merged, the next audit starts
from exact main `8d5bbaf5445fb4557bbf4f66df6aa6d692bb9a56`.

The remaining `--wrap=EspNativeGameplayMonsterState_actionService` was not
monster-AI logic. It existed only to pause the world/action feedback leaf while
HUB or automap owned the framebuffer, preventing delayed top-bar or viewport
feedback expiry from restoring stale world pixels underneath an overlay.

Commit `3f9b862bcbca3d2217efe0b388e3c9914d1d4b23` replaces that hidden
linker composition with the explicit permanent API:

```text
EspNativeGameplayMonsterCombat service
 -> EspNativeGameplayHubActionGate_service
 -> EspNativeGameplayMonsterState_actionService
 -> generic ActionEngine service
```

No timing or gameplay rule moves. The gate retains its one-byte paused owner,
the same realtime lease semantics, the same `HUBACTIONGATE PAUSE/RESUME`
diagnostics, and the same return behavior.

Build witness:

```text
esp32-cyd CI #1056 = SUCCESS
static RAM = 45776 B
flash = 815609 B
artifact id = 11052390593
translation units = 173
active --wrap entries = 59
```

RAM and flash are byte-for-byte identical to merged main; only the active linker
wrap count changes from 60 to 59.

Real-CYD overlay witness:

```text
[HUBACTIONGATE] PAUSE owner=hub worldActionFeedback=yes timer=realtime mutation=no
...
[HUBACTIONGATE] PAUSE owner=automap worldActionFeedback=yes timer=realtime mutation=no
```

Because PAUSE is logged only when the private paused flag changes 0 -> 1, seeing
the later automap PAUSE proves the earlier HUB pause cycle already returned
through the resume path before automap opened. After the overlays, a normal MOVE
executes the complete permanent monster chain:

```text
sprite 218 subtype 3: movement COMMIT + MONSTERPOSTMOVE COMPLETE
sprite 237 subtype 5: movement COMMIT + MONSTERPOSTMOVE COMPLETE
sprite 0 subtype 4: 470 -> 471 -> 439 -> 440, MONSTER3GOAL COMPLETE
sprite 1 subtype 4: 534 -> 535 -> 536 -> 537, MONSTER3GOAL COMPLETE
MONSTERACTIVESEQ COMPLETE turn=1 reason=1 activeCount=4 delivered=4
sameMonsterTurn=yes ordered=yes publication=per-member multiAttack=deferred
ALIVE uptime=79136 ms heap=82704 heap8=17152 largest8=10228
ALIVE uptime=84137 ms heap=82704 heap8=17152 largest8=10228
```

Therefore the HUB/automap feedback gate is hardware validated as explicit
native composition and the linker interception is retired.

## Consolidation checkpoint after six active-wrap removals

Relative to current merged main
`8d5bbaf5445fb4557bbf4f66df6aa6d692bb9a56`:

```text
translation units = 173 -> 173
active --wrap flags = 60 -> 59
static RAM = 45776 B -> 45776 B
flash = 815609 B -> 815609 B
```

Across the wider consolidation sequence from the earlier 65-wrap baseline:

```text
translation units = 175 -> 173
active --wrap flags = 65 -> 59
```

Hardware-tested boundaries in order:

```text
4731d826... dormant Retaliation compatibility removal
aa7cb5c7... explicit MonsterPosition prepare activation/capture boundary
cb45792a... explicit MovementProbe reset composition
560e54bd... explicit synthetic MovementView publication
2976cf9f... explicit MonsterTurn post-move composition
b2c22ee6... obsolete MonsterState view witness + TU retirement
3f9b862b... explicit HUB/automap action-feedback gate
```

The remaining active monster-domain wrappers are now the two orchestration
seams:

```text
EspNativeGameplayMonsterTurn_view
EspNativeGameplayMonsterMovement_service
```

The branch is hardware-pass. Any commit after
`3f9b862bcbca3d2217efe0b388e3c9914d1d4b23` must remain documentation-only
before merge. Future work should audit these two seams as an ownership problem,
not remove them mechanically.
