# Milestone — retire legacy Game map string tables

Status: **REAL-CYD PASS**

Hardware-tested code boundary:
`46b463a6c240f5e7fadd02d3aeb3dc83d65a620e`

Branch:
`agent/esp32-compact-game-entity-storage`

Previous tested boundary:
`449e9e8425e6a4d125804728052c2b4735e31b97`

## Ownership audit

After compacting the dormant entity stores, `Game_t` was 1296 B. Two mutable
desktop lookup tables still occupied 576 B:

```text
mapNames[MAPNAME_MAX][24] = 11 * 24 = 264 B
mapFiles[MAPFILE_MAX][24] = 13 * 24 = 312 B
```

ESP32 already had a permanent immutable map owner:
`EspMapCatalog`. Native map transitions used it directly. The remaining native
bootstrap edge redundantly read `game->mapFiles[startupMap - 1]` only to map
that string back into an `EspMapCatalog` ID.

## Boundary

For `DOOMRPG_ESP32` only:

- `Game_t::mapNames` capacity is 1;
- `Game_t::mapFiles` capacity is 1;
- desktop/J2ME retains `MAPNAME_MAX` / `MAPFILE_MAX`;
- `Game_init()` no longer populates the desktop string tables;
- `Game_getResourceMapID()` resolves through `EspMapCatalog_idForName()`;
- the legacy save-state resource-name edge resolves through
  `EspMapCatalog_nameForId()`;
- native START/bootstrap resolves `startupMap` directly through
  `EspMapCatalog_nameForId()`;
- generated `Game.c` fails closed if direct `game->mapNames[` or
  `game->mapFiles[` access survives.

Compile-time guards require both sentinel capacities to stay 1 and
`sizeof(Game_t) == 768`.

## CI

Normal `esp32-cyd` CI #1605: **SUCCESS**.

```text
RAM:   45464 B
Flash: 780721 B
```

The previous code boundary reported 781141 B Flash, so this milestone also
removes 420 B of linked Flash while static RAM remains unchanged. The heap gain
is runtime because `Game_t` is dynamically allocated.

## Real-CYD acceptance

Boot reports the exact new layout:

```text
Engine structs: Render=5040 Game=768 Canvas=3740 Total=10308 bytes
[CORE] Game           used=784 heap=176740 largest=110580
[CORE] Legacy entity runtime retired stores=sentinel capacities=1/1/1 reclaimed=35172 entities=0 monsters=0 owner=native-resident-map
[CORE] Legacy Game map tables retired stores=sentinel capacities=1/1 gameBytes=768 desktopBytes=36468 totalReclaimed=35700 owner=EspMapCatalog
[CORE] READY objects=5 heap used=10488 remaining=176740 largest=110580 clip=160x120
```

This is exactly 528 B less `Game_t` than the preceding tested 1296 B layout.

The normal boot reaches MENU_MAIN at:

```text
[MAINOPAQUE] ... heap8=140000 largest8=110580
[ALIVE] ... heap8=140000 largest8=110580 ...
```

START then traverses the entire intro and catalog-backed resident load:

```text
[NATIVEBOOT] LOADING-TAKEOVER map=1 source=intro-disposed owner=transition-presentation
[BSPREAD] ENTRY /intro.bsp ...
[NATIVEBOOT] RESIDENT map=1 file=/intro.bsp ...
[NATIVEBOOT] READY map=1 ... shapeData=0x0 mediaTexels=0x0
```

The exact native first-frame witness remains unchanged:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
```

Settled gameplay:

```text
[RAMBUDGET] LAZY_POST heap=99760 heap8=99760 largest8=86004 ...
[ALIVE] ... heap=164872 heap8=98948 largest8=86004 ...
```

The run exercises movement, two Armor Shard pickups, a regular door,
HUB/SYS and confirmed Exit To Menu:

```text
[RESIDENTRESET] heap8=126836->144844 released=18008 ... empty=1
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty saveWrite=no checkpoint=unchanged
```

The second START sees `stateFNV=363261d1`, resets to
`stateFNV=e745fce9`, loads `/intro.bsp` through the same native catalog path
and again settles at:

```text
[ALIVE] ... heap=164872 heap8=98948 largest8=86004 ...
```

This provides the repeated-session leak/lifecycle witness.

## Invariants

The hardware run preserves:

- `shapeData == NULL`;
- `mediaTexels == NULL`;
- PAK-backed runtime;
- no runtime ZIP dependency;
- `doomRpg->player == NULL`;
- legacy entity/monster runtime absent;
- map identity/resource strings owned by immutable `EspMapCatalog`.

## Deferred

The remaining 768 B `Game_t` shell is still retained. It must be audited by
field family before further compaction; this milestone intentionally does not
remove generic compatibility scalars or custom-sprite state.

## Closure

Hardware-tested code ends at
`46b463a6c240f5e7fadd02d3aeb3dc83d65a620e`.

The closure commit updates documentation only.
