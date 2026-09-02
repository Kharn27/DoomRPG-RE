# Milestone — Generic native player resources and projection

## Git / hardware boundary

```text
base main = 854ade1a7110fff44926099bdae418ca47e55365
branch = agent/esp32-native-player-resources
hardware-tested code boundary = 8152cf8d233067a44b2d705edcaf315845b7744a
status = REAL-CYD PASS
branch policy = LOCKED; docs-only tail only
```

`8152cf8d...` is the last code commit exercised on the real classic CYD. Commits
after that SHA must remain documentation-only until merge.

## Goal

Replace the historical weapon-only pickup bring-up path with one generic native
resource engine backed by the shared 52 B `EspNativeGameplayPlayerState`.

Permanent data flow:

```text
/entities.db metadata from DoomRPG-ESP32.pak
        -> compact EntityDef type/subtype/parm catalog
        -> generic player resource classifier
        -> one shared PlayerState owner
        -> consumed-sprite overlay + HUD projection
        -> native redraw / rollback
```

No per-item owner is allowed. Armor, health, credits, keys, inventory, ammo and
weapon pickup differences are data/metadata decisions inside one engine.

## EntityDef metadata

The compact native EntityDef catalog now retains:

```text
tile
entity type
subtype
parm
```

The catalog is allocated only when the native gameplay PAK catalog is built; it
is not a permanent boot-time BSS payload. Entrance hardware witness:

```text
[ENTITYDEFTYPE] READY defs=115 metadata=115 cache=920B recordBytes=8 ...
```

This avoids the boot-time fragmentation regression that initially prevented
`menu.bsp` inflate. The corrected real-CYD boot again reaches the normal menu and
Entrance with `shapeData=0x0` and `mediaTexels=0x0`.

## Generic pickup/resource corpus

Entrance corpus reported by the same executor:

```text
[PLAYERRES] CORPUS map=1 arena=c3882516 pickups=114 type3=84 type4=6 type5=3 type6=17 type16=4 routes=all-generic playerOwner=shared
```

Owned families:

```text
type 3  world resources: health / armor / credits / keys
type 4  inventory
type 5  weapons
type 6  ammo
type 16 alternate ammo entries
```

The map-local consumed owner is one bit per map sprite. Entrance uses 43 B for
344 sprites. Player values themselves live only in the shared 52 B PlayerState.

## Real-CYD pickup witnesses

### Armor

Two distinct armor shards were consumed by the same generic path:

```text
[PLAYERRES] PREPARE tile=839 sprite=99 defTile=92 type=3 subtype=21 parm=4 action=armor value=0->4 ... worldRemove=hidden-overlay rollback=armed
[PLAYERRES] COMMIT ... consumed=1 ... armor=4/20 ... rollback=closed

[PLAYERRES] PREPARE tile=838 sprite=86 defTile=92 type=3 subtype=21 parm=4 action=armor value=4->8 ... worldRemove=hidden-overlay rollback=armed
[PLAYERRES] COMMIT ... consumed=1 ... armor=8/20 ... rollback=closed
```

Both sprites disappeared immediately on the real display and armor became 8.

### Inventory / medkit

A type-4 medkit/inventory pickup executed after its tutorial dialog and was
consumed through the same owner:

```text
[PLAYERRES] PREPARE tile=738 sprite=34 defTile=99 type=4 subtype=25 parm=25 action=inventory value=0->1 slot=index/0 worldRemove=hidden-overlay rollback=armed
[PLAYERRES] COMMIT ... consumed=1 ... rollback=closed
```

The user confirmed the pickup disappeared physically.

### Weapon / extinguisher

The extinguisher used the same generic resource path and immediately projected
weapon selection into the HUD/weapon renderer:

```text
[PLAYERRES] PREPARE tile=643 sprite=50 defTile=2 type=5 subtype=1 parm=10 action=weapon ... slot=index/1 worldRemove=hidden-overlay rollback=armed
[WEAPON] DRAW weapon=1 ... pose=idle ...
[PLAYERRES] COMMIT ... weapon=1 weapons=0006 ammo0=10 ammo1=12 ... rollback=closed
```

The extinguisher disappeared from the world and became the selected weapon.

### Ammo and credits

Hardware witnesses include extinguisher recharge and credits:

```text
[PLAYERRES] PREPARE tile=782 sprite=181 defTile=81 type=6 subtype=0 parm=3 action=ammo value=10->13 ...
[PLAYERRES] COMMIT ... ammo0=13 ... rollback=closed

[PLAYERRES] PREPARE tile=783 sprite=191 defTile=95 type=3 subtype=22 parm=1 action=credits value=0->1 ...
[PLAYERRES] COMMIT ... credits=1 ... rollback=closed
```

Both disappeared physically after pickup.

## HUD and shared PlayerState projection

`EspNativeGameplayHud_view()` now projects health, armor, selected weapon and the
selected weapon's ammo directly from `EspNativeGameplayPlayerState`.

This removes the old split where the weapon-only pickup owner could change a
presentation selection while gameplay resources lived elsewhere.

The player owner remains 52 B and is shared by:

```text
combat stats
HP / armor
XP / level
keys / credits
ammo[6]
inventory[5]
weapon ownership / selected weapon
```

## Extinguisher ammo transaction — hardware PASS

Fire removal now consumes the same PlayerState ammo transactionally. Real-CYD
witness:

```text
[ACTIONENGINE] ARM seq=76 sprite=74 effect=fire-remove ... ammoUsage=1-pending ... rollback=armed
[ACTIONENGINE] FIRE-COMMIT seq=76 sprite=74 ammoType=0 ammo=10->9 playerFNV=a6e115a7->15cb16e4 xp=2-deferred sound=5045-deferred turnAdvance=deferred rollback=closed
```

The HUD count decremented physically when the fire was extinguished. A failed
redraw restores both ammo and the fire overlay.

The historical fire +2 XP remains deferred; that old consequence has not yet
been migrated into PlayerState progression.

## Generic monster presentation repairs validated on this branch

This milestone also fixed projection bugs exposed while validating the shared
resource/render path. These are generic renderer/combat ownership fixes, not
per-monster handlers.

### Pain and ordinary death

An alive monster hit uses visual state 6 for 250 ms and returns to its normal
state. A normal non-gib death uses visual state 4 for 250 ms and then settles to
stable corpse visual state 2 while remaining unlinked.

### Generic overkill/gib family

The recovered legacy overkill rule is shared by all ordinary monsters. A gib
kill has no corpse: after the death pose the monster becomes hidden and is
replaced by a bounded native gib effect.

The FX is presentation-only:

```text
owner = 148 B
particles = bounded local burst
chunks = 5
lease = 350 ms
visual RNG = local xorshift
gameplay RNG = untouched
legacy ParticleSystem = not owned
```

Real-CYD Hellhound witness at the hardware boundary:

```text
[MONSTERCOMBAT] DEATH-SETTLE sprite=179 visual=4->hidden delayMs=250 gib=1 gibFX=deferred immutableSprite=yes
[MONSTERCOMBAT] COMMIT ... alive=1->0 ... visual=death4->gib-hidden/250ms+unlink,gibFX-deferred ... rollback=closed
[GIBFX] PAINT sprite=179 subtype=1 particles=17 chunks=5 pixels=86 center=80,52 ownerBytes=148 leaseMs=350 visualRng=local gameplayRng=untouched legacyParticleSystem=no
[GIBFX] EXPIRE sprite=179 leaseMs=350 frame=1426a13d presented=1 restored=world-redraw gameplayRng=untouched
```

The user confirmed the blood burst disappears automatically without movement or
touch input.

A Zombie subtype 0 was also hardware-tested through the same gib route on the
preceding code boundary, proving the path is not Hellhound-specific.

## Hardware memory witness

Representative final runtime after combat/resource ownership:

```text
heap8 = 19788 B
largest8 = 14324 B
shapeData = NULL
mediaTexels = NULL
PSRAM = none
```

The selected resident asset cache remains unchanged at 23592 B owner / 19456 B
payload.

## Explicitly deferred families

This milestone does not claim complete gameplay consequences. Still deferred:

```text
pickup sounds/messages/got-face presentation
generic combat MISS/HIT/CRIT text feedback
fire +2 XP and jammed-door +1 XP migration into PlayerState
materialized monster drops
corpse-pile trimming
enemy AI / retaliation / turn advancement
actual sound playback
chaingun/plasma multi-loop presentation/commit
rocket/BFG radius damage
special death consequences for subtypes 7, 8, 12, 13
Kronos-specific semantics
```

These are mechanical family boundaries. Do not create per-item or per-monster
mini-milestones.

## Merge rule

The code boundary is locked at `8152cf8d233067a44b2d705edcaf315845b7744a`.
After this hardware PASS, only documentation commits are permitted on this
branch. After merge, recover the exact new `main` SHA before starting the next
`agent/*` family.
