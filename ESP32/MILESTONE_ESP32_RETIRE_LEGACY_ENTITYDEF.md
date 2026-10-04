# ESP32 legacy EntityDef retirement milestone

Date: 2026-10-04

Branch:
`agent/esp32-retire-legacy-entitydef`

Base main:
`9e337e21ab8aa4ab9851f1169b277a11c73b2a74`

Hardware-tested code boundary:
`9fa46cf86a1e8b4bbb25e20d80cfbf9664ef22db`

## Goal

Remove the inherited desktop `EntityDefManager_t` from the normal ESP32
runtime now that native gameplay already owns the permanent compact
`EspEntityDefTypeCatalog`.

This milestone is intentionally narrower than retiring `Game_t` or
`Combat_t`. It changes no entity semantics, combat math, save format, RNG
ordering, renderer ownership or BSP topology.

## Retired desktop ownership

The legacy object model was:

```text
DoomRPG_t::entityDef
 -> EntityDefManager_t
 -> EntityDef_startup()
 -> heap EntityDef_t[numDefs]
 -> each EntityDef_t = 24 B including a resident 16-byte name
```

The ESP32 runtime already had a second representation:

```text
/DoomRPG-ESP32.pak:/entities.db
 -> EspEntityDefTypeCatalog
 -> sorted 8-byte {tileIndex,type,subtype,parm}
 -> one-byte source-order index
 -> name read lazily from PAK only when presentation needs it
```

Keeping both was duplicate ownership.

At the final code boundary:

- `DoomRPG_initEngineCore()` no longer calls `EntityDef_init()`;
- `doomRpg->entityDef` must remain `NULL`;
- pre-render startup no longer requires or calls `EntityDef_startup()`;
- `entities.db` is no longer a pre-render compatibility resource;
- `EntityDef.c` is excluded from the normal ESP32 build;
- generated ESP32 `DoomRPG.c` cleanup no longer retains
  `EntityDef_free()`;
- the resident-map lifecycle remains the only production owner that builds
  entity-definition metadata.

Desktop source files may still contain historical `EntityDef_find/lookup`
calls inside dead legacy map/entity paths. They remain behavioral reference
source, not linked runtime ownership.

## Link/build proof

Normal `esp32-cyd` CI #1568 succeeds with `EntityDef.c` absent from the
compile graph. The job log contains no `EntityDef.c.o` compile and no
`EntityDef_init/startup/find/lookup/free` symbol reference.

The native `EspEntityDefTypeCatalog_*` implementation remains linked and is
built when a resident map loads.

Build metrics:

```text
RAM:   13.9% (45488 / 327680 B)
Flash: 59.9% (784573 / 1310720 B)
artifact id: 11280285689
artifact sha256: 3bff6f3b27453ac42a988f918eac3d0d83c1adc6813e091f54b131d855d38785
```

Static RAM is 8 B below the merged pre-milestone build. More importantly, the
runtime no longer constructs the desktop manager object or its separately
allocated full definition table.

No local PlatformIO build is claimed.

## Real-CYD acceptance

The hardware run covers the native consumers that would fail immediately if the
catalog replacement were incomplete.

### Static pickups: metadata + name

Armor Shard uses native metadata `type=3 subtype=21 parm=4` and then reads the
definition name lazily for feedback:

```text
[PLAYERRES] PREPARE tile=839 sprite=99 defTile=92 type=3 subtype=21 parm=4 action=armor value=0->4 ...
[PLAYERRES] FEEDBACK tile=839 message="Got Armor Shard" sourceDefTile=92 ...
```

The next shard repeats the same lookup and commit.

Weapon acquisition exercises another definition family and on-demand name:

```text
[PLAYERRES] PREPARE tile=655 sprite=189 defTile=1 type=5 subtype=0 parm=0 action=weapon ...
[PLAYERRES] FEEDBACK tile=655 message="Got Axe" sourceDefTile=1 ...
```

### Monster/drop definition lookup

A lethal monster combat resolves a dynamic drop from native type/subtype data:

```text
[MONSTERDROP] COMMIT ... type=3 subtype=20 def=91 tile=658 ... visible=1 ...
```

Entering that tile validates the dynamic record through the same catalog,
retrieves `parm=4`, applies the health pickup and reads the name from PAK:

```text
[PLAYERRES] PREPARE tile=658 sprite=65535 defTile=91 type=3 subtype=20 parm=4 action=health value=27->30 ...
[PLAYERRES] FEEDBACK tile=658 message="Got Health Vial" sourceDefTile=91 ...
```

This is particularly strong acceptance because monster-drop materialization uses
`EspEntityDefTypeCatalog_getParm()` and
`EspEntityDefTypeCatalog_findTileIndex()`, while pickup uses
`getMetadata()` and presentation uses `readName()`.

### Wider gameplay regression

The same session also passes:

- ordinary door open and deferred close;
- dialogs and dialog-chain resume;
- secret discovery;
- monster activation from renderer visibility;
- ordered monster attack/movement sequencing;
- player melee kill;
- dynamic drop render/cull/pickup;
- repeated stable ALIVE samples.

Observed ALIVE examples:

```text
heap=122376 heap8=56452 largest8=51188
heap=118916 heap8=52992 largest8=49140
```

These samples are recorded as runtime witnesses only; they are not used to
attribute an exact allocator-byte delta to this milestone.

## Invariants

The real-CYD gameplay remains on the native architecture:

```text
doomRpg->entityDef == NULL by construction
shapeData == NULL
mediaTexels == NULL
runtime assets = DoomRPG-ESP32.pak
native entity metadata = EspEntityDefTypeCatalog
legacy Game.entities / Game.monsters are not revived
no runtime ZIP fallback added
no map-wide EntityDef clone added
```

## Closure

Hardware-tested code boundary:
`9fa46cf86a1e8b4bbb25e20d80cfbf9664ef22db`.

The runtime code requires no further change for this milestone. Post-test
closure is documentation-only.
