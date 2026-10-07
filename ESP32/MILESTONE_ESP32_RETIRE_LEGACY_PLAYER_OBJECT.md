# Milestone — retire legacy Player object

Status: **REAL-CYD PASS**

Hardware-tested code boundary:
`9b47b4c141232bc75646d25d04d8b7adf6ecffc2`

Branch:
`fix/mainMenu`

## Boundary

This milestone retires the inherited desktop `Player_t` object from the
normal classic-CYD runtime without expanding scope into `Game_t`.

Permanent ownership after the boundary:

- `doomRpg->player == NULL`;
- `EspNativeGameplayPlayerState` is the sole authoritative player state;
- START resets that native owner through
  `EspNativeGameplayPlayerState_resetFresh()`;
- `Player.c` is excluded from the ESP32 build;
- `CombatEntity.c` is excluded with it because its remaining production owner
  was the legacy Player stats container;
- generated `Game_unloadMapData()` no longer dereferences
  `player->facingEntity` / `player->dogFamiliar`;
- generated `DoomRPG_FreeAppData()` no longer owns/frees `Player_t`;
- the inherited ST_DYING compatibility path no longer writes Player weapon
  fields.

No desktop Player shim or replacement allocation was introduced.

## Hardware acceptance

The first START on the real CYD establishes the canonical native fresh state:

```text
[PLAYERSTATE] READY bytes=52 level=1 xp=0/80 hp=30/30 armor=0/20 def=16 str=12 agi=14 acc=16 ammo1=8 weapon=2 weapons=0004 stateFNV=e745fce9 legacyPlayer=no
[MAINSTART] READY native new-game -> PlayerState_resetFresh -> ST_INTRO legacyPlayer=NULL
```

The run then reaches resident MAP_INTRO gameplay and exercises dialogs, doors,
fire clearing, pickups, HUB pages, secret XP, monster activation, committed
retaliation, live monster movement and a native player kill/gib path.

The decisive ownership test is a second fresh START after mutating gameplay and
returning through SYS Exit To Menu. Before reset:

```text
[MAINSTART] Native player before stateFNV=4745097e legacyPlayer=0x0 owner=native-gameplay-player-state
```

After reset:

```text
[PLAYERSTATE] READY ... hp=30/30 armor=0/20 ... weapon=2 weapons=0004 stateFNV=e745fce9 legacyPlayer=no
```

Therefore the actual gameplay owner, not a dead desktop mirror, is reset for a
new game.

## Memory witness

Exit teardown returns to an empty resident session:

```text
[RESIDENTRESET] heap8=87668->105684 released=18016 ... empty=1
[SYSEXIT] MENU-READY ... session=off resident=empty ...
```

The repeated-start gameplay settles at `heap8=59780`, matching the prior
gameplay total. The largest free block falls from 51188 to 36852 on the second
resident cycle. This is still comfortably above the 16384 reserve target and
is tracked as allocator fragmentation to watch over repeated cycles.

## Post-review Config compatibility correction

Code boundary:
`084b0c0345cd38ed2093d6c08faf5db65d9a60e9`

A P1 review correctly identified that `Game_loadConfig()` still assigned the
legacy Config `totalDeaths` field through `doomRpg->player`. With the retired
Player contract, an existing compatible Config would therefore null-dereference
during startup. The same audit found stale Player/Sound dereferences in
`Game_saveConfig()`.

The ESP32 generator now consumes the retired load fields without dereference and
writes zero placeholders on legacy save paths, preserving the Config binary
layout. It additionally rejects generated `Game.c` if either retired field
access survives.

The corrected firmware was booted on the real CYD through config/mappings
startup. The board had no Config file, so compatible-Config parsing itself is
not claimed as hardware-exercised; the null dereferences are structurally absent
and guarded at generation time.

## Next architectural boundary

`Game_t` remains intentionally resident. The next desktop-disengagement work
should first sever its legacy entity/monster initialization and cleanup closure
(`Entity.c` / `EntityMonster.c`) or, if the dependency audit proves bounded,
retire the Game object itself. The hardware-tested Player boundary must not be
expanded retroactively.
