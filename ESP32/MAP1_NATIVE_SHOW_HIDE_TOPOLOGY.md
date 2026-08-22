# ESP32 MAP_INTRO native SHOW/HIDE sprite-topology milestone

Branch: `agent/esp32-map1-native-show-hide-topology`

Base merged `main`:

```text
PR   = #61 — native EV_CHANGEMAP pending transition intent
main = fc39ac60757e0d992e3729a5044a9d83e9994971
```

Current firmware candidate:

```text
93d26e171e8a98f3824b3071e01b9234c8ebe6c3
```

Status: **IMPLEMENTED; CORRECTED REAL-CYD HARDWARE VALIDATION PENDING**.

## Objective

Own together the final two real MAP_INTRO opcode families:

```text
7  EV_SHOW
18 EV_HIDE
```

They share one sprite/entity tile topology in the legacy engine. Splitting them would create an artificial ownership boundary.

The milestone never constructs legacy `Game.entities[400]`, `entityDb[1024]`, pointer chains or mutable legacy `Render.mapSprites`. `shapeData` and `mediaTexels` remain NULL; legacy map entities remain zero.

## Exact legacy behavior

SHOW:

```text
sprite = mapSprites[arg1 & 0xffff]
replace visual state from arg1 bits 16..23
if sprite->ent exists:
    find enemy/destructible blocker and Entity_died()
    find again and Entity_died() a second blocker if present
    link sprite->ent at sprite x/y tile
handled=true
```

Legacy visual assignment:

```text
sprite->info = (sprite->info & 0xFFFEE1FF) |
               (((arg1 >> 16) & 0xff) << 9)
```

HIDE:

```text
walk the entity chain at tile (arg1.x,arg1.y)
for each non-line, non-enemy entity:
    set map-sprite hidden bit 0x10000
    unlink entity
handled=true
```

A HIDE on an empty/eligible-free tile is still handled and is a zero-mutation no-op.

## Blocker boundary

`Entity_died()` is a large gameplay fan-out: XP, sounds, HUD, drops, inactive-monster lists, secondary spawns, tile events and other gameplay state may change.

This milestone owns only the compact deterministic SHOW/HIDE topology projection:

```text
target visual state
base blocker alive/link projection
base blocker visual projection
target link/unlink topology
```

Non-owned blocker gameplay is represented by `ESP_MAP_SHOW_EFFECT_DEFER_BLOCKER_GAMEPLAY`; the native probe never calls legacy `Entity_died()`.

The RNG crate branch (`eType=12,eSubType=2`) remains fail-closed before mutation. Repeated SHOW on an already-linked entity target is also refused instead of reproducing legacy double-link corruption.

## Native compact owner

Permanent files:

```text
ESP32/include/esp_map_sprite_topology.h
ESP32/src/esp_map_sprite_topology.c
```

For every one of the 344 map sprites:

```text
entity type       1 B
entity subtype    1 B
visual state      1 B
link state/tile   2 B
link order        2 B
---------------------
                  7 B
```

Payload:

```text
344 * 7 = 2408 B
```

No `Entity_t*`, `Sprite_t*`, monster object or 1024-entry pointer database is retained.

Packed link state:

```text
bits 0..9  tile
bit 10     linked
bit 11     legacy sprite->ent exists
bit 12     base alive/damageable projection
bit 13     map-sprite entity exists
```

The build reproduces the relevant `Game_loadMapEntities()` classification from immutable BSP map sprites plus one bounded `/entities.db` PAK read. SHOW/HIDE execution itself performs no PAK or ZIP I/O.

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

Expected result ABIs:

```text
EspMapShowResult = 26 B
EspMapHideResult = 18 B
```

## First real-CYD attempt

Initial firmware:

```text
1e9760de2269f57ec24dcea0fc16774a119ae65a
```

The first probe stopped at:

```text
[MAPSHOWHIDEPROBE] FAILED topology/corpus audit
```

The hardware heap delta already showed that the 2408 B owner had built:

```text
heap8 68080 -> 65656
delta=2424 B
payload=2408 B
allocator overhead=16 B
```

A diagnostic-only firmware then isolated the failure:

```text
3a7dc83b14e8de47827b51bee12b0c907635ffc3
```

Hardware diagnostic established:

```text
sprites=344
storageBytes=2408
stateFNV=3f321e43
entities=220
hasDef=213
fallback=7
linked=209
hiddenEntities=11
enemies=30
destructibles=13
nextOrder=209
initial audit=1
```

All real SHOW/HIDE opcodes succeeded individually:

```text
refs=12
show=11
hide=1
showOk=11
hideOk=1
showAlreadyLinked=0
showRandomBlocker=0
showOtherFailure=0
hideOtherFailure=0
finalFNV=3f321e43
```

The sole HIDE is a valid source-state no-op:

```text
cmd=173 event=60 off=9
tile=2,22 index=706
status=OK
hidden=0
effects=00
handled=1
removeIfHandled=1
FNV 3f321e43 -> 3f321e43
```

The original probe incorrectly required every real HIDE corpus to include an isolated mutation:

```text
hideMutated > 0
hideEntitiesTotal > 0
```

That assumption is false for MAP_INTRO source state and caused the failure. It did **not** disprove the permanent owner.

## Real same-event context for HIDE

The same event contains an earlier SHOW targeting the exact HIDE tile:

```text
SHOW cmd=165 event=60 off=1
sprite=0
tile=706
status=OK
FNV 3f321e43 -> 2de723aa
```

Therefore the corrected proof separates two questions:

```text
1. Is every real command valid from source state?
   -> yes; the HIDE may legitimately be handled/no-op.

2. Can HIDE actually perform its topology mutation when an eligible entity exists?
   -> prove by reset + real same-event/same-tile SHOW + real HIDE.
```

No permanent `ESP32/src/esp_map_sprite_topology.c` code was changed after this diagnosis.

## Corrected final probe

Authoritative candidate firmware:

```text
93d26e171e8a98f3824b3071e01b9234c8ebe6c3
```

New temporary validation files:

```text
ESP32/include/native_map1_show_hide_final_probe.h
ESP32/src/native_map1_show_hide_final_probe.c
```

The lifecycle now services this corrected final probe instead of the old failing probe/diagnostic.

Corrected sequence:

```text
build owner
cross-check all 344 classifications

for every real SHOW/HIDE:
    reset to source topology
    state-only executor must refuse
    native executor must return OK
    validate provenance/result
    record source-state mutation or no-op
    reset exact

SHOW repeat proof:
    first SHOW OK
    second identical SHOW -> TARGET_ALREADY_LINKED
    no second mutation

HIDE context proof:
    reset
    locate an earlier real SHOW in the same event on the HIDE tile
    apply that SHOW
    apply real HIDE
    require hiddenEntityCount > 0 and topology FNV change
    apply HIDE again
    require handled + hiddenEntityCount=0 + exact unchanged FNV
    reset exact initial FNV
```

The context SHOW is discovered by event/tile relationship, not hardcoded by command index.

## Corrected acceptance

```text
initial topology audit PASS
showRefs > 0
hideRefs > 0
refs = showRefs + hideRefs
stateExecRefused = refs
rollback = refs/refs
showMutated > 0
isolated HIDE handled whether mutation or no-op
same-event/same-tile context SHOW found before HIDE
context HIDE hiddenEntityCount > 0
context HIDE changes topology FNV
second context HIDE handled + hiddenEntityCount=0 + exact FNV unchanged
showRepeatGuard=1
hideContext=1
hideIdempotent=1
reset=1
showResultBytes=26
hideResultBytes=18
```

Fail closed:

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

Topology payload:

```text
2408 B
```

First hardware attempt observed:

```text
persistent topology heap = 2424 B
allocator overhead       = 16 B
```

This becomes canonical only after the corrected final probe passes on the exact final firmware.

Acceptance bounds remain:

```text
2408 <= persistentHeapCost <= 2536 B
largest8 >= 32768 B
```

## Integrity boundary

Final hardware PASS must restore the initial topology FNV and preserve:

```text
arenaFNV            c3882516
mapStateFNV         cd99b98e
scriptFNV           f9e3d9df
lineStateFNV        e5e74861
lineTextureStateFNV f1fc1875
automapStateFNV     669b1aa7
framebuffer unchanged
legacy notebook/keys/Hud/password/continuation unchanged
legacy entity topology witness unchanged
PAK closed
legacy Render runtime clear
entities=0 monsters=0
ST_PLAYING not reached
```

## Expected final Serial family

```text
[MAPSHOWHIDEFINAL] ARMED ...

=== Doom RPG ESP32-native MAP_INTRO SHOW/HIDE final topology ===
[MAPSHOWHIDEFINAL] CONTRACT ...
[MAPTOPOLOGY] READY ...
[MAPSHOWHIDE] READY ... hideMutatedIsolated=... hideNoMutation=...
[MAPSHOW] SAMPLE ...
[MAPHIDE] ISOLATED ...
[MAPHIDE] CONTEXT showCmd=... hideCmd=... hidden=... contextProven=1 idempotent=1
[MAPSHOWHIDE] STATE ... rollback=.../... showRepeatGuard=1 hideContext=1 hideIdempotent=1 reset=1 worldRestored=yes
[MAPSHOWHIDE] FAILCLOSED ... stateAtomic=yes
[MAPSHOWHIDEFINAL] IO ...
[MAPSHOWHIDEFINAL] RAM ...
[MAPSHOWHIDEFINAL] LEGACY ...
[MAPSHOWHIDEFINAL] PARK ... allMapIntroOpcodeFamiliesOwned=yes ... entities=0 monsters=0 noGameplay=yes
[ALIVE] ...
```

Use normal PlatformIO environment `esp32-cyd`.

No CI status is published for `93d26e171e8a98f3824b3071e01b9234c8ebe6c3`. No local build or corrected hardware PASS is claimed.

## Boundary after PASS

If this corrected probe passes, all 16 real MAP_INTRO opcode IDs will have an explicit native ownership/execution boundary.

That completes MAP_INTRO event-family ownership only; full native entity/monster gameplay, deferred effect consumers, transition consumption, renderer integration and actual `ST_PLAYING` progression remain later work.
