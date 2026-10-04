# ESP32 legacy Combat retirement milestone

Date: 2026-10-04

Branch:
`agent/esp32-retire-legacy-combat`

Base main:
`72ec1b2307685dc83b344db8584b0646cde27e47`

Hardware-tested code boundary:
`766d1e0cf2280d784f2c073f552e3f57889dc541`

## Goal

Retire the inherited desktop `Combat_t` object and its translation unit from
the normal ESP32 runtime without changing native combat behavior.

This milestone does not redesign combat. Player attacks, monster attacks,
retaliation, damage math, weapon presentation, hit/death FX and monster-turn
ordering were already native owners. The work removes the remaining desktop
allocation/link roots around them.

## Pre-milestone link boundary

Inspection of the merged-main ELF showed that nearly all of `Combat.c` was
already dead. Only these desktop symbols remained linked:

```text
Combat_init
Combat_free
Combat_drawWeapon
```

No legacy player/monster sequence, projectile, damage, particle or attack
execution function from `Combat.c` was in the normal ELF.

`Combat_t` itself is 1036 bytes on the ESP32 ABI and was still allocated in
the startup core object graph.

The native first-person weapon path was already independent:
`EspNativeGameplayWeapon_render()` owns the original idle/attack offsets,
bounded PAK reads, one reusable decode/cache workspace and the one-shot attack
pose. It does not consume `Combat_t`.

## Final ownership change

At the final code boundary:

- `DoomRPG_initEngineCore()` no longer calls `Combat_init()`;
- `doomRpg->combat` must remain `NULL`;
- `Combat.c` is excluded from the normal ESP32 build;
- desktop `Weapon.c` is also excluded because no desktop `Weapon_*` symbol
  remains reachable;
- generated ESP32 `DoomRPG.c` no longer retains `Combat_free()`;
- generated ESP32 `DoomCanvas.c` retires the old
  `Combat_drawWeapon()` call because native resident rendering already owns
  first-person weapon presentation.

The final ELF contains zero `Combat_*` and zero desktop `Weapon_*` symbols.

`CombatEntity_*` is deliberately not part of this retirement. `Player_t`
still embeds `CombatEntity_t ce` for legacy-compatible player stats; that
smaller dependency belongs to the later Player cleanup.

## Hardware failure and correction

The first retirement build reached MENU_MAIN successfully with:

```text
[CORE] Combat retired object=NULL owners=native-combat+weapon+monster-turn
```

but START crashed immediately after the menu start route:

```text
Guru Meditation Error: Core 1 panic'ed (StoreProhibited)
EXCVADDR: 0x00000004
```

The exact firmware backtrace symbolized to:

```text
Game_unloadMapData
 -> DoomRPG_esp32ReleaseMainMenuMemory
 -> DoomRPG_esp32ActivateMainMenuStart
```

`Game_unloadMapData()` still performed two legacy cleanup writes:

```c
game->doomRpg->combat->curTarget = NULL;
game->doomRpg->combat->curAttacker = NULL;
```

Those fields are not production state anymore; native combat owners reset their
own bounded state. The final patch generates an ESP32-only `Game.c` that
retires exactly these two writes and leaves the desktop source unchanged.

No dummy Combat object, compatibility allocation or nullable stub was
introduced.

## CI

Normal `esp32-cyd` CI #1575: SUCCESS.

```text
RAM:   13.9% (45480 / 327680 B)
Flash: 59.7% (782773 / 1310720 B)
artifact id: 11299309172
artifact sha256: f0d066fdfa7535ae875e1a9ff06269656103ff5b3952ae6a05ee85378a1d18ab
```

Merged-main baseline at
`72ec1b2307685dc83b344db8584b0646cde27e47` was:

```text
RAM:   45488 B
Flash: 784573 B
```

So the linked image loses 8 B static RAM and 1800 B Flash. The principal
runtime gain is the retired 1036-byte `Combat_t` heap allocation.

No local PlatformIO build is claimed.

## Real-CYD acceptance

The final hardware build passes START, reaches resident MAP_INTRO and remains
stable. A real crate attack provides a bounded end-to-end player attack witness:

```text
[ACTIONENGINE] TRACE seq=3 weapon=2 distance=1 tile=873 target=sprite index=127 line=65535 type=12 subtype=2 route=CRATE_SUBTYPE2
[CRATE] ARM seq=3 ... weapon=2 ... ammoType=1 ammoUsage=1 loops=1 ...
[CRATE] CONSEQUENCE seq=3 ... outcome=TRANSFORM ... attackDamage=6 attackArmorDamage=4 ...
[MONSTERTURN] ATTACK-REQUEST seq=3 source=explicit-native-player-attack rollback=available-until-cancel
[CRATE] COMMIT seq=3 ... ammo=8->7 ... turnAdvance=PLAYER_ATTACK-requested rollback=closed
[ACTIONENGINE] ATTACK seq=3 weapon=2 frame=1->0 generic=yes worldCommitted=yes
[MONSTERTURN] ORDERED-DISPATCH reason=PLAYER_ATTACK turnToken=2 activeCount=0 ...
[MONSTERACTIVESEQ] BEGIN turn=2 reason=3 activeCount=0 ...
```

This proves on hardware, with `doomRpg->combat == NULL`:

- START/menu teardown no longer dereferences the retired object;
- resident gameplay renders and accepts input;
- native first-person weapon attack pose completes;
- ammo consumption is committed;
- generic player combat math executes;
- crate consequence RNG/world mutation executes;
- the semantic player-attack monster turn is published exactly once.

The submitted witness has `activeCount=0`, so this specific test does not
claim an active monster attack in the same run. Monster combat/retaliation code
was not changed by this milestone and remains owned by the already
hardware-validated native subsystems.

Stable hardware witness:

```text
heap=123436 heap8=57512 largest8=51188
```

A previous comparable early resident-gameplay witness before Combat retirement
was:

```text
heap=122376 heap8=56452 largest8=51188
```

The +1060 B free-heap difference is consistent with removing the 1036-byte
`Combat_t` allocation plus allocator overhead. It is recorded as supporting
evidence rather than exact allocator attribution.

## Architecture invariants

The milestone preserves:

```text
doomRpg->combat == NULL by construction
shapeData == NULL
mediaTexels == NULL
runtime assets = DoomRPG-ESP32.pak
player combat = native
monster combat / retaliation = native
weapon presentation = native PAK-backed renderer
monster-turn ordering = native
no runtime ZIP fallback
no desktop Combat compatibility allocation
```

## Closure

Hardware-tested code boundary:
`766d1e0cf2280d784f2c073f552e3f57889dc541`.

Post-test closure is documentation-only.
