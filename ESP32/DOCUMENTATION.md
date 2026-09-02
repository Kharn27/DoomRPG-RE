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
main = 854ade1a7110fff44926099bdae418ca47e55365
branch = agent/esp32-native-player-resources
base main = 854ade1a7110fff44926099bdae418ca47e55365
hardware-tested code boundary = 8152cf8d233067a44b2d705edcaf315845b7744a
status = generic player resources + shared HUD/ammo projection + generic gib FX hardware PASS
branch policy = LOCKED; docs-only tail only
```

Do not treat commits after `8152cf8d...` as new hardware-tested code. The tail
must remain documentation-only until merge.

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
native idle weapon rendering / attack frame presentation
jammed-door subtype-3 axe destruction + traversal
generic compact monster-state initialization
generic type=1 hit / miss / damage / HP / armor math
generic pain presentation
generic ordinary death -> corpse presentation
generic overkill/gib classification + bounded native gib FX
native player XP ownership / progression state
generic type 3/4/5/6/16 pickup/resource engine
shared PlayerState health/armor/credits/keys/ammo/inventory/weapons
consumed-pickup world removal
HUD projection from PlayerState
extinguisher ammo consumption + fire removal transaction
```

Relevant milestone records:

- [`MILESTONE_NATIVE_JAMMED_DOOR.md`](MILESTONE_NATIVE_JAMMED_DOOR.md)
- [`MILESTONE_NATIVE_MONSTER_COMBAT.md`](MILESTONE_NATIVE_MONSTER_COMBAT.md)
- [`MILESTONE_NATIVE_PLAYER_RESOURCES.md`](MILESTONE_NATIVE_PLAYER_RESOURCES.md)

## Generic player resource engine

The resource path is deliberately shared across item categories:

```text
EntityDef {tile,type,subtype,parm}
 -> PlayerResources classifier
 -> one 52 B PlayerState
 -> one consumed-sprite bitset
 -> HUD/world projection
 -> transactional redraw
```

Entrance hardware corpus:

```text
114 pickups total
type3  = 84
type4  = 6
type5  = 3
type6  = 17
type16 = 4
consumed bitset = 43 B for 344 sprites
EntityDef metadata = 115 x 8 B = 920 B
```

Real-CYD witnesses include armor shards, medkit/inventory, extinguisher weapon,
extinguisher ammo and credits. The picked sprites disappear physically and the
same PlayerState values drive the HUD.

Do not create separate health-pickup, armor-pickup, medkit, ammo, credit or
weapon owners.

## Shared PlayerState

`EspNativeGameplayPlayerState` is 52 B and is the permanent player-facing owner
for:

```text
HP / max HP
armor / max armor
defense / strength / agility / accuracy
XP / level / next-level XP
keys / credits
ammo[6]
inventory[5]
weapon ownership / selected weapon
```

Combat, resources and future key/script consequences should continue to use this
same owner.

## Generic monster engine, not per-monster routes

Combat remains split into reusable pieces:

```text
MonsterTrace
 -> CombatMath
 -> MonsterState + PlayerState
 -> MonsterCombat transaction
 -> visual/liveness projection
```

Ordinary monster differences are data (`subtype`, `mType`, randomized stats),
not executor code.

Hardware has exercised the same backend with Hellhound subtype 1 and Zombie
subtype 0, including hits, misses, pain, lethal XP/liveness and overkill/gib.

### Presentation state machine

```text
nonlethal hit
 -> pain visual 6 / 250 ms
 -> normal visual

ordinary non-gib death
 -> death visual 4 / 250 ms
 -> corpse visual 2, unlinked

overkill/gib death
 -> death visual 4 / 250 ms
 -> hidden + bounded gib burst
 -> burst expires after 350 ms via autonomous world redraw
```

The gib layer owns 148 B, uses a local deterministic visual RNG and never consumes
gameplay RNG. It does not revive the legacy `ParticleSystem`.

## Extinguisher transaction

The extinguisher now reads/writes the same ammo owner as pickups/HUD. Hardware
witness:

```text
ammo0 = 10 -> 9
fire = removed
HUD = decremented
rollback = armed/closed
```

The historical +2 XP consequence remains deferred.

## Representative final RAM

Latest real-CYD gameplay witness:

```text
heap8 = 19788 B
largest8 = 14324 B
shapeData = NULL
mediaTexels = NULL
```

No PSRAM is present.

## Current intentionally deferred families

```text
pickup sounds/messages/got-face presentation
combat MISS/HIT/CRIT text feedback
action XP migration: extinguisher +2, jammed door +1
materialized monster drops
corpse-pile trimming
enemy AI / retaliation / turn advance
actual sound playback
chaingun/plasma multi-loop presentation/commit
rocket/BFG radius damage
special death consequences for subtypes 7, 8, 12, 13
Kronos-specific semantics
password input
SAVEGAME / CHANGEMAP production route
```

These are mechanical family boundaries, not individual monster/item TODOs.

## CHANGEMAP recovery point

Entrance event 1 / tile 69 remains recovered but intentionally deferred:

```text
SAVEGAME -> /junction.bsp, targetMapId 9, savePos 992,1888 angle 64
CHANGEMAP -> /junction.bsp, targetMapId 9, showStats 1, spawnParam 0
OPENLINE -> third eligible command
```

Do not force the transition before enough native gameplay exists to complete the
map normally.

## After this merge

Do not continue code on this locked branch.

When the merge is announced:

1. read the true GitHub `main` and exact SHA;
2. re-read `PORTING_STATUS.md`, this file and the latest milestone;
3. create a fresh coherent `agent/*` branch from that SHA;
4. choose the next bounded gameplay family from the actual merged frontier.

Likely high-value families include shared combat/action text feedback, migration
of deferred XP consequences into PlayerState, standard ammo-consuming direct-fire
weapons, monster turn/retaliation ownership, or materialized drops. Decide only
after reading merged `main`.

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
