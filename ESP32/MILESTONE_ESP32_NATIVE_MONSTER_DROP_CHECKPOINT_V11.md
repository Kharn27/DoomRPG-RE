# ESP32 native monster-drop checkpoint V11 milestone

Date: 2026-10-02

Branch: `agent/esp32-monster-drop-checkpoint-v11`

Hardware-tested code boundary:
`41665b6676f31372ef67a41257fd8baabc74d8ed`

## Scope

This milestone closes checkpoint persistence for the existing compact native
monster-drop owner. It does not change drop RNG, rendering rules, pickup math or
the eight-slot live ownership model.

The legacy reference stores its eight rotating drop entities in the World save,
including materialized type/subtype, sprite position and `dropIndex`, then
restores those records directly. LOAD does not call `Entity_spawnDropItem()`
again and therefore does not reroll drop RNG.

The permanent native equivalent keeps the same behavior without reviving legacy
entities or mutable BSP sprites.

## V11 checkpoint format

V11 appends one bounded `EspNativeGameplayMonsterDropSnapshot` after the V10
level-progress suffix:

```text
V9 spatial checkpoint        5444 B
V10 level-progress suffix      16 B
V10 total                    5460 B
V11 monster-drop suffix       144 B
V11 total                    5604 B
```

The 144-byte snapshot contains:

- eight 16-byte `EspNativeGameplayMonsterDropRecord` slots;
- immutable arena FNV identity;
- `spawnSerial`;
- semantic snapshot FNV;
- `nextSlot`;
- three zero reserved bytes.

Each record preserves `spawnOrder`, tile index, entity-definition tile,
world X/Y, type/subtype, `active` and `taken`.

`visibleCount` and owner-active state are derived on restore instead of being
serialized. Shape validation checks arena identity, rotating-slot order, tile /
world-coordinate agreement, bounds and the snapshot FNV. Restore additionally
re-resolves type/subtype through the compact entity-definition catalog and
requires the stored definition tile to match.

The record CRC covers the V11 suffix. Atomic temp/backup/rename writing and
64-byte streamed byte verification remain the existing checkpoint mechanism.

## Backward compatibility and same-map reset

V1..V10 remain readable. An older record has no dynamic-drop section, so LOAD
restores no dynamic drops and emits an explicit compatibility witness rather
than fabricating state:

```text
[NATIVESAVE] LEGACY-MONSTER-DROP-GAP version=10 dynamicDrops=fresh-empty ... rng=untouched
```

Session replacement now resets `EspNativeGameplayMonsterDrop` alongside
monster position/activation owners. This is required even when LOAD rebuilds the
same map: identical immutable arena FNV must not cause current live drops to
survive into a checkpoint that predates them.

V11 restore reads the already-materialized records directly. It never calls the
drop resolver, never consumes gameplay RNG and never mutates immutable BSP
MapSprites.

## Real-CYD PASS

The hardware session began by loading an existing V10 checkpoint. The V11
firmware accepted the 5460-byte record and correctly exposed an empty dynamic
drop owner:

```text
[NATIVESAVE] READABLE-SPATIAL path=/DoomRPG-ESP32.sav bytes=5460 ... result=valid
[NATIVESAVE] LEGACY-MONSTER-DROP-GAP version=10 dynamicDrops=fresh-empty warning=drop-pool-not-present-in-record rng=untouched
[NATIVESAVE] LOAD ... version=10 bytes=5460 ... monsterDrops=legacy-empty/0/00000000/serial0/next0 ...
```

A zombie then generated a native Shell Clips drop in slot 0:

```text
[MONSTERDROP] COMMIT roll=b61a3cc5 slot=0 overwriteVisible=0 type=16 subtype=2 def=86 tile=178 pos=1184,352 visible=1 next=1 ownerBytes=144 ...
```

SAVE captured that exact pool:

```text
[MONSTERDROP] SAVE version=11 arena=c3882516 serial=1 next=1 visible=1 stateFNV=16550b12 snapshotBytes=144 rng=untouched
[NATIVESAVE] SAVE ... version=11 bytes=5604 ... recordCrc=3a147996 ...
```

After the SAVE, the player stepped onto tile 178 and consumed the dynamic drop.
The player then moved back and loaded the checkpoint on the same map. V11
restored the saved pool, not the newer consumed live state:

```text
[NATIVESAVE] READABLE-SPATIAL ... bytes=5604 ... result=valid
[MONSTERDROP] RESTORE version=11 arena=c3882516 serial=1 next=1 visible=1 stateFNV=16550b12 rng=untouched materialize=replay-no
[NATIVESAVE] LOAD ... version=11 bytes=5604 ... monsterDrops=restored/1/16550b12/serial1/next1 ... world=...+monster-drops-restored-exact
```

The user reported the restored scene is visually correct. The player then
entered tile 178 again and the restored drop was consumed normally:

```text
[MONSTERDROP] RENDER-CULL slot=0 tile=178 reason=player-tile pickup=pending-after-commit
[PLAYERRES] PREPARE ... defTile=86 type=16 subtype=2 ... action=ammo value=25->35 ... worldRemove=dynamic-drop-slot rollback=armed
[PLAYERRES] PREPARE ... defTile=83 type=6 subtype=1 ... action=ammo value=6->10 ... worldRemove=hidden-overlay rollback=armed
[PLAYERRES] COMMIT tile=178 candidates=2 consumed=2 ...
```

This proves the complete hardware path:

`live materialized drop -> SAVE -> later live pickup -> same-map LOAD -> exact
saved drop restoration -> visible/pickable dynamic drop -> normal PlayerState
pickup`.

The separate scenario where a drop is already `taken` before SAVE and is then
loaded was not independently repeated in this serial session. V11 stores the
current taken bit and exact pool state, but this document does not invent a
separate hardware witness for that case.

## Architecture

The milestone preserves the permanent native boundaries:

- immutable BSP MapSprites remain untouched;
- no legacy `Entity_t` drop clone is serialized or reconstructed;
- the owner remains exactly eight rotating slots;
- no runtime ZIP dependency is introduced;
- restore consumes no gameplay RNG and does not call
  `Entity_spawnDropItem()`;
- `shapeData == NULL` and `mediaTexels == NULL` remain required;
- the existing 3508-byte V9 spatial workspace remains the largest streamed
  checkpoint tail; the V11 suffix itself is 144 bytes.

## Build

GitHub Actions `ESP32 CYD Build` run #1540: SUCCESS.

```text
RAM:   13.9% (used 45392 bytes from 327680 bytes)
Flash: 59.6% (used 781433 bytes from 1310720 bytes)
artifact id: 11248194036
```

No local PlatformIO build is claimed.

## Closure

Hardware-tested code boundary:
`41665b6676f31372ef67a41257fd8baabc74d8ed`.

Post-test closure is documentation-only. No runtime source change is required
after the accepted CYD run.
