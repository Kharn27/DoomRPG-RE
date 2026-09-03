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
main = 782bee100bad1169b85cc738c3a860e367e81553
branch = agent/esp32-native-pickup-feedback
base main = 782bee100bad1169b85cc738c3a860e367e81553
hardware-tested code boundary = c9df4d452eae2610701fde839b7aa73cdceda0ac
status = pickup feedback + adaptive plane cache + gameplay regression PASS
branch policy = LOCKED; docs-only tail only
```

Do not treat commits after `c9df4d45...` as new hardware-tested code. The tail
must remain documentation-only until merge.

## Build environment

Normal hardware reference:

```text
pio run -e esp32-cyd
```

GitHub Actions runs this environment through `.github/workflows/esp32-cyd.yml`
and uploads firmware artifacts. CI build success is a compile/link gate only and
never replaces real-CYD serial validation.

Bring-up diagnostics perturb RAM and are not the production memory canon. Never
claim a local build or hardware pass that did not occur.

## Hardware / permanent memory rules

```text
classic CYD = ESP32-2432S028R
MCU = ESP32-D0WD-V3 dual core 240 MHz
flash = 4 MB
PSRAM = none
logical framebuffer = 160x120 RGB565 = 38400 B
shapeData == NULL
mediaTexels == NULL
native backing store = /DoomRPG-ESP32.pak
```

Do not recreate map-wide texel ownership or migrate native gameplay/map data back
to ZIP. The current firmware still has a transitional `/DoomRPG.zip` startup
dependency for legacy HUD/layout resources; removal remains migration debt.

## Selected resident-cache baseline

```text
owner = 23592 B
payload = 19456 B (19 KiB)
range records = 288
range record = 12 B
resident entry slots = 24
large exact range = 2048 B
```

Cache recycle stalls remain separate performance work.

## Current native gameplay frontier

Hardware-owned behavior now includes:

```text
movement / turn / strafe
native collision + compact sprite topology
event-first SELECT routing
SHOW / HIDE / UNLOCK
OPENLINE / CLOSELINE
DIALOG / DIALOGNOBACK
FORCEMESSAGE / NOTE
state ops 11 / 19 / 20
regular door animation + mutable line texture variants
native idle weapon rendering + attack frame presentation
jammed-door subtype-3 Axe destruction + traversal
generic compact monster-state initialization
generic type-1 player attack hit/miss/crit/HP/armor math
pain / corpse / overkill-gib presentation
shared 52 B PlayerState + XP/progression state
generic type 3/4/5/6/16 pickup/resource engine
consumed-pickup world removal
HUD projection from PlayerState
extinguisher ammo consumption + fire removal transaction
stationary monster-turn scheduling + LOS recovery
live nonlethal monster retaliation
native PASS_TURN + exact "Turn passed." top-bar feedback
transaction-safe gameplay RNG replay across 128-byte refill boundary
compact mutable MonsterPosition owner
legacy-compatible bounded movement planner
persistent movement RNG reservation/replay
live one-monster movement commit + topology relink
renderer projection of committed moved monster position
generic NEXT + PREV weapon cycling
live selected-weapon HUD/first-person redraw without turn advance
live Pistol ammo consumption + generic monster combat commit
live generic pickup messages + white viewport-border flash
adaptive native plane cache under memory pressure
```

Relevant milestone records:

- [`MILESTONE_NATIVE_JAMMED_DOOR.md`](MILESTONE_NATIVE_JAMMED_DOOR.md)
- [`MILESTONE_NATIVE_MONSTER_COMBAT.md`](MILESTONE_NATIVE_MONSTER_COMBAT.md)
- [`MILESTONE_NATIVE_PLAYER_RESOURCES.md`](MILESTONE_NATIVE_PLAYER_RESOURCES.md)
- [`MILESTONE_NATIVE_MONSTER_TURN.md`](MILESTONE_NATIVE_MONSTER_TURN.md)
- [`MILESTONE_NATIVE_PASS_TURN.md`](MILESTONE_NATIVE_PASS_TURN.md)
- [`MILESTONE_NATIVE_WEAPON_CONTROL.md`](MILESTONE_NATIVE_WEAPON_CONTROL.md)
- [`MILESTONE_NATIVE_MONSTER_MOVEMENT.md`](MILESTONE_NATIVE_MONSTER_MOVEMENT.md)
- [`MILESTONE_NATIVE_MONSTER_MOVEMENT_LIVE.md`](MILESTONE_NATIVE_MONSTER_MOVEMENT_LIVE.md)
- [`MILESTONE_NATIVE_PICKUP_FEEDBACK.md`](MILESTONE_NATIVE_PICKUP_FEEDBACK.md)

## Shared PlayerState

`EspNativeGameplayPlayerState` remains the single 52 B player-facing owner for:

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

## Generic player resources and feedback

```text
EntityDef {tile,type,subtype,parm}
 -> generic PlayerResources classifier
 -> shared PlayerState
 -> one consumed-sprite bitset
 -> HUD/world projection
 -> bounded pickup feedback
 -> transactional redraw
```

Entrance corpus remains:

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

Current real-CYD pickup feedback witnesses:

```text
Armor Shard: armor 0->4 then 4->8
Bullet Clip: ammo1 8->12
Fire Ext: selected weapon 1
1 credit: credits 0->1
```

All use the same bounded presentation contract:

```text
message = top bar / 1200 ms
pickup flash = white RGB565 ffff / 500 ms
flash viewport = 0,20,160,80
border = 2 px / 944 pixels
snapshot = bounded
```

Pickup sound playback and got-face presentation remain deferred.

## Adaptive plane renderer cache

The floor/ceiling path uses independent 2048 B texture leases. Six slots remain
the performance ceiling, but the renderer now accepts any successful prefix of
1..6 slots instead of failing the entire frame when one lease cannot be acquired.

```text
max slots = 6
slot bytes = 2048 B
minimum valid slots = 1
reduced capacity = more PAK misses/reads only
```

This was added after a dialog continuation allocated its 2408 B topology rollback
snapshot and exposed the old all-six-or-fail policy. The final real-CYD run
repeatedly exercised:

```text
[NATIVEPLANE] CACHE-FALLBACK slots=5/6 leaseBytes=2048 totalLeaseBytes=10240
[NATIVEPLANE] rows=80 pixels=12800 ...
[PLANEPROFILE] ... ok=1
```

The fallback remained stable through jammed-door destruction, weapon cycling,
Pistol combat, movement and later pickups. The player reported no recurrence of
the soldier-dialog crash.

## Native generic weapon-control boundary

The circular selector reuses PlayerState and CombatMath weapon metadata. Selection
is allocation-free over historical slots 0..11 and uses the exact legacy gate:
owned plus `ammoUsage == 0 || ammo[ammoType] > 0`.

Weapon cycling redraws the current frame, never advances the turn by itself and
rolls PlayerState back on redraw failure.

NEXT was already independently hardware-validated. The current branch adds real
PREV witnesses:

```text
PREV weapon 1->0, weapons=0007, turn=no, rollback=closed
PREV weapon 0->2, ammoType=1 ammo=12, turn=no, rollback=closed
```

Direct generic single-target combat is live for Axe, Pistol, Shotgun and Super
Shotgun. Chaingun/Plasma multi-loop, Rocket/BFG radial and familiar slots 9..11
remain separate families.

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
committed movement / rotation / player attack / PASS_TURN
 -> MonsterTurn schedule
 -> candidate + cardinal LOS recovery
 -> exact rollback probe
 -> conservative activation delivery
 -> MonsterRetaliation transaction
 -> PlayerState + redraw
```

Movement side:

```text
MonsterState + topology
 -> MonsterPosition {sprite,tile,x,y}
 -> movement trigger/path planner probe
 -> exact gameplay-RNG replay
 -> MonsterPosition commit
 -> SpriteTopology relink
 -> moved-position sprite projection
 -> complete native redraw/present
```

Projection currently snaps to destination; interpolation/animation remains a
future family.

## Current combat regression witness

The final locked-boundary run killed Hellhound sprite 179 with the Pistol after a
gameplay RNG refill while the plane cache was already in 5/6 fallback mode:

```text
weapon=2 distance=3 tile=750 sprite=179
RNG refill=1
roll: totalDamage=5 armorDamage=3 crit=0 rngCalls=4
plane cache=5/6, PLANEPROFILE ok=1
attack pose logical=242 actual=611 frame=1
commit: hp 6->0, armor 2->0, ammo 12->11, xp=5, rollback=closed
```

The same session then moved and consumed another pickup. This is the current
renderer/combat survival witness.

## Representative final RAM

Final hardware `ALIVE` lines at the locked boundary:

```text
heap = 82516 B
heap8 = 16784 B
largest8 = 14324 B
SD/ZIP/VIDEO/CORE/LAYOUT/PRERENDER/RENDER/MAPPINGS/MENUBSP = ready
```

The supplied excerpt does not independently print `shapeData` or `mediaTexels`;
their NULL values remain hard invariants and retained earlier witnesses.

## Current intentionally deferred families

```text
PASS_TURN current-tile type10/11 Entity_touched semantics
pickup sound playback / got-face presentation
combat/retaliation MISS/HIT/CRIT text feedback
action XP migration: extinguisher +2, jammed door +1
materialized monster drops
corpse-pile trimming
monster movement interpolation/animation
multiple-monster activation/movement ordering
unsupported special calcPath plane corpus
special subtype-10 AI
player lethal/death transition
monster attack/player-pain animation and FX
actual sound playback
chaingun/plasma multi-loop mechanics
rocket/BFG radius damage
familiar weapon attack semantics for slots 9..11
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

## After this merge

Do not continue code on this locked branch.

When the merge is announced:

1. read the true GitHub `main` and exact SHA;
2. re-read `PORTING_STATUS.md`, this file and latest relevant milestone(s);
3. create a fresh coherent `agent/*` branch from that SHA;
4. choose the next bounded gameplay family from the actual merged frontier.

High-value candidates include Chaingun/Plasma multi-loop mechanics, Rocket/BFG
radial damage, multiple-monster activation/movement ordering, movement
interpolation/animation, monster attack/player-pain presentation, player lethal
death transition, deferred action XP migration or materialized monster drops.
Decide only after reading merged `main`.

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
 -> verify post-test commits are docs-only
 -> merge-ready
```

After a merge announcement, re-read actual GitHub `main`, record its exact SHA,
and create the next `agent/*` branch from that SHA. Never merge `main` without an
explicit user request.
