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
