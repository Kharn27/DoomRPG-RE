# ESP32 documentation map

Recovery and development should start from:

1. current GitHub `main` and its exact SHA;
2. [`PORTING_STATUS.md`](PORTING_STATUS.md) — authoritative tested/candidate boundary;
3. [`ARCHITECTURE.md`](ARCHITECTURE.md) — permanent native engine design;
4. this file — build/layout/recovery pointers;
5. latest relevant milestone/source on the active branch.

Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime truth.

## Current locked branch

```text
main = 563804b09fda67ba06516c8dc13585a1125a4bb0
branch = agent/esp32-native-dog-combat
base main = 563804b09fda67ba06516c8dc13585a1125a4bb0
hardware-tested code boundary = e56bfcf86489f5b0f9ae10deb29a73fabf098756
status = generic native type=1 monster combat hardware PASS
branch policy = LOCKED; docs-only tail only
```

Do not treat commits after `e56bfcf...` as hardware-tested code. The tail must
remain documentation-only until merge.

## Build environment

Normal hardware reference:

```text
pio run -e esp32-cyd
```

Bring-up diagnostics perturb RAM and are not the production memory canon. Never
claim a local build or hardware pass that did not occur.

## Hardware / permanent memory rules

```text
classic CYD ESP32-2432S028R
ESP32-D0WD-V3 dual core 240 MHz
4 MB flash
no PSRAM
160x120 RGB565 framebuffer = 38400 B
shapeData == NULL
mediaTexels == NULL
native backing store = /DoomRPG-ESP32.pak
```

Do not recreate map-wide texel ownership or migrate native runtime data back to
ZIP.

## Selected resident-cache baseline

```text
owner = 23592 B
payload = 19456 B (19 KiB)
range records = 288
range record = 12 B
resident entry slots = 24
large exact range = 2048 B
```

Cache recycle stalls remain separate performance work. Preserve this baseline
while correctness milestones advance.

## Current native gameplay frontier

Hardware-owned behavior includes:

```text
movement / turn / strafe
native collision
event-first SELECT routing
SHOW / HIDE / UNLOCK
OPENLINE / CLOSELINE
DIALOG / DIALOGNOBACK
FORCEMESSAGE / NOTE
state ops 11 / 19 / 20
regular door animation
mutable line texture variants
weapon pickup and native weapon rendering
attack frame presentation
adjacent extinguisher fire clear
jammed-door subtype-3 axe destruction + traversal
generic compact monster-state initialization
generic type=1 monster hit / damage / HP / armor
generic pain / death / unlink presentation
native player XP ownership / progression state
```

Relevant milestone records:

- [`MILESTONE_NATIVE_JAMMED_DOOR.md`](MILESTONE_NATIVE_JAMMED_DOOR.md)
- [`MILESTONE_NATIVE_MONSTER_COMBAT.md`](MILESTONE_NATIVE_MONSTER_COMBAT.md)

## Generic monster engine, not per-monster routes

The current production combat path is deliberately split into reusable pieces:

```text
MonsterTrace
 -> CombatMath
 -> MonsterState + PlayerState
 -> MonsterCombat transaction
 -> renderer/liveness overlays
```

`MonsterState` holds compact mutable enemy state; `PlayerState` is the single
shared player-facing owner intended for combat, pickups, ammo, inventory, keys
and progression.

Real-CYD hardware proves the same backend against at least two distinct ordinary
monster subtypes:

```text
Hellhound subtype 1, sprite 179:
  6 HP -> 3 HP -> dead
  pain6 then gib-hidden+unlink
  XP +5 applied

Zombie subtype 0, sprite 106:
  7 HP -> dead in one axe hit
  death4+unlink
  XP +6 applied
```

The zombie's later raw `[ACTIONENGINE] TRACE` messages are compatibility-log
noise: the native combat trace correctly filters its `alive=0` record, so no
second `[MONSTERCOMBAT] ARM` occurs.

Ordinary new monsters must remain table/data-driven through `subtype/mType`.
Do not create dog/zombie/imp-specific combat executors.

## Current intentionally deferred combat families

```text
enemy AI / retaliation / turn advance
actual sound playback
corpse-pile trimming
materialized drops
ammo-consuming weapon transaction
chaingun/plasma multi-loop presentation/commit
rocket/BFG radius damage
special death consequences for subtypes 7, 8, 12, 13
Kronos-specific teleport semantics where applicable
```

These are mechanical family boundaries, not individual monster TODOs.

## Next milestone after merge

Preferred next family is **generic player resources / pickups + standard ammo
weapons**.

Goal:

```text
EntityDef/player-facing pickup metadata
 -> one generic pickup/resource executor
 -> one EspNativeGameplayPlayerState owner
 -> health / armor / credits / keys / ammo / inventory / weapons
 -> direct-fire ammo weapons reuse existing CombatMath/MonsterCombat
```

The test corpus should deliberately contain several different pickup categories
and several different weapons. Do not create a PR per item.

Radius damage and genuinely scripted/special item effects remain separate family
boundaries.

## CHANGEMAP recovery point

Entrance event 1 / tile 69 remains recovered but intentionally deferred:

```text
SAVEGAME -> /junction.bsp, targetMapId 9, savePos 992,1888 angle 64
CHANGEMAP -> /junction.bsp, targetMapId 9, showStats 1, spawnParam 0
OPENLINE -> third eligible command
```

Do not force the transition before enough native gameplay exists to complete
the map normally.

## Development workflow

```text
recover true main + docs
 -> choose one bounded behavior FAMILY
 -> recover exact legacy behavior
 -> design small permanent native owner/API
 -> keep genuinely different families fail-closed
 -> commit/push agent/*
 -> test normal esp32-cyd on real CYD
 -> Serial is truth
 -> fix failures directly
 -> after PASS, docs-only tail
 -> merge-ready
```

After a merge announcement, re-read actual GitHub `main`, record its exact SHA,
and create the next `agent/*` branch from that SHA. Never merge `main` without an
explicit user request.
