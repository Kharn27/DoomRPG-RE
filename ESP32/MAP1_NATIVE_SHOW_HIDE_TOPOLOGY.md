# ESP32 MAP_INTRO native SHOW/HIDE sprite-topology milestone

Branch: `agent/esp32-map1-native-show-hide-topology`

Base merged `main`:

```text
PR   = #61 — native EV_CHANGEMAP pending transition intent
main = fc39ac60757e0d992e3729a5044a9d83e9994971
```

Firmware candidate:

```text
1e9760de2269f57ec24dcea0fc16774a119ae65a
```

Status: **IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING**.

## Objective

Own together the final two real MAP_INTRO opcode families:

```text
7  EV_SHOW
18 EV_HIDE
```

They share one sprite/entity tile topology in the legacy engine, so splitting them would create an artificial ownership boundary.

This milestone does not build the legacy `Game.entities[400]`, `entityDb[1024]`, `Entity_t` pointer chains or mutable legacy `Render.mapSprites`. `shapeData` and `mediaTexels` remain NULL; legacy map entities remain zero.

## Exact legacy behavior

SHOW:

```text
sprite = mapSprites[arg1 & 0xffff]
replace legacy visual byte from arg1 bits 16..23
if sprite->ent exists:
    find first enemy/destructible on target tile and Entity_died()
    find again and Entity_died() a second blocker if present
    link sprite->ent at sprite x/y tile
handled=true
```

The legacy visual mask is:

```text
sprite->info = (sprite->info & 0xFFFEE1FF) |
               (((arg1 >> 16) & 0xff) << 9)
```

HIDE:

```text
walk entityDb chain at tile (arg1.x,arg1.y)
for each non-line, non-enemy entity:
    set map-sprite hidden bit 0x10000
    unlink entity
handled=true
```

A repeated HIDE is handled but becomes a zero-mutation no-op once the eligible entities are gone.

## Blocker boundary

`Entity_died()` is a gameplay fan-out, not just a topology mutation. It may touch XP, sounds, HUD, monster lists, drops, secondary spawns, tile events and other state.

This milestone directly owns only the compact base SHOW/HIDE topology projection:

```text
target visual state
base blocker alive/link projection
base blocker visual projection
target link/unlink topology
```

Non-owned blocker gameplay is represented by `ESP_MAP_SHOW_EFFECT_DEFER_BLOCKER_GAMEPLAY`; the native probe never calls legacy `Entity_died()`.

The known RNG crate branch (`eType=12,eSubType=2`) is rejected before any mutation with `ESP_MAP_SPRITE_TOPOLOGY_RANDOM_BLOCKER`. The real MAP_INTRO corpus must prove whether this guard is sufficient.

Repeated SHOW on an already-linked entity target is also refused fail-closed (`TARGET_ALREADY_LINKED`) rather than reproducing a corrupting legacy double-link.

## Initial topology reconstruction

The compact owner reproduces the relevant `Game_loadMapEntities()` rules from immutable BSP sprites plus `/entities.db`:

```text
source sprites scanned in order
source 0x01000000 -> no initial map-sprite entity
lookup = info & 511
source 0x00040000 -> lookup += 305
EntityDef hit -> map-sprite entity + sprite->ent
source 0x00020000 fallback -> entity exists but sprite->ent does not
source hidden 0x00010000 -> entity initially unlinked
otherwise Game_linkEntity insertion order is reproduced
```

`Game_linkEntity()` inserts at the head of the tile chain. Native code replaces pointers with a monotonic 16-bit link order and reconstructs traversal order allocation-free.

## Native `/entities.db` parser

During owner construction only, the caller opens `/DoomRPG-ESP32.pak` and passes the `/entities.db` entry.

Recovered record:

```text
uint16 tileIndex
uint8  eType
uint8  eSubType
int32  parm
char   name[16]
----------------
24 B
```

Only type/subtype classification is retained. The PAK closes before SHOW/HIDE execution; the permanent executor performs no PAK or ZIP I/O.

## Permanent compact owner

Files:

```text
ESP32/include/esp_map_sprite_topology.h
ESP32/src/esp_map_sprite_topology.c
```

Per 344 map sprites:

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

No entity pointers, monster objects or 1024-entry pointer database are stored.

Packed link state:

```text
bits 0..9  tile
bit 10     linked
bit 11     legacy sprite->ent exists
bit 12     base alive/damageable projection
bit 13     map-sprite entity exists
```

Permanent API:

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

Expected classic ESP32 ABI:

```text
EspMapShowResult = 26 B
EspMapHideResult = 18 B
```

Both results retain source event/command provenance, legacy handled value and `removeCommandIfHandled`; script state itself is not removed here.

## Temporary real-CYD probe

Files:

```text
ESP32/include/native_map1_show_hide_probe.h
ESP32/src/native_map1_show_hide_probe.c
ESP32/src/native_map1_show_hide_probe_internal.h
ESP32/src/native_map1_show_hide_probe_support.c
ESP32/src/native_map1_show_hide_probe_corpus.c
```

The stage runs only after hardware-proven CHANGEMAP.

Inherited required fingerprints:

```text
arenaFNV            = c3882516
mapStateFNV         = cd99b98e
scriptFNV           = f9e3d9df
lineStateFNV        = e5e74861
lineTextureStateFNV = f1fc1875
automapStateFNV     = 669b1aa7
```

Other preconditions:

```text
/intro.bsp bytes=21823 crc32=623f34e4
mapSprites=344 events=93 byteCodes=265
ST_INTRO page=3
pack closed
legacy Render runtime clear
entities=0 monsters=0
```

### Initial topology audit

The hardware probe cross-checks all 344 native sprite classifications against the already-loaded legacy `EntityDefManager`, without creating legacy map entities.

Hardware establishes:

```text
entityDefCount
map-sprite entity count
EntityDef-backed / fallback counts
linked count
hidden sprite / hidden entity counts
enemy / destructible counts
nextLinkOrder
initial topologyFNV
```

### Complete opcode corpus

All 93 events / 265 bytecodes are scanned. Every real opcode 7/18 must be refused by the old state-only executor and accepted only by this topology executor.

Hardware establishes:

```text
refs / show refs / hide refs
removable refs
showMutated
hideMutated / hideNoMutation
show target entity / no-entity counts
blockers found / removed / no-op
blocker deferred-gameplay count
hideEntities total
showResultFNV / hideResultFNV
showStateFNV / hideStateFNV
```

Acceptance:

```text
showRefs > 0
hideRefs > 0
refs = showRefs + hideRefs
stateExecRefused = refs
rollback = refs/refs
showMutated > 0
hideMutated > 0
hideEntities > 0
showResultBytes = 26
hideResultBytes = 18
```

Repeat proof:

```text
SHOW entity target: first apply OK, second apply TARGET_ALREADY_LINKED with no state change
HIDE: first apply mutates, second apply handled with hiddenEntityCount=0 and exact state unchanged
```

Fail closed:

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

Hardware-proven native persistent heap entering this milestone:

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

New topology payload:

```text
2408 B
```

Probe acceptance bounds:

```text
2408 <= persistentHeapCost <= 2536 B
largest8 >= 32768 B
```

Actual allocator overhead and the new persistent total are hardware values.

## Integrity boundary

Final state must prove:

```text
topology state restored exactly after corpus mutations
arena/map/script/line/texture/automap fingerprints unchanged
framebuffer unchanged
legacy notebook/keys/Hud/password/continuation unchanged
legacy 400-entity + 1024 entityDb topology witness unchanged
PAK closed
legacy Render runtime clear
entities=0 monsters=0
ST_PLAYING not reached
```

The probe does not call legacy `Entity_died()`, `Game_linkEntity()`, `Game_unlinkEntity()` or mutate legacy `Render.mapSprites`.

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

Use normal PlatformIO environment `esp32-cyd`.

No CI status is published for firmware candidate `1e9760de2269f57ec24dcea0fc16774a119ae65a`. No local build or hardware PASS is claimed.

## Boundary after PASS

If this milestone passes, all 16 real MAP_INTRO opcode IDs will have an explicit native ownership/execution boundary.

That is **not** the end of the port: full native entity/monster gameplay, deferred effect consumers, transition consumption, rendering and actual `ST_PLAYING` progression still remain.
