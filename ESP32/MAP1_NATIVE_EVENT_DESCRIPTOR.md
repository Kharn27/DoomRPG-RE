# ESP32 MAP_INTRO native event descriptor / bytecode linkage

Branch: `agent/esp32-map1-native-event-descriptor`

Base merged `main`:

```text
PR   = #46 — allocation-free native tile -> event lookup
main = 438cffabaaaaa3dc3b45486f56eacec1a047edcf
```

Status: **IMPLEMENTED; AWAITING REAL-CYD HARDWARE PASS**.

## Objective

Decode each compact 32-bit MAP_INTRO event into a read-only native descriptor and validate its linkage to the already resident compact bytecode records, without executing a single command and without allocating new persistent memory.

Inherited hardware-proven layers:

```text
EspMapRuntime immutable arena
    payload      = 14095 B
    actual heap  = 14112 B
    arenaFNV     = c3882516

EspMapState mutable tile flags
    payload      = 1024 B
    actual heap  = 1040 B
    stateFNV     = cd99b98e

EspMapEvents tile lookup
    persistent   = 0 B
    lookupFNV    = 63430151
    93 hits / 931 misses over all 1024 tiles
```

This milestone adds **0 persistent bytes**.

## Recovered 32-bit event format

`Game_runEvent()` decodes one event value as follows:

```text
bits  0..9   tile index
bits 10..18  first command index
bits 19..24  command count
bits 25..28  event state
bits 29..31  event flags
```

Native masks/shifts:

```text
ESP_MAP_EVENT_TILE_MASK          = 0x000003ff
ESP_MAP_EVENT_COMMAND_INDEX_MASK = 0x0007fc00
ESP_MAP_EVENT_COMMAND_COUNT_MASK = 0x01f80000
ESP_MAP_EVENT_STATE_MASK         = 0x1e000000
ESP_MAP_EVENT_FLAGS_MASK         = 0xe0000000
```

The reference code computes:

```text
eventState   = (event & 0x1e000000) >> 25
eventFlags   = (event & 0xe0000000) >> 29
commandCount = (event & 0x01f80000) >> 19
commandIndex = (event & 0x0007fc00) >> 10
```

The desktop implementation then multiplies `commandCount` and `commandIndex` by `BYTE_CODE_MAX == 3` because `Render.mapByteCode` is an `int[]` flattened as:

```text
ID, ARG1, ARG2, ID, ARG1, ARG2, ...
```

The ESP32 compact arena already stores one logical command as one 9-byte `EspMapByteCode` record, so native indexes remain **record indexes** and must not inherit the desktop `* 3` storage artifact.

## Initial state versus mutable runtime state

The recovered engine later rewrites only the state field in `tileEvents[]`:

```text
tileEvents[eventIndex] =
    (event & 0xe1ffffff) | (eventState << 25)
```

Therefore the immutable BSP value can only represent the **initial** event state.

The native descriptor deliberately names the field:

```text
initialState
```

A later current-state implementation must use a separate small mutable overlay. It must never patch the immutable `EspMapRuntime` arena.

## Permanent native API

Extended permanent component:

```text
ESP32/include/esp_map_events.h
ESP32/src/esp_map_events.c
```

Descriptor:

```text
EspMapEventDescriptor
    value                 uint32
    eventIndex            uint16
    tileIndex             uint16
    firstCommandIndex     uint16
    commandEndIndex       uint16   # exclusive
    commandCount          uint8
    initialState          uint8
    flags                 uint8
```

New APIs:

```text
EspMapEvents_describe(eventRef, &descriptor)
EspMapEvents_getCommand(&descriptor, commandOffset, &byteCode)
```

`EspMapEvents_describe()` validates:

```text
- eventRef index exists in the resident runtime
- resident raw value equals eventRef.value
- low-10-bit tile equals eventRef.tileIndex
- firstCommandIndex <= byteCodeCount
- firstCommandIndex + commandCount <= byteCodeCount
```

`EspMapEvents_getCommand()` validates:

```text
commandOffset < commandCount
commandEndIndex == firstCommandIndex + commandCount
commandEndIndex <= resident byteCodeCount
```

The API returns an existing compact command through `EspMapRuntime_getByteCode()`; it creates no command copies or ownership graph.

## Known MAP_INTRO event/bytecode facts

Hardware-proven event section:

```text
events       = 93
bytes        = 372
sorted       = yes
unique tiles = yes
first        = tile 68 / event 0 / 00080044
last         = tile 968 / event 92 / 000c23c8
```

Resident compact bytecodes:

```text
records = 265
bytes   = 2385
```

The first raw event already decodes structurally to:

```text
value             = 00080044
tile              = 68
firstCommandIndex = 0
commandCount      = 1
initialState      = 0
flags             = 0
```

The last raw event is expected to prove the upper linkage boundary on hardware.

## Temporary real-CYD probe

Validation scaffold:

```text
ESP32/include/native_map1_event_descriptor_probe.h
ESP32/src/native_map1_event_descriptor_probe.c
```

It runs only after the hardware-proven `MAPEVENTPROBE` stage completes.

The probe must:

1. require the inherited arena/state/event-lookup safe boundary;
2. decode all 93 raw events independently with masks/shifts;
3. resolve each event tile through `EspMapEvents_findByTile()`;
4. require `EspMapEvents_describe()` to reproduce every independently decoded field;
5. require every command range to stay inside the 265 compact bytecodes;
6. walk every command linked by every descriptor through `EspMapEvents_getCommand()`;
7. compare every linked command to direct `EspMapRuntime_getByteCode(globalIndex)` access;
8. require `commandOffset == commandCount` to fail closed for every event;
9. verify invalid NULL/forged descriptor inputs fail closed;
10. compute a canonical `descriptorFNV` over all descriptor fields;
11. compute a canonical `linkageFNV` over every event -> command linkage plus command payload;
12. measure total command references, unique command indexes, overlaps and uncovered bytecode indexes;
13. establish command-count range, maximum linked end index, observed initial-state mask and event-flags mask;
14. require zero heap/largest-block drift;
15. require framebuffer, immutable arena and mutable tile state unchanged;
16. PARK before any event execution, state mutation, entities, rendering or `ST_PLAYING`.

A 265-bit stack bitmap (~34 bytes) is used only as a temporary coverage oracle. No persistent coverage/index structure is added.

## Values the real CYD must establish

```text
descriptorFNV
linkageFNV
full descriptor/link sweep elapsed time

total command references
unique command indexes
overlap references
uncovered bytecodes
commandCount min..max
maximum commandEndIndex
observed initialState mask
observed event-flags mask

first descriptor
last descriptor
```

The probe deliberately does **not** assume in advance that all 265 bytecodes are covered exactly once. It measures the actual topology first.

## Expected new log tail

```text
[MAPDESCPROBE] ARMED ...

=== Doom RPG ESP32-native MAP_INTRO event descriptor linkage ===
[MAPDESCPROBE] CONTRACT decode event bits + validate linked compact bytecodes; 0 persistent bytes; no script execution, mutation, entities, rendering or gameplay

[MAPDESC] READY events=93 byteCodes=265 descriptorFNV=........ linkageFNV=........ persistentBytes=0
[MAPDESCPROBE] READY elapsed=...ms commandRefs=... unique=... overlaps=... gaps=... countRange=..... maxEnd=... stateMask=.... flagsMask=..
[MAPDESCPROBE] SAMPLE first=68/0/00080044 cmd=0+1 state=0 flags=0 last=968/92/000c23c8 cmd=...+... state=... flags=...
[MAPDESCPROBE] RAM heap8=X->X delta=0 largest8=Y->Y delta=0 frameFNV=F->F arenaFNV=c3882516->c3882516 stateFNV=cd99b98e->cd99b98e
[MAPDESCPROBE] PARK state=9 page=3 nativeArena=yes nativeTileState=yes nativeEventLookup=yes nativeEventDescriptor=yes persistentBytes=0 scriptExecution=no entities=0 monsters=0 noGameplay=yes
```

## Memory plan

New persistent payload:

```text
0 B
```

Existing measured persistent map/world foundation remains:

```text
14112 B immutable arena actual heap
+1040 B mutable tile state actual heap
--------------------------------------
15152 B total
```

The descriptor and linked `EspMapByteCode` values are stack/local copies only.

## Still forbidden

```text
Game_runEvent() equivalent
Game_executeEvent() equivalent
bytecode side effects
current event-state mutation
EVENT_FLAG_BLOCKINPUT behavior
command arg2 execution filters
save/resume event execution
legacy Render.tileEvents ownership
legacy Render.mapByteCode ownership
entity/monster activation
player spawn
native gameplay rendering
ST_PLAYING
```

## Next boundary after hardware PASS

If the descriptor/linkage contract passes, native code will know, without allocation:

```text
which tile -> which event
which event -> which compact command range
which command offset -> which exact EspMapByteCode
```

Only then should the next milestone recover **execution filtering semantics** from `Game_runEvent()` (`initial/current state`, direction/trigger flags, block-input, remove/modify-world behavior), still preferably as a side-effect-free decision layer before implementing actual script execution.
