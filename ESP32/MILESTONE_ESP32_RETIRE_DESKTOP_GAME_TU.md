# Milestone — retire desktop Game translation unit

Status: **REAL-CYD PASS**

Hardware-tested code boundary:
`e497199152be23b86d98054ca458761f04109b9d`

Branch:
`agent/esp32-compact-game-entity-storage`

Previous tested boundary:
`233904e1b27c03554166f0514bbf34465234dcb4`

## Motivation

After compacting the inherited entity/map/transient stores, the normal ESP32
ELF retained only four `Game_*` roots. Continuing to generate the entire
desktop `Game.c` merely to let link-time GC discard the rest kept a large
legacy compile surface alive and allowed accidental future dependencies to
re-enter silently.

## Boundary

Desktop `src/Game.c` remains the behavioral reference but is no longer built
for ESP32.

`ESP32/src/esp_legacy_game_bridge.c` permanently owns exactly:

- `Game_init()`: allocate/zero the compatibility shell and bind DoomRPG;
- `Game_loadConfig()`: preserve Config v23 field ordering while consuming
  retired Sound/Player fields without dereferencing retired owners;
- `Game_unloadMapData()`: fail-closed compatibility cleanup while native
  resident/session owners perform real teardown;
- `Game_activate()`: stale ABI no-op; native monster activation is authoritative.

A new inherited `Game_*` dependency therefore becomes a link error and
requires an explicit migration milestone.

The `Game_t` layout is intentionally unchanged at 440 B in this milestone.

## CI

Normal `esp32-cyd` CI #1615: **SUCCESS**.

```text
[ESP32] Desktop Game.c retired; esp_legacy_game_bridge.c owns Game_init/Game_loadConfig/Game_unloadMapData/Game_activate
RAM:   45464 B
Flash: 780881 B
```

The exact build retains only the four bridge roots.

## Real-CYD acceptance

The shortened boot witness covers the config owner:

```text
[CONFIG] -> Game_loadConfig()
loadConfig
loadConfig: (unable to open file)
[CONFIG] DONE heap delta=0 heap8=148768 largest8=110580
[CONFIGMAP] READY config path exercised and mappings resident
```

Fresh START remains exact:

```text
[NATIVEBOOT] RESIDENT map=1 file=/intro.bsp ...
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap8=99276 largest8=86004 ...
```

The run also exercises native crate transformation, pickup, dialog continuation,
HUB/SYS and confirmed Exit To Menu. That EXIT reaches resident reset and
MENU_MAIN, exercising the bridge teardown path without regression.

The same firmware then exercises the dedicated LOAD card and restores a
version-11 checkpoint exactly enough to resume resident gameplay:

```text
[NATIVESAVE] LOAD ... version=11 ...
[ENGINESESSION] RESUME checkpoint=restored ...
[ENGINESESSION] READY map=1 angle=0 ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap8=98240 largest8=86004 ...
```

## Invariants

- `shapeData == NULL`;
- `mediaTexels == NULL`;
- PAK-backed runtime;
- no runtime ZIP dependency;
- `doomRpg->player == NULL`;
- no desktop Entity/EntityMonster/Combat/Player/Game translation unit revival;
- Game world/entity/script/gameplay ownership remains native.

## Next boundary

The remaining 440-byte `Game_t` shell is now primarily constrained by the
full inherited `DoomCanvas.c` compile surface. The next milestone should first
reduce/replace that desktop translation unit while preserving the actually
linked DoomCanvas ABI, then shrink `Game_t` from evidence rather than source
dead-code assumptions.

## Closure

Hardware-tested code ends at
`e497199152be23b86d98054ca458761f04109b9d`.

The closure commit updates documentation only.
