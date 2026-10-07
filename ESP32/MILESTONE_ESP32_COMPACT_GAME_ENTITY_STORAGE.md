# Milestone — compact dormant Game entity storage

Status: **REAL-CYD PASS**

Hardware-tested code boundary:
`449e9e8425e6a4d125804728052c2b4735e31b97`

Branch:
`agent/esp32-compact-game-entity-storage`

Base main:
`8bd04772f420c8d81d45ab1c095866a9342ef91b`

## Audit

The merged main was audited from its normal `esp32-cyd` ELF, not only from
source references. Only five `Game_*` symbols survive final linking:

- `Game_init`;
- `Game_gsprite_clear`;
- `Game_loadConfig`;
- `Game_unloadMapData`;
- `Game_activate`, already a 5-byte fail-closed ABI stub.

The rest of the desktop Game implementation is currently discarded at link
time. Native startup still consumes a narrow subset of the `Game_t` shell:
`doomRpg`, `mapFiles[]`, entity/monster zero-state witnesses, a few retained
DoomCanvas compatibility scalars, and the bounded `gsprites[16]` state.

Before this milestone, `sizeof(Game_t) == 36468`. Its retired entity stores
accounted for 35296 B:

```text
entities[400]        25600 B
entityDb[1024]        4096 B
entityMonsters[100]   5600 B
```

## Boundary

This milestone does **not** remove `Game_t` and does not migrate config,
map-name/transition or remaining compatibility scalars.

For `DOOMRPG_ESP32` only:

- `entities[]` capacity becomes 1;
- `entityDb[]` capacity becomes 1;
- `entityMonsters[]` capacity becomes 1;
- desktop builds retain 400 / 1024 / 100;
- generated `Game_unloadMapData()` replaces the dead 1024-entry entityDb clear
  with a single sentinel clear.

The one-element arrays are compile-only compatibility sentinels for unlinked
legacy functions that still parse against historical field names. They are not
runtime entity owners.

Static guards require:

```text
GAME_LEGACY_ENTITY_CAPACITY == 1
GAME_LEGACY_ENTITY_DB_CAPACITY == 1
GAME_LEGACY_MONSTER_CAPACITY == 1
sizeof(Game_t) == 1296
```

## CI / ELF

GitHub Actions normal `esp32-cyd` run #1602: **SUCCESS**.

```text
RAM:   45464 B
Flash: 781141 B
```

The preceding merged-main CI #1600 reported:

```text
RAM:   45464 B
Flash: 781485 B
```

Static RAM is unchanged because `Game_t` is heap allocated. Linked Flash drops
344 B. The final ELF retains the same five `Game_*` roots as main; no retired
Entity/EntityMonster/Player/Combat translation unit was restored.

## Real-CYD acceptance

Core allocation:

```text
[CORE] Game           used=1312 heap=176212 largest=110580
[CORE] Legacy entity runtime retired stores=sentinel capacities=1/1/1 gameBytes=1296 desktopBytes=36468 reclaimed=35172 entities=0 monsters=0 owner=native-resident-map
[CORE] READY objects=5 heap used=11016 remaining=176212 largest=110580 clip=160x120
```

The preceding hardware boundary reported `Game used=36484` and core remaining
heap `141040`. The observed improvement is exactly 35172 B, matching the
structural reduction from 36468 B to 1296 B plus unchanged allocator overhead.

Startup/config/mappings remain healthy:

```text
[CONFIG] Config file present=no (missing is valid on first boot)
[CONFIG] DONE heap delta=0 heap8=147908 largest8=110580
[MAPPINGS] INSTALLED payload=8376 heap8=139468 largest8=110580 scratch=framebuffer noInflatedHeap=yes
```

Fresh START reaches the exact resident gameplay witness:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY map=1 angle=64 residentCache=yes largeCache=yes touch=invisible-120ms TURN+MOVE=armed shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap8=98416 largest8=86004 ...
```

The run then exercises movement, two native Armor Shard pickups, HUB/SYS and
confirmed Exit To Menu:

```text
[RESIDENTRESET] heap8=126304->144312 released=18008 ... empty=1
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty saveWrite=no checkpoint=unchanged
```

A second START begins with the mutated native player fingerprint
`363261d1`, resets it to canonical fresh `e745fce9`, traverses intro and
resident loading again, and settles at the exact same resident memory state:

```text
[MAINSTART] Native player before stateFNV=363261d1 legacyPlayer=0x0 owner=native-gameplay-player-state
[PLAYERSTATE] READY ... stateFNV=e745fce9 legacyPlayer=no ...
...
[ALIVE] ... heap8=98416 largest8=86004 ...
```

This is the leak/lifecycle witness for the exercised START -> gameplay -> EXIT ->
START cycle.

One existing native renderer safety path was also exercised:

```text
[NATIVEFRAME] LEGACY_GUARD ...
[NATIVEFRAME] RETRY ...
[NATIVEFRAME] RECOVERED ...
```

The guard recovered as designed and gameplay immediately continued through a
committed turn and pickups. No failure is attributed to the Game storage
compaction.

## Invariants

The hardware run preserves:

- `shapeData == NULL`;
- `mediaTexels == NULL`;
- PAK-backed runtime;
- no runtime ZIP dependency;
- `doomRpg->player == NULL`;
- legacy entity/monster owners absent;
- native resident-map/gameplay state authoritative.

## Deferred

`Game_t` itself remains intentionally alive. Remaining compatibility
responsibilities must be split by a later audited milestone rather than removed
wholesale.

The valid-Config-file branch remains structurally guarded but was not exercised
on this hardware run because no Config file was present.

## Closure

Hardware-tested code ends at
`449e9e8425e6a4d125804728052c2b4735e31b97`.

The closure commit updates documentation only.
