# ESP32 MAP_INTRO native event descriptor / bytecode linkage

Branch: `agent/esp32-map1-native-event-descriptor`

Base merged `main`:

```text
PR   = #46 — allocation-free native tile -> event lookup
main = 438cffabaaaaa3dc3b45486f56eacec1a047edcf
```

Hardware-tested code:

```text
b3e453baea1ac861c608f0c11b8aaa592f1cc3e5
```

Status: **REAL-CYD HARDWARE PASS; EVENT DESCRIPTOR + BYTECODE LINKAGE VALIDATED; MERGE-READY**.

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

Therefore the immutable BSP value represents the **initial** event state.

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

The API returns an existing compact command through `EspMapRuntime_getByteCode()`; it creates no command ownership graph.

## MAP_INTRO event/bytecode facts

```text
events       = 93
event bytes  = 372
byteCodes    = 265
byteCode bytes = 2385
sorted event tiles = yes
unique event tiles = yes
```

First descriptor:

```text
value             = 00080044
eventIndex        = 0
tile               = 68
firstCommandIndex = 0
commandCount      = 1
commandEndIndex   = 1
initialState      = 0
flags             = 0
```

Last descriptor:

```text
value             = 000c23c8
eventIndex        = 92
tile               = 968
firstCommandIndex = 264
commandCount      = 1
commandEndIndex   = 265
initialState      = 0
flags             = 0
```

## Hardware validation method

The real-CYD probe:

1. requires the inherited arena/state/event-lookup safe boundary;
2. independently decodes all 93 raw events with masks/shifts;
3. resolves each event tile through `EspMapEvents_findByTile()`;
4. requires `EspMapEvents_describe()` to reproduce every field;
5. requires every command range to remain inside the 265 compact bytecodes;
6. walks every linked command through `EspMapEvents_getCommand()`;
7. compares every linked command with direct `EspMapRuntime_getByteCode(globalIndex)` access;
8. requires `commandOffset == commandCount` to fail closed for every event;
9. checks NULL/forged descriptor/ref inputs fail closed;
10. computes canonical descriptor/linkage fingerprints;
11. measures total refs, unique refs, overlaps, gaps, count range, maximum end and observed state/flag sets;
12. requires zero heap/largest-block drift and no framebuffer/arena/state mutation;
13. PARKs before script execution, event-state mutation, entities, rendering or `ST_PLAYING`.

Coverage uses only a temporary 265-bit (~34-byte) stack bitmap. No persistent coverage/index structure exists.

## Real-CYD hardware result

Exact serial evidence:

```text
[MAPDESC] READY events=93 byteCodes=265 descriptorFNV=27115328 linkageFNV=5727902c persistentBytes=0
[MAPDESCPROBE] READY elapsed=2ms commandRefs=265 unique=265 overlaps=0 gaps=0 countRange=1..14 maxEnd=265 stateMask=0001 flagsMask=03
[MAPDESCPROBE] SAMPLE first=68/0/00080044 cmd=0+1 state=0 flags=0 last=968/92/000c23c8 cmd=264+1 state=0 flags=0
[MAPDESCPROBE] RAM heap8=68992->68992 delta=0 largest8=36852->36852 delta=0 frameFNV=bd237825->bd237825 arenaFNV=c3882516->c3882516 stateFNV=cd99b98e->cd99b98e
[MAPDESCPROBE] PARK state=9 page=3 nativeArena=yes nativeTileState=yes nativeEventLookup=yes nativeEventDescriptor=yes persistentBytes=0 scriptExecution=no entities=0 monsters=0 noGameplay=yes
```

Canonical fingerprints:

```text
descriptorFNV = 27115328
linkageFNV    = 5727902c
```

Full descriptor + linkage sweep:

```text
elapsed = 2 ms
```

## Important structural discovery: exact bytecode partition

The real CYD proved:

```text
total command references = 265
unique command indexes    = 265
overlap references        = 0
uncovered bytecodes       = 0
maximum commandEndIndex   = 265
```

Therefore the 93 MAP_INTRO events partition the complete 265-command bytecode stream **exactly once**:

```text
93 event command ranges
        -> 265 total references
        -> every bytecode referenced exactly once
        -> no holes
        -> no overlap
        -> final exclusive end = 265
```

This is a strong native format invariant for `/intro.bsp` and removes the need for any extra event-to-command ownership structure.

Observed event command-count range:

```text
min = 1
max = 14
```

## Observed initial state and event flags

Hardware result:

```text
stateMask = 0001
flagsMask = 03
```

The probe builds these masks as `1 << observedValue`, therefore:

```text
initial event states present = { 0 }
event flag values present    = { 0, 1 }
```

All 93 BSP events therefore begin in state 0. Runtime state changes remain a later mutable-overlay concern.

## Memory / integrity verdict

New persistent payload:

```text
0 B
```

Existing measured persistent foundation remains:

```text
14112 B immutable arena actual heap
+1040 B mutable tile state actual heap
--------------------------------------
15152 B total
```

Descriptor probe integrity:

```text
heap8       = 68992 -> 68992
largest8    = 36852 -> 36852
frameFNV    = bd237825 -> bd237825
arenaFNV    = c3882516 -> c3882516
stateFNV    = cd99b98e -> cd99b98e
script exec = no
entities    = 0
monsters    = 0
ST_PLAYING  = no
```

Later `[ALIVE]` heartbeat remained stable at `heap8=68992`, `largest8=36852`.

## Regression fingerprint chain

```text
source BSP FNV = d5cc751f
arenaFNV       = c3882516
decodedFNV     = a426dd18
stateFNV       = cd99b98e
lookupFNV      = 63430151
descriptorFNV  = 27115328
linkageFNV     = 5727902c
```

Each fingerprint protects a distinct layer: source bytes, compact physical representation, decoded records, mutable tile state, tile-event lookup, event descriptor semantics and event-command linkage.

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

## Merge recommendation

**MERGE `agent/esp32-map1-native-event-descriptor`.**

Hardware-tested code:

```text
b3e453baea1ac861c608f0c11b8aaa592f1cc3e5
```

Any later commits should remain documentation-only until merge; if so, no additional hardware flash is required.

## Next boundary after merge

Native code can now answer, allocation-free:

```text
which tile -> which event
which event -> exact descriptor
which event -> exact compact command range
which command offset -> exact EspMapByteCode
```

The next bounded milestone should recover **side-effect-free `Game_runEvent()` eligibility/filtering semantics**: current state, event flags, trigger/direction flags, block-input behavior and command `arg2` filters. A small separate mutable current-event-state overlay is likely required because the source arena contains only `initialState`.

Do not execute commands or mutate the world until that decision layer is hardware-proven.
