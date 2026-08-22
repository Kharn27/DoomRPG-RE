# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #61 — native EV_CHANGEMAP pending transition intent
main = fc39ac60757e0d992e3729a5044a9d83e9994971
hardware-tested firmware = 93e0be24558ebffcbc9f60ef0ced54f29274ab28
```

Merged evidence: [`MAP1_NATIVE_CHANGE_MAP_INTENT.md`](MAP1_NATIVE_CHANGE_MAP_INTENT.md).

## Current candidate

```text
branch = agent/esp32-map1-native-show-hide-topology
base   = fc39ac60757e0d992e3729a5044a9d83e9994971
firmware candidate = 1e9760de2269f57ec24dcea0fc16774a119ae65a
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

Active evidence: [`MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md`](MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md).

The candidate owns together the final two real MAP_INTRO opcode IDs:

```text
7  EV_SHOW
18 EV_HIDE
```

They share a compact native map-sprite/entity tile topology. The stage does not instantiate legacy map entities or mutable legacy Render map data.

## Permanent invariants

```text
board       = ESP32-2432S028R classic CYD
MCU         = ESP32-D0WD-V3 dual core 240 MHz
flash       = 4 MB
PSRAM       = none
framebuffer = 160x120 RGB565 = 38400 B
shapeData   = NULL
mediaTexels = NULL
runtime ZIP = forbidden
backing     = /DoomRPG-ESP32.pak
```

## MAP_INTRO identity

```text
/intro.bsp / Entrance
bytes=21823 crc32=623f34e4
nodes=223 lines=480 mapSprites=344 events=93 byteCodes=265
strings=94 stringData=7779 maxString=313
```

Real opcode IDs:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

## Hardware-proven fingerprints through PR #61

```text
arenaFNV               = c3882516
mapStateFNV            = cd99b98e
scriptFNV              = f9e3d9df
lineStateFNV           = e5e74861
lineTextureStateFNV    = f1fc1875
automapStateFNV        = 669b1aa7
lineDoorFNV            = b1c9d297
unlockFNV              = 261d756a
giveMapFNV             = 98c7ac59
giveMapMutatedAutoFNV  = 9d03ca2d
giveMapMutatedMapFNV   = e21edbce
saveRouteOwnerFNV      = 06ea6ea8
saveRouteResultFNV     = c2ecb064
saveRouteContentFNV    = 725845aa
legacySaveRouteFNV     = 9bcfe135
changeMapOwnerFNV      = f75eb7c7
changeMapResultFNV     = 2f40c9be
changeMapContentFNV    = f7a79d99
changeMapInitialFNV    = 69691905
changeMapSampleFNV     = 4e4ebeac
legacyTransitionFNV    = 79ab740c
playerStatsFNV         = 0b2ae445
```

## Persistent native RAM entering this candidate

Hardware-proven heap:

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

CHANGEMAP and SAVE route are caller-owned value states and add no persistent heap.

## Candidate compact sprite topology

Permanent files:

```text
ESP32/include/esp_map_sprite_topology.h
ESP32/src/esp_map_sprite_topology.c
```

Storage is exactly 7 B per map sprite:

```text
entity type       1 B
entity subtype    1 B
visual state      1 B
link state/tile   2 B
link order        2 B
---------------------
344 sprites       2408 B payload
```

No `Entity_t*`, `Sprite_t*`, monster objects or 1024-entry pointer entity database are stored.

The owner reconstructs relevant `Game_loadMapEntities()` classification from immutable map sprites plus a bounded `/entities.db` read from the native PAK. PAK access occurs only while building the owner; SHOW/HIDE execution performs no PAK or ZIP I/O.

Hardware must establish:

```text
entityDefCount
entityCount
EntityDef-backed count
fallback count
initial linked count
hidden sprite count
hidden entity count
enemy count
destructible count
nextLinkOrder
topologyFNV
actual persistent heap cost / allocator overhead
```

Expected persistent allocation bound:

```text
2408 B <= heapCost <= 2536 B
largest8 >= 32768 B
```

New total persistent heap is hardware-pending:

```text
15584 B + actual topology heap cost
```

## SHOW permanent contract

Legacy bytecode behavior:

```text
spriteIndex = arg1 & 0xffff
showFlags   = (arg1 >> 16) & 0xff
replace sprite visual state
if sprite->ent:
    find enemy/destructible blocker and Entity_died()
    find a second blocker and Entity_died()
    link target entity at sprite tile
handled=true
```

Native direct ownership is deliberately bounded to the base compact topology/visual projection. Full `Entity_died()` gameplay fan-out is not opened here.

Deferred metadata covers non-owned blocker gameplay. The known RNG crate branch (`eType=12,eSubType=2`) is rejected before mutation. Repeated SHOW on an already-linked target is also refused rather than reproducing legacy double-link corruption.

Expected result ABI:

```text
EspMapShowResult = 26 B
```

## HIDE permanent contract

Legacy behavior walks the tile entity chain and, for each non-line non-enemy entity, sets the map-sprite hidden bit and unlinks the entity.

Native topology preserves map-sprite link traversal order using 16-bit link-order values rather than pointers.

Expected result ABI:

```text
EspMapHideResult = 18 B
```

Repeated HIDE remains handled and becomes a zero-mutation no-op after eligible entities are already hidden/unlinked.

## Temporary real-CYD probe

Probe files:

```text
ESP32/include/native_map1_show_hide_probe.h
ESP32/src/native_map1_show_hide_probe.c
ESP32/src/native_map1_show_hide_probe_internal.h
ESP32/src/native_map1_show_hide_probe_support.c
ESP32/src/native_map1_show_hide_probe_corpus.c
```

The probe runs only after hardware-proven CHANGEMAP.

Initial topology is cross-checked against the already-loaded legacy `EntityDefManager` without calling legacy map-entity construction.

The complete 93-event / 265-bytecode corpus is scanned. Every opcode 7/18 must remain unsupported by the old state-only executor and be accepted only by this topology executor.

Hardware will establish:

```text
refs / showRefs / hideRefs
removableRefs
showMutated
hideMutated / hideNoMutation
showTargetEnt / showTargetNoEnt
blockersFound / blockersRemoved / blockerNoops
deferredDeaths
hideEntities total
showResultFNV / hideResultFNV
showStateFNV / hideStateFNV
```

Acceptance requires:

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
showRepeatGuard = 1
hideIdempotent = 1
reset = 1
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

## Integrity boundary

Hardware must prove after final rollback/PARK:

```text
topology initial FNV restored exactly
arenaFNV=c3882516
mapStateFNV=cd99b98e
scriptFNV=f9e3d9df
lineStateFNV=e5e74861
lineTextureStateFNV=f1fc1875
automapStateFNV=669b1aa7
framebuffer unchanged
legacy notebook/keys/Hud/password/continuation unchanged
legacy entity topology witness unchanged
pack closed
legacy Render runtime clear
entities=0 monsters=0
ST_PLAYING not reached
```

## Latest hardware-proven CHANGEMAP boundary

```text
refs=1 pending=1 showStats=1 directLoad=0
name="/junction.bsp" targetMap=9 spawnParam=0 effects=03
ownerBytes=16 resultBytes=20 persistentHeapBytes=0
ownerFNV=f75eb7c7 resultFNV=2f40c9be contentFNV=f7a79d99
rollback=1/1 reapplyExact=1 closedPackApply=1
transitionTriggered=no statsMutation=no menuMutation=no mapLoad=no
heap8=68176->68176 largest8=34804->34804
```

## Validation target

Build/flash normal optimized environment:

```text
esp32-cyd
```

Branch / firmware:

```text
agent/esp32-map1-native-show-hide-topology
1e9760de2269f57ec24dcea0fc16774a119ae65a
```

Capture `[MAPTOPOLOGY]`, `[MAPSHOWHIDE]`, `[MAPSHOW]`, `[MAPHIDE]`, `[MAPSHOWHIDEPROBE]` and stable `[ALIVE]` lines.

No CI status is published for this firmware candidate. No local build or hardware PASS is claimed.

## Boundary after candidate PASS

If SHOW/HIDE passes, **all 16 real MAP_INTRO opcode IDs will have explicit native ownership/execution boundaries**.

This is only the end of MAP_INTRO event-family ownership. Full native gameplay/effect consumers, entity/monster runtime, transition consumption, renderer integration and actual `ST_PLAYING` progression remain later milestones.
