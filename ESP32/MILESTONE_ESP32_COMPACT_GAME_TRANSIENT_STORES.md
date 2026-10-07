# Milestone — compact dormant Game transient stores

Status: **REAL-CYD PASS**

Hardware-tested code boundary:
`233904e1b27c03554166f0514bbf34465234dcb4`

Branch:
`agent/esp32-compact-game-entity-storage`

Previous tested boundary:
`46b463a6c240f5e7fadd02d3aeb3dc83d65a620e`

## Audit

The exact normal ESP32 ELF retained only four `Game_*` symbols after LTO:
`Game_init`, `Game_loadConfig`, `Game_unloadMapData`, and the five-byte
fail-closed `Game_activate`. Legacy producers/consumers of the two transient
desktop buffers are absent:

- `Game_trace()` is not linked; native collision/action tracing is authoritative;
- `Game_gsprite_allocAnim()`, `Game_gsprite_alloc()`, and
  `Game_gsprite_update()` are not linked; native gameplay FX/render overlays
  are authoritative.

The inherited shell still contained:

```text
traceEntities[8]                 32 B
gsprites[MAX_CUSTOM_SPRITES=16] 320 B
```

One element of each remains as compile-only compatibility storage, so the exact
struct reduction is 28 + 300 = 328 B.

## Boundary

On `DOOMRPG_ESP32` only:

- trace capacity is 1 instead of 8;
- GameSprite capacity is 1 instead of 16;
- desktop capacities remain unchanged;
- generated `Game_gsprite_clear()` clears only the sentinel plus
  `activeSprites/f684l`;
- generated `Game_unloadMapData()` clears only the trace sentinel and
  `numTraceEntities`.

The small scalars `activeSprites`, `f684l`, and `monstersTurn` are
deliberately retained because retained DoomCanvas state code still observes
them.

Compile-time guards require both new capacities to remain 1 and
`sizeof(Game_t) == 440`.

## CI

Normal `esp32-cyd` CI #1609: **SUCCESS**.

```text
RAM:   45464 B
Flash: 780817 B
```

Generator witness:

```text
... 1 legacy GameSprite clear compacted + 1 legacy trace clear compacted
```

## Real-CYD acceptance

Boot:

```text
Engine structs: Render=5040 Game=440 Canvas=3740 Total=9980 bytes
[CORE] Game           used=456 heap=177068 largest=110580
[CORE] Legacy Game transient stores retired trace/gsprites=sentinel capacities=1/1 gameBytes=440 desktopBytes=36468 totalReclaimed=36028 owner=native-collision+gameplay-fx
[CORE] READY objects=5 heap used=10160 remaining=177068 largest=110580 clip=160x120
```

This is exactly 328 B less than the previous 768-byte tested layout.

Native startup and gameplay remain exact:

```text
[NATIVEBOOT] RESIDENT map=1 file=/intro.bsp ...
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap=165200 heap8=99276 largest8=86004 ...
```

The first session exercises movement, turning, two Armor Shard pickups, a
regular door, HUB/SYS and confirmed Exit To Menu:

```text
[RESIDENTRESET] heap8=127164->145172 released=18008 ... empty=1
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty ...
```

The second START resets player state from `363261d1` to `e745fce9`,
re-enters the same resident map and settles at the same
`heap8=99276/largest8=86004`. It then exercises native crate combat/removal
and additional movement/strafe/turn paths without regression.

## Invariants

- `shapeData == NULL`;
- `mediaTexels == NULL`;
- PAK-backed runtime;
- no runtime ZIP dependency;
- `doomRpg->player == NULL`;
- legacy Entity/EntityMonster stores remain sentinel-only;
- trace ownership is native;
- gameplay/render FX ownership is native.

## Closure

Hardware-tested code ends at
`233904e1b27c03554166f0514bbf34465234dcb4`.

The closure commit updates documentation only.
