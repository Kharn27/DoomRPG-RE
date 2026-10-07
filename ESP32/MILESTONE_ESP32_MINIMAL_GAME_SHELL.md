# Milestone — minimal ESP32 Game compatibility shell

Status: **REAL-CYD PASS**

Hardware-tested code boundary:
`44ecee201cbfec6de72150999d94060c231585e5`

Branch:
`agent/esp32-compact-game-entity-storage`

Previous accepted Game-TU boundary:
`e497199152be23b86d98054ca458761f04109b9d`

## Goal

Remove the remaining inherited Game world/runtime fields from the ESP32
structure once the desktop `Game.c` translation unit had been retired.

## Result

Desktop layout remains unchanged at 36,468 B.

ESP32 layout is now:

```c
typedef struct Game_s
{
    struct DoomRPG_s* doomRpg;
} Game_t;
```

Exact structural reduction:

```text
desktop Game_t: 36468 B
ESP32 Game_t:       4 B
reclaimed:      36464 B
```

The heap allocation is 20 B on the tested allocator, matching the 4-byte
payload plus allocator overhead.

## Compile-surface retirement

The reduction was intentionally fail-closed. The first compile exposed three
remaining source dependencies; they were removed explicitly rather than
reintroducing fields.

The final generated ESP32 source path:

- prunes 14 dead DoomCanvas functions whose only relevance was inherited Game
  field access;
- replaces the two retained DoomCanvas Game scalar reads with the constant
  state implied by the already-retired legacy producers;
- removes the Render legacy monster activation block, because native monster
  activation is authoritative and `Game_activate()` is a stale no-op ABI;
- redirects the inherited Render map-file lookup to `EspMapCatalog`;
- removes inherited DoomRPG writes/clears against Game fields that no longer
  exist.

The compatibility bridge still exports four ABI names, while link-time GC may
retain fewer depending on the current call graph.

## CI

Normal `esp32-cyd` CI #1622: **SUCCESS**.

```text
RAM:   45464 B
Flash: 780189 B
```

## Real-CYD acceptance

Boot:

```text
Engine structs: Render=5040 Game=4 Canvas=3740 Total=9544 bytes
[CORE] Game           used=20 heap=177504 largest=110580
[CORE] Legacy Game shell minimal gameBytes=4 desktopBytes=36468 totalReclaimed=36464 fields=doomRpg-only worldOwner=native
[CONFIGMAP] READY config path exercised and mappings resident
[MAINBOOT] READY owner=native-opaque ...
```

Fresh START:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap8=99708 largest8=86004 ...
```

The fresh session then successfully exercises:

- movement/rotation/strafe;
- crate subtype-2 transform to armor shard;
- pickup commit;
- multiple dialog events and continuation via opcode 19;
- door open, deferred close across monster-turn boundary;
- HUB/SYS and confirmed Exit To Menu.

Exit proves native teardown remains authoritative even though `Game_t` no
longer owns any world/session storage:

```text
[RESIDENTRESET] ... released=18008 ... empty=1
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty ...
```

The same firmware then loads a version-11 checkpoint. Resources, script,
lines/textures, action removals, crate transforms, automap, monster state,
topology, position, activation, and drops are restored; resident gameplay
restarts with 12 active monsters and remains operational. Another crate
transform/pickup after resume succeeds.

Checkpoint-resume settles at:

```text
heap8=98672
largest8=86004
```

The existing renderer compact guard is observed and recovers normally.

## Invariants

- `shapeData == NULL`;
- `mediaTexels == NULL`;
- PAK-backed runtime only;
- no runtime ZIP;
- `doomRpg->player == NULL`;
- no desktop Game/Entity/EntityMonster/Combat/Player runtime ownership;
- all map/world/entity/script/gameplay mutation remains in native owners.

## Next boundary

The principal inherited compile surface now blocking further structural cleanup
is the generated full desktop `DoomCanvas.c`. Only a small linked subset of
its ABI is still required. The next milestone should replace that full
translation unit with an explicit ESP32 bridge/owner after auditing the exact
linked `DoomCanvas_*` symbol set.

## Closure

Hardware-tested code ends at
`44ecee201cbfec6de72150999d94060c231585e5`.

The closure commit is documentation-only.
