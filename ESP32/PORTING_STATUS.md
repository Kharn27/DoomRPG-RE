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
firmware candidate = 93d26e171e8a98f3824b3071e01b9234c8ebe6c3
status = IMPLEMENTED; CORRECTED REAL-CYD VALIDATION PENDING
```

Active evidence: [`MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md`](MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md).

The candidate owns together the final two real MAP_INTRO opcode IDs:

```text
7  EV_SHOW
18 EV_HIDE
```

They share one compact native map-sprite/entity tile topology. No legacy map entities or mutable legacy Render map data are instantiated.

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

No `Entity_t*`, `Sprite_t*`, monster object or 1024-entry pointer entity database is stored.

The owner reconstructs the relevant `Game_loadMapEntities()` classification from immutable BSP map sprites plus one bounded `/entities.db` read from `/DoomRPG-ESP32.pak`. PAK access occurs only during owner build; SHOW/HIDE execution performs no PAK or ZIP I/O.

Expected persistent allocation bound:

```text
2408 B <= heapCost <= 2536 B
largest8 >= 32768 B
```

## SHOW/HIDE permanent boundary

Legacy SHOW replaces sprite visual state and, when `sprite->ent` exists, may call `Entity_died()` on up to two enemy/destructible blockers before linking the target entity.

The native owner directly owns only deterministic compact topology consequences:

```text
SHOW:
- visual state update
- base blocker unlink/death projection
- target tile link
- deferred blocker-gameplay metadata

HIDE:
- traverse native tile links in legacy link order
- leave enemies linked
- hide/unlink eligible map-sprite entities
```

Full `Entity_died()` gameplay fan-out remains deferred. The RNG crate branch (`eType=12,eSubType=2`) is fail-closed before mutation. Repeated SHOW on an already-linked target is refused instead of reproducing legacy double-link corruption.

Expected result ABIs:

```text
EspMapShowResult = 26 B
EspMapHideResult = 18 B
```

## First real-CYD attempt — probe acceptance bug, permanent owner not disproven

Firmware initially tested:

```text
1e9760de2269f57ec24dcea0fc16774a119ae65a
```

The first strict probe printed `FAILED topology/corpus audit`, but a follow-up diagnostic firmware (`3a7dc83b14e8de47827b51bee12b0c907635ffc3`) localized the cause.

Hardware proved the owner itself built correctly:

```text
sprites=344
storageBytes=2408
actual heap delta=2424 B = 2408 payload + 16 allocator overhead
stateFNV=3f321e43
entities=220
hasDef=213
fallback=7
linked=209
hiddenEntities=11
enemies=30
destructibles=13
nextOrder=209
initial audit=PASS
```

Complete isolated real corpus diagnostic:

```text
refs=12
SHOW=11
HIDE=1
SHOW status OK=11
HIDE status OK=1
SHOW already-linked=0
SHOW random-blocker=0
other SHOW failures=0
other HIDE failures=0
finalFNV=3f321e43
```

The sole real HIDE is:

```text
cmd=173 event=60 off=9
tile=2,22 index=706
handled=1 removeIfHandled=1
source-state hidden=0 effects=00
source-state FNV unchanged=3f321e43
```

Therefore the original probe assumption was wrong: it required `hideMutated > 0` and `hideEntities > 0` while testing every command after resetting to source state.

The same event contains an earlier real SHOW on the same tile:

```text
SHOW cmd=165 event=60 off=1
sprite=0 tile=706
status=OK
source FNV 3f321e43 -> 2de723aa
```

The corrected proof must distinguish:

```text
isolated corpus:
  HIDE is valid/handled and may be a source-state no-op

contextual topology proof:
  reset
  same-event same-tile real SHOW before HIDE
  HIDE must hide/unlink >=1 entity
  second HIDE must remain handled with zero additional mutation
  reset must restore exact initial topology FNV
```

No permanent `esp_map_sprite_topology.c` semantics were changed as a result of the first hardware failure.

## Corrected final real-CYD probe

Authoritative firmware candidate:

```text
93d26e171e8a98f3824b3071e01b9234c8ebe6c3
```

New temporary files:

```text
ESP32/include/native_map1_show_hide_final_probe.h
ESP32/src/native_map1_show_hide_final_probe.c
```

The lifecycle now runs only this corrected final SHOW/HIDE probe after CHANGEMAP. The older failing probe/diagnostic sources remain temporary source history but are not serviced.

Acceptance now requires:

```text
initial topology audit PASS
SHOW refs > 0
HIDE refs > 0
refs = SHOW + HIDE
stateExecRefused = refs
rollback = refs/refs
SHOW mutated > 0
isolated HIDE handled whether mutation or no-op
same-event/same-tile context SHOW found before HIDE
context HIDE hiddenEntityCount > 0
context HIDE state FNV changes
second context HIDE handled + hiddenEntityCount=0 + exact state unchanged
SHOW repeat guard=1
final reset exact
showResultBytes=26
hideResultBytes=18
```

Fail closed remains:

```text
unsupported=1
badOffset=1
badDescriptor=1
nullDescriptor=1
nullResult=1
no real RNG crate blocker
targetRelink=guarded
stateAtomic=yes
```

## Integrity boundary

Final hardware acceptance requires:

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

## Validation target

Build/flash normal environment:

```text
esp32-cyd
```

Branch / firmware:

```text
agent/esp32-map1-native-show-hide-topology
93d26e171e8a98f3824b3071e01b9234c8ebe6c3
```

Capture `[MAPTOPOLOGY]`, `[MAPSHOWHIDE]`, `[MAPSHOW]`, `[MAPHIDE]`, `[MAPSHOWHIDEFINAL]` and stable `[ALIVE]` lines.

No CI status is published for this firmware candidate. No local build or hardware PASS is claimed.

## Boundary after candidate PASS

If corrected SHOW/HIDE passes, **all 16 real MAP_INTRO opcode IDs will have explicit native ownership/execution boundaries**.

That completes MAP_INTRO event-family ownership only. Full native gameplay/effect consumers, entity/monster runtime, transition consumption, renderer integration and actual `ST_PLAYING` progression remain later milestones.
