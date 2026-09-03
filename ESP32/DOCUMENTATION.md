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
main = 65dea748ef8d2e5a6f3823676ac05ee62fb89407
branch = agent/esp32-native-monster-movement
base main = 65dea748ef8d2e5a6f3823676ac05ee62fb89407
hardware-tested code boundary = 7dd7faa3994969e43ccb27b5450ee828ade3c323
status = native monster position + movement planner probe + persistent RNG reservation hardware PASS
branch policy = LOCKED; docs-only tail only
```

Do not treat commits after `7dd7faa...` as new hardware-tested code. The tail
must remain documentation-only until merge.

## Build environment

Normal hardware reference:

```text
pio run -e esp32-cyd
```

Bring-up diagnostics perturb RAM and are not the production memory canon. Never
claim a local build or hardware pass that did not occur.

A future GitHub Actions workflow may run the same `esp32-cyd` build before
hardware testing, but CI build success will not replace real-CYD serial
validation.

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

Hardware-owned behavior now includes:

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
generic type=1 player attack hit / miss / crit / HP / armor math
generic pain / ordinary corpse / overkill-gib presentation
native player XP ownership / progression state
generic type 3/4/5/6/16 pickup/resource engine
shared PlayerState health/armor/credits/keys/ammo/inventory/weapons
consumed-pickup world removal
HUD projection from PlayerState
extinguisher ammo consumption + fire removal transaction
generic stationary monster-turn scheduling and LOS recovery
live nonlethal monster retaliation into PlayerState
transaction-safe RNG refill replay across the legacy 128-byte table boundary
compact mutable native monster position ownership
legacy-compatible bounded monster movement planner probe
persistent movement-probe RNG refill reservation + rollback replay chaining
```

Relevant milestone records:

- [`MILESTONE_NATIVE_JAMMED_DOOR.md`](MILESTONE_NATIVE_JAMMED_DOOR.md)
- [`MILESTONE_NATIVE_MONSTER_COMBAT.md`](MILESTONE_NATIVE_MONSTER_COMBAT.md)
- [`MILESTONE_NATIVE_PLAYER_RESOURCES.md`](MILESTONE_NATIVE_PLAYER_RESOURCES.md)
- [`MILESTONE_NATIVE_MONSTER_TURN.md`](MILESTONE_NATIVE_MONSTER_TURN.md)
- [`MILESTONE_NATIVE_MONSTER_MOVEMENT.md`](MILESTONE_NATIVE_MONSTER_MOVEMENT.md)

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

Player attacks, resources, HUD projection and monster retaliation all use this
same owner.

## Generic player resource engine

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

Do not create separate health, armor, medkit, ammo, credit or weapon owners.

## Generic monster engine

Ordinary monster differences are data (`subtype`, `mType`, randomized stats),
not executor code.

Player-attack side:

```text
MonsterTrace
 -> CombatMath
 -> MonsterState + PlayerState
 -> MonsterCombat transaction
 -> visual/liveness projection
```

Enemy-turn side:

```text
committed player action
 -> MonsterTurn schedule
 -> candidate + cardinal LOS recovery
 -> exact rollback probe
 -> conservative activation delivery
 -> MonsterRetaliation transaction
 -> PlayerState + redraw
```

Movement side now has its own permanent owner/probe layer:

```text
MonsterState + topology
 -> MonsterPosition {sprite,tile,x,y}
 -> legacy movement trigger/path planner
 -> temporary cardinal commit
 -> exact rollback
 -> live topology/renderer publication deferred
```

Hellhound subtype 1 and Zombie subtype 0 have exercised the generic combat/turn
paths on the real CYD. Hellhound sprite 179 is also the first movement planner
hardware witness.

### Presentation state machine

```text
nonlethal player hit on monster
 -> pain visual 6 / 250 ms
 -> normal visual

ordinary non-gib monster death
 -> death visual 4 / 250 ms
 -> corpse visual 2, unlinked

overkill/gib monster death
 -> death visual 4 / 250 ms
 -> hidden + bounded gib burst
 -> burst expires after 350 ms via autonomous world redraw
```

Monster attack animation, player pain FX, damage text and audio are not yet owned.

## Native monster retaliation boundary

Current live enemy retaliation is deliberately bounded to one unambiguous active
monster that is cardinally aligned with the player and has native line of sight.

Owned now:

```text
MOVE / ROTATE / PLAYER_ATTACK scheduling
stationary cardinal candidate recovery
native tile/line/sprite LOS
subtype + alternateAttack weapon selection
ranged AI decision byte where required
legacy-compatible hit / crit / damage / armor split
nonlethal HP/armor mutation in PlayerState
transactional redraw / rollback
```

Still fail-closed:

```text
multiple-attacker activation order
special subtype-10 AI
player lethal/death transition
dog-familiar redirection
monster attack / player pain presentation
sound and combat text feedback
```

## Native monster position owner

The position layer deliberately does not reuse immutable BSP sprite x/y as live
state. Initial position comes from the native topology tile center.

```text
record = spriteIndex:uint16 + tileIndex:uint16 + worldX:uint16 + worldY:uint16
record size = 8 B
Entrance count = 30
payload = 240 B
allocation = one map/session 8-bit-capable allocation
```

Prepare/commit/rollback accepts exactly one cardinal 64-unit move and maintains a
fingerprint over the complete owner.

Real-CYD movement probe witness:

```text
sprite=179 subtype=1 weapon=12
sourceTile=750 source=928,1504
target=928,1440
destTile=718 delta=0,-64
tieRand=217 choice=0 mask=ff87 rngCalls=1
positionFNV=61296c4a->10cf73aa->61296c4a
positionRollback=yes
randomLiveUntouched=yes
liveMove=no
```

Renderer publication, topology relink and interpolation remain deferred.

## Legacy-compatible movement planner boundary

The bounded planner recovers the first ordinary movement behavior from legacy
`Entity_aiThink` / `Entity_aiGoal_MOVE`:

```text
attack trace mask = 0x5687
movement mask = 0xff87
subtype 4/13 => remove 0x0c00
subtype 6/7  => remove 0x0800
subtype 10   => remove 0x0400
calcPath = two-step greedy squared-distance look-ahead
cardinal order = east, west, south, north
north legacy quirk preserved
visit choice = visitOrder[(rand & 3) % count]
```

The service consumes no live gameplay RNG during movement probing. It accepts
only one unambiguous alive linked map-session-active ordinary monster and fails
closed when order/geometry/special behavior is not owned.

## RNG replay guard + persistent movement reservation

`Random_t` is not the complete legacy RNG state. At the end of its 128-byte
table, `DoomRPG_setRand()` regenerates the table using hidden file-static state.

The generic guard now has two related bounded mechanisms:

```text
ordinary transactional replay lease = 1000 ms
movement-probe reservation = persistent until exact live boundary replay
current static guard owner = 284 B on 32-bit ESP32
allocation = none
```

For movement, a post-refill table may be generated once and reserved while live
`Random_t` is restored to its exact pre-refill bytes. Repeated movement probes
can borrow the same table even after the ordinary 1000 ms lease would have
expired.

The decisive final boundary chain on the real CYD is:

```text
PROBE-REFILL refill=1 hiddenGenerator=advanced-once
MONSTERMOVE tieRand=217 randomLiveUntouched=yes positionRollback=yes
PROBE-RESTORE liveRandomExact=yes reservation=pending
PROBE-REPLAY refill=1 replay=1 reservation=consumed rollbackReplay=armed sequenceExact=yes
MONSTERTURN ATTACK-PROBE firstRandHit=217 rngRollback=yes playerExact=yes
REPLAY refill=1 replay=2 hiddenGenerator=untouched rollbackSafe=yes
MONSTERRETAL MISS-COMMIT firstRandHit=217 gameplayRngCommitted=yes playerMutation=no
```

This matters because the first real boundary consumer is still the transactional
monster-turn probe. `PROBE-REPLAY` therefore arms the normal rollback lease so
the subsequent live retaliation can replay the exact same refill.

A later ordinary turn in the same session matched probe/live exactly with:

```text
firstRandHit = 203
firstRandDamage = 213
totalDamage = 2
armorDamage = 2
player = HP 30->28, armor 12->10
```

## Extinguisher transaction

The extinguisher reads/writes the same ammo owner as pickups/HUD. Hardware
witness:

```text
ammo0 = 10 -> 9
fire = removed
HUD = decremented
rollback = armed/closed
```

The historical +2 XP consequence remains deferred.

## Representative final RAM

Latest real-CYD gameplay witness at the locked code boundary:

```text
heap = 84648 B
heap8 = 18884 B
largest8 = 13812 B
shapeData = NULL
mediaTexels = NULL
PSRAM = none
```

The selected resident graphics-cache geometry remains unchanged.

## Current intentionally deferred families

```text
pickup sounds/messages/got-face presentation
combat/retaliation MISS/HIT/CRIT text feedback
action XP migration: extinguisher +2, jammed door +1
materialized monster drops
corpse-pile trimming
live monster movement publication into topology/renderer
monster movement interpolation/animation
multiple-monster activation/movement ordering
unsupported special calcPath plane corpus
special subtype-10 AI
player lethal/death retaliation transition
monster attack/player pain animation and FX
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
2. re-read `PORTING_STATUS.md`, this file and
   `MILESTONE_NATIVE_MONSTER_MOVEMENT.md`;
3. create a fresh coherent `agent/*` branch from that SHA;
4. choose the next bounded gameplay family from the actual merged frontier.

Likely high-value next families are live movement publication/topology relink,
monster attack/player-pain presentation, player lethal/death transition, deferred
action XP migration, or materialized monster drops. Decide only after reading
merged `main`.

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
