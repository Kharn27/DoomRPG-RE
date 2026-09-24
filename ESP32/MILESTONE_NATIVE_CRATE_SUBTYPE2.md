# Milestone — native crate subtype 2

Status: **REAL-CYD PASS / merge-ready**

Hardware-tested code boundary:

```text
571a1af81469ff85a88ae3ba94e5dc9535b6648a
```

Normal GitHub Actions reference:

```text
ESP32 CYD Build / run 35713491939 = SUCCESS
RAM static = 44832 B
```

Base main at branch creation:

```text
c6605cefa74750b5b745b01d8b22ff500f3a81be
PR #143
```

Branch:

```text
agent/esp32-native-crate-subtype2
```

## Goal

Own exactly the original generic type-12 / subtype-2 crate behavior without
introducing a legacy `Entity_t` graph or mutating immutable BSP records.

This milestone keeps unrelated destructible families fail-closed.

Permanent invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
immutable EspMapRuntime
native PAK/raw-flash gameplay backing
bounded mutable semantic owners only
```

## Legacy behavior recovered

The original SELECT path first checks the target EntityDef weapon mask:

```text
(1 << playerWeapon) & entityDef.parm
```

An allowed weapon then uses the normal generic combat path. A successful
destructible hit immediately reaches `Entity_died()`; crates do not own a
persistent HP bar in this legacy path.

For subtype 2, `Entity_died()` consumes the exact consequence RNG buckets:

```text
first < 2      -> trapped explosion + remove
first < 4      -> transform to type 3 / subtype 23
first < 12     -> transform to type 3 / subtype 22
first < 24     -> transform to type 4 / subtype 25
first < 150    -> transform to type 3 / subtype 21
first < 213    -> transform to type 6 / subtype (second RNG byte % 5)
otherwise      -> break/remove
```

The strict native probe covers boundaries
`0/2/4/12/24/150/213` without consuming gameplay RNG.

## Compact native owner

A transformed crate keeps the original immutable BSP sprite position and stores
only:

```text
spriteIndex
effective EntityDef tile
```

Maximum capacity:

```text
64 transformed crates
4 B per record
owner ~= 280 B
```

The owner projects the effective definition into:

```text
sprite renderer
effective topology/entity view
player-resource pickup flow
```

Thus a crate transformed into a pickup is still a world object and can be
picked up normally. It is deliberately not represented as a V5 removed bit.

## Hardware-discovered startup DRAM regression

The first complete candidate kept the ~280 B crate owner in static BSS:

```text
77bac6c3a78455b409a95a475bf2a658dabc9f00
CI = SUCCESS
static RAM = 45104 B
```

The merged V5 main used:

```text
44824 B
```

So the crate candidate added exactly 280 B of permanent startup RAM.

On the real classic CYD this was enough to break the two-stage ZIP inflate of
`mappings.bin`:

```text
[CONFIG] DONE heap8=32884 largest8=16372
[ZIP] read mappings.bin method=8 c=2156 u=8392
[DOOM ERROR] DoomRPG Error: out of memory expanding mappings.bin
```

The final correction moves the bounded crate owner out of startup BSS. It is
allocated only after a valid map/player context exists and freed on map reset.

Final build RAM:

```text
main c6605ce...      44824 B
final 571a1af...     44832 B
delta                    +8 B
```

The real CYD subsequently booted through mappings and reached normal gameplay.

## Real-CYD crate witness

Entrance crate:

```text
[ACTIONENGINE] TRACE seq=3 weapon=2 distance=1 tile=873
               target=sprite index=127 type=12 subtype=2
               route=CRATE_SUBTYPE2

[CRATE] ARM seq=3 sprite=127 tile=873
        defTile=161 parm=00000fff weapon=2
        worldDist=4096 ammoType=1 ammoUsage=1 loops=1
```

The generic combat and consequence RNG produced:

```text
[CRATE] CONSEQUENCE seq=3 sprite=127
        first=82 second=0 secondValid=0
        outcome=TRANSFORM effectiveDefTile=92
        rngCombat=2 rngConsequence=1
        damage=6 armorDamage=4
        mutation=transform-overlay
```

The transaction committed:

```text
[CRATE] COMMIT seq=3 sprite=127 weapon=2
        ammo=8->7
        loops=1 hits=1
        outcome=TRANSFORM
        effective=3/21/def92
        removed=0 transformed=1
        turnAdvance=PLAYER_ATTACK-requested
        rollback=closed
```

The same attack then entered the existing monster-turn pipeline:

```text
[MONSTERTURN] SCHEDULE n=2 reason=PLAYER_ATTACK attackSeq=3 ...
[MONSTERTURN] COMPLETE reason=PLAYER_ATTACK ...
```

## Real-CYD transformed pickup witness

After moving onto the transformed crate tile, the existing player-resource
owner saw the projected definition as an Armor Shard:

```text
[PLAYERRES] PREPARE tile=873 sprite=127
            defTile=92 type=3 subtype=21 parm=4
            action=armor value=0->4
            worldRemove=hidden-overlay
```

The pickup committed normally:

```text
[PLAYERRES] COMMIT tile=873 candidates=1 consumed=1
            armor=4/20
            message=pickup-live
            rollback=closed

[PLAYERRES] FEEDBACK ... message="Got Armor Shard" sourceDefTile=92
```

The user visually confirmed that the crate produced an Armor Shard.

This validates the permanent path:

```text
crate
 -> generic combat
 -> exact consequence RNG
 -> compact transform owner
 -> renderer/topology projection
 -> existing native pickup owner
 -> consumed resource overlay
```

## Scope not claimed by this hardware run

The strict software probe validates all threshold mappings, but this hardware
run observed only the `first=82` transform-to-`type3/subtype21` branch.

Therefore the following are not separately claimed as hardware-observed yet:

```text
trapped crate branch
ordinary break/remove branch
ammo transform requiring second RNG byte
other transform buckets
rocket/BFG radial-damage crate attacks
other type-12 destructible subtypes
```

The generic code remains fail-closed for radial-damage families not yet owned.

## Persistence boundary

A crate transformed into a pickup is **not persisted by SAVE V5**.

V5 can persist removed sprites, but a transformed crate remains present and has
a different effective EntityDef. Encoding it as removed would be semantically
wrong.

The next coherent save boundary is therefore a bounded checkpoint section for
crate transform records, preserving:

```text
runtime/map identity
transformedCount
{spriteIndex,effectiveDefTile} records
semantic fingerprint
```

without serializing legacy entities.

## Post-merge integration finding — initial RNG table

A later real-CYD run exposed repeated trapped crates:

```text
first=0 -> TRAPPED_REMOVE
rngByte=0 -> blast=5
message="10 damage!"
```

Different crates and different opening orders repeated the same result. This was
not an error in the crate threshold mapping. The ESP32 bring-up creates the real
`DoomRPG_t` root with `SDL_calloc()`, leaving its 128-byte `Random_t.randTable`
all zero. Because `DoomRPG_randNextByte()` only refills at the table boundary,
the first 128 byte draws were forced to zero.

Integration code head `6ab5d25216b52f096563a95749f1dbd8b33712dd` now calls one
`DoomRPG_setRand(&doomRpg->random)` when the real core root is created. This is
the inherited table generator; later RNG cadence and replay-guard semantics are
unchanged.

The `10 damage!` value is legacy-correct for a minimum trap roll: the original
explosion calls radius damage with `(rnd+5, rnd+5)`, and `Player_pain()` displays
their sum. With byte zero that is `5 + 5 = 10`.

Real-CYD proof after checkpoint reload:

```text
[CRATE] CONSEQUENCE seq=4 sprite=82 first=99 second=0 secondValid=0
        outcome=TRANSFORM effectiveDefTile=92
        rngCombat=2 rngConsequence=1
        attackDamage=6 attackArmorDamage=4
[CRATE] COMMIT ... outcome=TRANSFORM effective=3/21/def92
        removed=0 transformed=1 rollback=closed
```

`first=99` is in the exact recovered `24..149` bucket and produced an Armor
Shard (`type=3/subtype=21`). This hardware witness proves the live post-boot /
post-LOAD RNG stream is no longer the calloc-zero table.

Build reference:

```text
esp32-cyd CI #693 = SUCCESS
static RAM = 45736 B
flash = 764741 B
```
