# ESP32 MAP_INTRO native SHOW/HIDE sprite-topology milestone

Branch: `agent/esp32-map1-native-show-hide-topology`

Base merged `main`:

```text
PR   = #61 — native EV_CHANGEMAP pending transition intent
main = fc39ac60757e0d992e3729a5044a9d83e9994971
```

Firmware-bearing commit is the direct parent of the first documentation commit and is recorded exactly below after staging.

Status: **IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING**.

## Objective

Own the final two real MAP_INTRO opcode families together:

```text
7  EV_SHOW
18 EV_HIDE
```

They share one legacy sprite/entity tile topology and therefore belong in one bounded milestone. They are not simple visibility bits.

This milestone introduces a compact ESP32-native map-sprite/entity-topology owner and executes the directly owned topology/visual consequences of SHOW/HIDE without constructing the legacy `Game.entities[400]`, `entityDb[1024]`, `Entity_t` pointer chains or mutable legacy `Render.mapSprites`.

`shapeData` and `mediaTexels` remain NULL. Legacy `Render` map runtime remains clear. `entities=0`, `monsters=0` and `ST_PLAYING` remains unreachable in this probe stage.

## Exact legacy SHOW behavior

`Game_executeEvent()`:

```c
case EV_SHOW: {
    sprite = &render->mapSprites[arg1 & 65535];
    sprite->info = (sprite->info & 0xFFFEE1FF) |
                   (((arg1 >> 16) & 255) << 9);
    if (sprite->ent) {
        entity = Game_findMapEntityXYFlag(game, sprite->x, sprite->y, 4098);
        if (entity) Entity_died(entity);
        entity = Game_findMapEntityXYFlag(game, sprite->x, sprite->y, 4098);
        if (entity) Entity_died(entity);
        Game_linkEntity(game, sprite->ent, sprite->x >> 6, sprite->y >> 6);
    }
    break;
}
```

`4098 = (1 << enemyType) | (1 << destructibleType)`, so SHOW may ask the legacy gameplay layer to kill at most two blockers before linking the target entity.

The visual assignment clears the legacy hidden bit plus animation-state bits and applies the byte carried in `arg1 >> 16` while preserving unrelated visual bits.

### Important blocker boundary

`Entity_died()` is much larger than SHOW itself. Depending on type/subtype it may mutate XP, sound, HUD, drops, AI lists, secondary monsters, tile events, player state, particles and other gameplay state.

This milestone does **not** open those gameplay owners. It directly owns only the compact base topology consequences needed by SHOW:

```text
- target visual byte update
- deterministic blocker base unlink/death-state projection
- target link into the native tile topology
```

and returns `ESP_MAP_SHOW_EFFECT_DEFER_BLOCKER_GAMEPLAY` when a blocker death has non-topology gameplay consequences still deferred.

The known RNG crate branch (`eType=12,eSubType=2`) is rejected **before any mutation** with `ESP_MAP_SPRITE_TOPOLOGY_RANDOM_BLOCKER`; the probe fails if a real MAP_INTRO SHOW command requires that branch.

Repeated SHOW on an already-linked entity target is also refused fail-closed (`TARGET_ALREADY_LINKED`) rather than reproducing the corrupting legacy double-link behavior. A valid first SHOW remains handled.

## Exact legacy HIDE behavior

```c
case EV_HIDE: {
    entity = Game_findMapEntityXY(game,
                                  (arg1 & 255) << 6,
                                  ((arg1 >> 8) & 255) << 6);
    for (; entity != NULL; entity = entNext) {
        entNext = entity->nextOnTile;
        if ((entity->info & 0x200000) == 0 && entity->def->eType != 1) {
            render->mapSprites[(entity->info & 0xffff) - 1].info |= 0x10000;
            Game_unlinkEntity(game, entity);
        }
    }
    break;
}
```

Thus HIDE walks the tile chain in link order, leaves monsters and line entities linked, and hides/unlinks map-sprite entities. A second HIDE on the same tile is handled but produces zero additional mutation.

## Recovered initial map-sprite entity topology

Legacy `Game_loadMapEntities()` creates map-sprite entities in source sprite order:

```text
for each map sprite:
  if source info & 0x01000000:
      clear that source flag and skip initial entity creation
  else:
      lookup EntityDef by source tile index
      if special 0x00040000: lookup += 305
      if def exists OR source 0x00020000 fallback flag:
          create entity
          sprite->ent is assigned only when an EntityDef lookup succeeded
          apply recovered Entity_initspawn base flags
          if source hidden bit 0x00010000 is clear:
              Game_linkEntity(...)
```

`Game_linkEntity()` inserts at the head of the tile chain. Therefore source-order entities on one tile are traversed in reverse link order. The native owner preserves this using monotonically increasing 16-bit link-order values instead of pointers.

Fallback entities created from the source `0x00020000` flag intentionally exist in tile topology but have no `sprite->ent`, matching the legacy loader.

## Native `/entities.db` classification

Permanent owner build reads only `/entities.db` from `/DoomRPG-ESP32.pak` while the caller has the PAK open.

Recovered record format:

```text
uint16 tileIndex
uint8  eType
uint8  eSubType
int32  parm
char   name[16]
----------------
24 B / record
```

The build copies only the compact type/subtype classification required by topology. The PAK is closed before SHOW/HIDE execution; the executor performs no I/O.

No runtime ZIP read is introduced.

## Permanent compact owner

Files:

```text
ESP32/include/esp_map_sprite_topology.h
ESP32/src/esp_map_sprite_topology.c
```

For each of 344 map sprites the storage contains exactly:

```text
entity type       1 B
entity subtype    1 B
visual state      1 B
link state/tile   2 B
link order        2 B
---------------------
                  7 B
```

Expected payload:

```text
344 * 7 = 2408 B
```

There are no `Entity_t*`, `Sprite_t*`, linked-list pointers, monster objects or 1024-entry pointer database in this owner.

`link state` packs:

```text
bits 0..9  tile index
bit 10     linked
bit 11     legacy sprite->ent exists
bit 12     base alive/damageable projection
bit 13     map-sprite entity exists
```

The view also exposes counts and `nextLinkOrder` for strict hardware auditing.

Permanent calls:

```text
EspMapSpriteTopology_reset()
EspMapSpriteTopology_buildFromRuntime()
EspMapSpriteTopology_resetMutableFromRuntime()
EspMapSpriteTopology_isReady()
EspMapSpriteTopology_view()
EspMapSpriteTopology_getVisualState()
EspMapSpriteTopology_getEntity()
EspMapSpriteTopology_applyShow()
EspMapSpriteTopology_applyHide()
```

## Result ABIs

SHOW result:

```text
EspMapShowResult = 26 B expected
```

It records provenance, target sprite/tile, visual before/after, up to two blocker sprite indices, blocker counts, target entity/link state, deferred effects, handled return and `removeCommandIfHandled`.

HIDE result:

```text
EspMapHideResult = 18 B expected
```

It records provenance, tile coordinates/index, first/last hidden sprite, hidden entity count, effect flags, handled return and `removeCommandIfHandled`.

Valid SHOW/HIDE commands retain legacy `handled=true`; outer script-command removal remains metadata only and does not mutate `EspMapScriptState` in this milestone.

## Temporary real-CYD probe

Files:

```text
ESP32/include/native_map1_show_hide_probe.h
ESP32/src/native_map1_show_hide_probe.c
ESP32/src/native_map1_show_hide_probe_internal.h
ESP32/src/native_map1_show_hide_probe_support.c
ESP32/src/native_map1_show_hide_probe_corpus.c
```

The probe arms only after hardware-proven CHANGEMAP intent completion.

Inherited required native fingerprints:

```text
arenaFNV            = c3882516
mapStateFNV         = cd99b98e
scriptFNV           = f9e3d9df
lineStateFNV        = e5e74861
lineTextureStateFNV = f1fc1875
automapStateFNV     = 669b1aa7
```

Preconditions also require:

```text
/intro.bsp bytes=21823 crc32=623f34e4
mapSprites=344 events=93 byteCodes=265
ST_INTRO page=3
pack closed
legacy Render runtime clear
legacy entities=0 monsters=0
```

### Initial topology audit

After a bounded `/entities.db` read, the probe cross-checks every one of the 344 native classifications against the already-loaded legacy `EntityDefManager` **without creating any legacy map entities**.

Hardware will establish:

```text
entityDefCount
map-sprite entity count
EntityDef-backed count
fallback entity count
initial linked count
initial hidden map-sprite count
initial hidden entity count
enemy count
destructible count
nextLinkOrder
initial topologyFNV
```

### Complete real opcode corpus

All 93 events / 265 bytecodes are scanned.

For every real opcode 7 or 18:

```text
state-only executor must refuse UNSUPPORTED
canonical event descriptor provenance
native SHOW/HIDE executor only
exact handled/remove metadata
mutation fingerprint captured
owner reset to exact initial fingerprint afterwards
```

Hardware will establish rather than predeclare:

```text
refs / SHOW refs / HIDE refs
removable refs
SHOW mutated count
HIDE mutated / no-mutation counts
SHOW entity-target / no-entity-target counts
blockers found / removed / no-op
blocker deferred-gameplay count
HIDE hidden entities total
SHOW result FNV
HIDE result FNV
SHOW state aggregate FNV
HIDE state aggregate FNV
```

Acceptance requires at minimum:

```text
SHOW refs > 0
HIDE refs > 0
refs = SHOW + HIDE
stateExecRefused = refs
rollback = refs/refs
SHOW mutated > 0
HIDE mutated > 0
HIDE hidden entities total > 0
showResultBytes = 26
hideResultBytes = 18
```

### Repeat semantics

HIDE is proven idempotent:

```text
first HIDE  -> handled, mutation
second HIDE -> handled, hiddenEntityCount=0, exact state unchanged
```

SHOW repeat protection is deliberate:

```text
first valid SHOW on entity target -> handled, target linked
second identical SHOW             -> TARGET_ALREADY_LINKED, zero result/state mutation
```

For a SHOW target with no `sprite->ent`, a repeat may remain handled because no topology relink is attempted.

### Fail closed

Probe acceptance requires:

```text
unsupported=1
badOffset=1
badDescriptor=1
nullDescriptor=1
nullResult=1
randomCrate=guarded
targetRelink=guarded
stateAtomic=yes
```

## RAM target

Hardware-proven native persistent heap entering this final opcode-family milestone:

```text
immutable arena        14112 B
mutable tile state      1040 B
mutable script state     100 B
mutable line state       136 B
mutable texture state     76 B
mutable automap state    120 B
-----------------------------
total                  15584 B
```

Topology payload is exactly 2408 B. Probe accepts bounded allocator overhead:

```text
2408 B <= persistentHeapCost <= 2536 B
largest8 >= 32768 B
```

The exact allocator overhead and new persistent total are hardware values and remain pending.

The `/entities.db` PAK-open cost is transient and separately logged.

## Integrity boundary

After every corpus mutation rollback and at final PARK:

```text
topology FNV restored exactly
arena/map/script/line/texture/automap fingerprints unchanged
framebuffer unchanged
legacy notebook/keys/Hud/password/continuation unchanged
legacy 400-entity + 1024 entityDb topology witness unchanged
PAK closed
legacy Render runtime clear
legacy entities=0
legacy monsters=0
ST_PLAYING not reached
```

No `Entity_died()`, `Game_linkEntity()`, `Game_unlinkEntity()` or legacy `Render.mapSprites` mutation is executed by the native probe.

## Expected Serial family

```text
[MAPSHOWHIDEPROBE] ARMED ...

=== Doom RPG ESP32-native MAP_INTRO SHOW/HIDE sprite topology ===
[MAPSHOWHIDEPROBE] CONTRACT ...
[MAPTOPOLOGY] READY sprites=344 storageBytes=2408 defCount=... entities=... hasDef=... fallback=... linked=... hiddenSprites=... hiddenEntities=... enemies=... destructibles=... nextOrder=... stateFNV=...
[MAPSHOWHIDE] READY refs=... show=... hide=... removable=... stateExecRefused=... showMutated=... hideMutated=... hideNoMutation=... showTargetEnt=... showTargetNoEnt=... blockersFound=... blockersRemoved=... blockerNoops=... deferredDeaths=... hideEntities=... showResultBytes=26 hideResultBytes=18 showResultFNV=... hideResultFNV=... showStateFNV=... hideStateFNV=... elapsed=...ms
[MAPSHOW] SAMPLE ...
[MAPHIDE] SAMPLE ...
[MAPSHOWHIDE] STATE initialFNV=... rollback=.../... showRepeatGuard=1 hideIdempotent=1 reset=1 worldRestored=yes
[MAPSHOWHIDE] FAILCLOSED unsupported=1 badOffset=1 badDescriptor=1 nullDescriptor=1 nullResult=1 randomCrate=guarded targetRelink=guarded stateAtomic=yes
[MAPSHOWHIDEPROBE] IO entityDefs=/entities.db size=... crc32=... heapOpen=... transientPackCost=... largestOpen=... packIO=yes buildOnly=yes executorPackIO=no
[MAPSHOWHIDEPROBE] RAM heap8=...->... persistentHeapCost=... payload=2408 allocatorOverhead=... largest8=...->... frameFNV=...->... arenaFNV=c3882516->c3882516 mapStateFNV=cd99b98e->cd99b98e scriptFNV=f9e3d9df->f9e3d9df automapFNV=669b1aa7->669b1aa7
[MAPSHOWHIDEPROBE] LEGACY ... entityTopologyFNV=...->... legacyRuntimeClear=yes
[MAPSHOWHIDEPROBE] PARK ... nativeSpriteTopology=yes nativeShowHideExec=yes topologyBytes=2408 showResultBytes=26 hideResultBytes=18 worldMutationProven=yes worldRestored=yes legacyEntityMutation=no framebufferMutation=no entities=0 monsters=0 noGameplay=yes
[ALIVE] ...
```

Use normal optimized PlatformIO environment `esp32-cyd`.

No local build, CI result or hardware PASS is claimed until explicitly observed.

## Boundary after PASS

If this milestone passes, **all 16 real MAP_INTRO opcode IDs will have an explicit native ownership/execution boundary**.

That does **not** mean the game port is finished. Major later work still includes native gameplay/effect consumers, complete entity/monster runtime, transition consumption, renderer integration, input/gameplay progression and eventually reaching `ST_PLAYING` without the legacy desktop world.

The next milestone after PASS + merge must be chosen only after recovering the new true `main` and reassessing the now-complete MAP_INTRO event-family surface.