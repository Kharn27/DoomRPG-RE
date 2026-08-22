# ESP32 MAP_INTRO native SHOW/HIDE sprite-topology milestone

Branch: `agent/esp32-map1-native-show-hide-topology`

Base merged `main`:

```text
PR   = #61 — native EV_CHANGEMAP pending transition intent
main = fc39ac60757e0d992e3729a5044a9d83e9994971
```

Hardware-tested firmware:

```text
f881ccdad20d950462dd781456c340e792f59ec3
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective and result

This milestone owns together the final two real MAP_INTRO opcode families:

```text
7  EV_SHOW
18 EV_HIDE
```

They share one compact sprite/entity tile topology. The milestone does not construct legacy `Game.entities[400]`, `entityDb[1024]`, pointer chains or mutable legacy `Render.mapSprites`. `shapeData` and `mediaTexels` remain NULL; legacy map entities remain zero.

With the final real-CYD PASS, **all 16 real MAP_INTRO opcode IDs now have an explicit native ownership/execution boundary**.

## Permanent compact owner

Permanent files:

```text
ESP32/include/esp_map_sprite_topology.h
ESP32/src/esp_map_sprite_topology.c
```

For all 344 map sprites:

```text
entity type       1 B
entity subtype    1 B
visual state      1 B
link state/tile   2 B
link order        2 B
---------------------
                  7 B / sprite
```

Hardware-proven storage:

```text
sprites            = 344
payload             = 2408 B
actual heap cost    = 2424 B
allocator overhead  = 16 B
largest8            = 34804 B unchanged
```

No `Entity_t*`, `Sprite_t*`, monster object or 1024-entry pointer database is retained.

The topology is reconstructed from immutable BSP sprites plus one bounded `/entities.db` read from `/DoomRPG-ESP32.pak`. SHOW/HIDE execution itself performs no PAK or ZIP I/O.

## Hardware-proven initial topology

```text
entityDefCount = 115
entities       = 220
hasDef         = 213
fallback       = 7
linked         = 209
hiddenSprites  = 11
hiddenEntities = 11
enemies        = 30
destructibles  = 13
nextOrder      = 209
stateFNV       = 3f321e43
```

Result ABI:

```text
EspMapShowResult = 26 B
EspMapHideResult = 18 B
```

## Exact permanent boundary

Legacy SHOW replaces sprite visual state and may call `Entity_died()` on up to two enemy/destructible blockers before linking its target entity.

The native owner directly owns only deterministic compact consequences:

```text
SHOW:
- visual state update
- base blocker alive/link projection
- base blocker visual projection
- target tile link
- deferred blocker-gameplay metadata

HIDE:
- traverse native tile links in legacy link order
- leave enemies linked
- hide/unlink eligible map-sprite entities
```

Full `Entity_died()` gameplay fan-out remains deferred. The RNG crate branch (`eType=12,eSubType=2`) is fail-closed before mutation. Repeated SHOW on an already-linked target is guarded instead of reproducing legacy double-link corruption.

## Real MAP_INTRO corpus

Hardware-proven corpus:

```text
refs                = 12
SHOW                 = 11
HIDE                 = 1
removable            = 12
stateExecRefused     = 12
showMutated          = 11
hideMutatedIsolated  = 0
hideNoMutation       = 1
showTargetEnt        = 11
showTargetNoEnt      = 0
blockersFound        = 2
blockersRemoved      = 2
blockerNoops         = 0
deferredDeaths       = 2
hideEntitiesIsolated = 0
```

Fingerprints:

```text
showResultFNV = 6029eb3c
hideResultFNV = d24f5bae
showStateFNV  = b6a45f47
hideStateFNV  = bec68187
```

Canonical sample SHOW:

```text
cmd=111 event=43 off=0
sprite=1 tile=613
blockers=0 removed=0
effects=0005
handled=1
removeIfHandled=1
```

## HIDE source-state and contextual proof

The only real HIDE is a legitimate source-state no-op:

```text
cmd=173 event=60 off=9
tile=2,22 index=706
hidden=0
effects=00
handled=1
removeIfHandled=1
```

The same event has an earlier real SHOW on the exact tile:

```text
SHOW cmd=165 event=60 off=1
sprite=0 tile=706
```

Hardware-proven contextual sequence:

```text
initial topology FNV = 3f321e43
SHOW165 after FNV    = 2de723aa
HIDE173 after FNV    = bb1d78a4
hidden               = 1
first/last sprite    = 0 / 0
effects              = 03
second HIDE hidden   = 0
contextProven        = 1
idempotent           = 1
```

This proves both correct source-state handling and real hide/unlink mutation without inventing a synthetic opcode.

## Rollback / fail-closed

```text
rollback        = 12/12
showRepeatGuard = 1
hideContext     = 1
hideIdempotent  = 1
reset           = 1
worldRestored   = yes
```

Fail-closed hardware proof:

```text
unsupported      = 1
badOffset        = 1
badDescriptor    = 1
nullDescriptor   = 1
nullResult       = 1
randomCrateReal  = 0
targetRelink     = guarded
stateAtomic      = yes
```

## PAK / RAM evidence

```text
/entities.db size  = 2762 B
CRC32              = 4f2be32d
heapOpen           = 63704
transientPackCost  = 4376 B
largestOpen        = 34804
packIO             = yes, build only
executorPackIO     = no
```

Persistent owner cost:

```text
heap8 68080 -> 65656
persistentHeapCost = 2424 B
payload            = 2408 B
allocatorOverhead  = 16 B
largest8 34804 -> 34804
```

Hardware-proven persistent native total after this milestone:

```text
immutable arena          14112 B
mutable tile state        1040 B
mutable script state       100 B
mutable line state         136 B
mutable texture state       76 B
mutable automap state      120 B
mutable sprite topology   2424 B
-------------------------------
total                    18008 B
```

## Integrity boundary

Hardware proved exact preservation/restoration:

```text
frameFNV       b3b98db4 -> b3b98db4
arenaFNV       c3882516 -> c3882516
mapStateFNV    cd99b98e -> cd99b98e
scriptFNV      f9e3d9df -> f9e3d9df
automapFNV     669b1aa7 -> 669b1aa7

notebookFNV       4d7705c5 -> 4d7705c5
keys              00000000 -> 00000000
hudFNV            505b1255 -> 505b1255
passwordCanvasFNV 214171cf -> 214171cf
continuationFNV   e2ba14a5 -> e2ba14a5
entityTopologyFNV f8f9b485 -> f8f9b485
```

Final PARK:

```text
state=9 page=3
nativeSpriteTopology=yes
nativeShowHideExec=yes
topologyBytes=2408
showResultBytes=26
hideResultBytes=18
allMapIntroOpcodeFamiliesOwned=yes
worldMutationProven=yes
worldRestored=yes
legacyEntityMutation=no
framebufferMutation=no
entities=0
monsters=0
noGameplay=yes
```

Stable heartbeats:

```text
70345 ms heap=131420 heap8=65656 largest8=34804
75346 ms heap=131420 heap8=65656 largest8=34804
80347 ms heap=131420 heap8=65656 largest8=34804
85348 ms heap=131420 heap8=65656 largest8=34804
90349 ms heap=131420 heap8=65656 largest8=34804
```

## Probe-history note

The first probe incorrectly required the isolated HIDE to mutate. Diagnostic firmware proved all 12 opcode applications were individually valid and exposed that the sole HIDE is a valid source-state no-op. The corrected final proof then composed the earlier real SHOW from the same event/tile before HIDE.

A later build-only failure in the corrected probe was caused solely by include ordering (`esp_heap_caps.h` exposing `false/true` before legacy `DoomRPG.h`). Firmware `f881ccdad20d950462dd781456c340e792f59ec3` corrected only that compilation plumbing; permanent `esp_map_sprite_topology.c` semantics were unchanged.

## Boundary after PASS

All 16 real MAP_INTRO opcode IDs now have explicit native ownership/execution boundaries.

This closes **MAP_INTRO event-family ownership**, not the whole port. Still outside this milestone:

```text
full native entity/monster gameplay
consumption of deferred blocker gameplay effects
real CHANGEMAP transition consumption
renderer/gameplay integration
actual ST_PLAYING progression
```

No CI status is published for the hardware-tested firmware. The real classic CYD Serial log is the hardware source of truth.
