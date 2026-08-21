# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for stable build/architecture guidance, [`DOCUMENTATION.md`](DOCUMENTATION.md) for documentation ownership rules, and milestone documents for detailed hardware evidence.

The older full recovery catalog remains preserved in [`archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md`](archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md).

## Latest merged hardware baseline

```text
PR   = #47 — native MAP_INTRO event descriptor / bytecode linkage
main = a3e629ba0be6b4dcc6329b17f18a0c3ca9828958
```

Current candidate:

```text
branch = agent/esp32-map1-native-event-filter
base   = a3e629ba0be6b4dcc6329b17f18a0c3ca9828958
status = REAL-CYD HARDWARE PASS; SCRIPT STATE + SIDE-EFFECT-FREE EVENT FILTER VALIDATED; MERGE-READY
```

Detailed active milestone: [`MAP1_NATIVE_EVENT_FILTER.md`](MAP1_NATIVE_EVENT_FILTER.md).

Merged descriptor evidence: [`MAP1_NATIVE_EVENT_DESCRIPTOR.md`](MAP1_NATIVE_EVENT_DESCRIPTOR.md).

## Permanent target / ownership

```text
board        = ESP32-2432S028R classic CYD
MCU          = ESP32-D0WD-V3 dual-core 240 MHz
flash        = 4 MB
PSRAM        = none
display      = ILI9341 320x240 landscape
touch        = XPT2046
storage      = microSD
framebuffer  = 160x120 RGB565 = 38400 B
presentation = exact nearest-neighbor 2x
audio        = deferred
```

Permanent resource invariant:

```text
shapeData   == NULL
mediaTexels == NULL
```

Permanent engine ownership:

```text
DoomRPG-RE = executable specification / format + behavior reference
final CYD engine = our ESP32-native engine
```

Desktop-derived `Render_t`, `DoomCanvas_t`, pointer-heavy map structures and linker wrappers remain migration scaffolding, not permanent architecture requirements.

Current native direction:

```text
Doom RPG source data
    -> EspBspReader
    -> immutable compact EspMapRuntime
    -> allocation-free native accessors
    -> explicit mutable EspMapState
    -> allocation-free EspMapEvents tile lookup
    -> read-only event descriptor / bytecode linkage
    -> compact mutable EspMapScriptState
    -> side-effect-free Game_runEvent eligibility/filtering
    -> bounded fail-closed opcode execution experiments
    -> native gameplay + renderer
```

## Current hardware-safe boundary

Normal optimized firmware now reaches:

```text
menu                    = MENU_NONE
state                   = ST_INTRO (9)
storyPage               = 3
storyTextPage           = 0
startupMap              = 1 (MAP_INTRO / /intro.bsp)
intro clock/input       = inactive
intro images/texts      = NULL
legacy nodes/lines      = NULL
legacy mapSprites       = NULL
legacy mappings         = NULL
shapeData               = NULL
mediaTexels             = NULL
wall/sprite LRU caches  = inactive
entities/monsters       = 0
legacy gameplay loader  = NOT called

native map arena        = RESIDENT + IMMUTABLE
arena payload           = 14095 B
actual arena heap       = 14112 B
arenaFNV                = c3882516

native tile state       = RESIDENT + MUTABLE
tile payload            = 1024 B
actual tile heap        = 1040 B
stateFNV                = cd99b98e

native event lookup     = READY + ALLOCATION-FREE
lookup persistent bytes = 0
lookupFNV               = 63430151

native descriptors      = READY + ALLOCATION-FREE
descriptorFNV           = 27115328
linkageFNV              = 5727902c

native script state     = RESIDENT + MUTABLE
script payload          = 81 B
actual script heap      = 100 B
scriptFNV               = f9e3d9df

native event filter     = READY + SIDE-EFFECT-FREE
filter persistent bytes = 0
filterFNV               = a5923b21
resumeFNV               = b98452da

combined native heap    = 15252 B
heap8                   = 68844 on current hardware run
largest8                = 36852
ST_PLAYING              = NOT entered
```

Absolute `heap8` may move slightly between builds as firmware code grows. Regression is based on exact ownership, measured allocation cost, largest-block integrity, fingerprints and zero drift inside allocation-free stages.

## MAP_INTRO source reference

```text
file        = /intro.bsp
name        = Entrance
sourceBytes = 21823
CRC32       = 623f34e4
FNV-1a      = d5cc751f
nodes       = 223
lines       = 480
mapSprites  = 344
events      = 93
byteCodes   = 265
strings     = 94
stringData  = 7779 B
maxString   = 313 B
```

## Native foundation — hardware validated

### Immutable compact arena

```text
payload                = 14095 B
actual heap use        = 14112 B
allocator overhead     = 17 B
populateReadCalls      = 33
populateElapsed        = 61–63 ms across tested builds
arenaFNV               = c3882516
largest8               = 36852 preserved
```

### Allocation-free access contract

```text
decodedFNV = a426dd18
```

The real CYD swept every node, line, map sprite, event, bytecode, string offset, block cell, plane cell, resource ID and bounds family with zero drift.

### Mutable tile state

```text
payload            = 1024 B
actual heap use    = 1040 B
allocator overhead = 16 B
stateFNV           = cd99b98e
build/verify       = 9 ms
largest8           = 36852 preserved
```

Spatial topology:

```text
block base             = 298 / 697 / 27 / 2
texture-7 entrances    = 4 refs / 4 unique cells
first entrance tile    = 68
event-bearing cells    = 93 refs / 93 unique cells
first event tile       = 68
visited cells          = 0
```

### Allocation-free tile -> event lookup

```text
events       = 93
sorted       = yes
unique tiles = yes
first        = 68 / 0 / 00080044
last         = 968 / 92 / 000c23c8
queries      = 1024
hits/misses  = 93 / 931
elapsed      = 5 ms
lookupFNV    = 63430151
persistent   = 0 B
```

### Event descriptor / bytecode linkage

Recovered event layout:

```text
bits  0..9   tile
bits 10..18  first compact command index
bits 19..24  command count
bits 25..28  initial event state
bits 29..31  event flags
```

Real-CYD topology:

```text
descriptorFNV = 27115328
linkageFNV    = 5727902c
elapsed       = 2 ms
persistent    = 0 B
commandRefs   = 265
unique        = 265
overlaps      = 0
gaps          = 0
countRange    = 1..14
maxEnd        = 265
initial states= {0}
event flags   = {0,1}
```

The 93 event command ranges partition the entire 265-command stream exactly once.

## Mutable script state — hardware validated

Recovered mutable ownership replaces two legacy in-place mutations:

```text
event current state             -> 4 bits/event
successful 0x200 command removal-> 1 bit/bytecode
```

Compact storage:

```text
93 event states      = 47 B
265 removal bits     = 34 B
payload              = 81 B
actual heap          = 100 B
allocator overhead   = 19 B
buildElapsed         = 1 ms
initialFNV           = f9e3d9df
```

Reversible ownership proof:

```text
event 0 state       0 -> 15 -> 0
command 0 removed   0 -> 1 -> 0
scriptFNV           f9e3d9df -> 99003167 -> f9e3d9df
```

No immutable map byte changed.

## Side-effect-free `Game_runEvent()` filter — hardware validated

Permanent APIs:

```text
EspMapEventFilter_prepare()
EspMapEventFilter_evaluate()
```

The filter reproduces pre-execution behavior only:

```text
EVENT_FLAG_BLOCKINPUT
current event state selector
player-key priority red -> blue -> green -> yellow
arg2 state mask
arg2 key field
trigger/direction/input flag match
resume/start command offset
previously removed-command state
```

It never calls `Game_executeEvent()`.

Exhaustive real-CYD matrix:

```text
states             = 16
run-flag contexts  = 12
key sets           = 8
contexts/event     = 1536
total contexts     = 142848
total evaluations  = 407040
```

Exact decisions:

```text
eligible       = 4784
eventBlocked   = 2048
stateMismatch  = 379392
keyMismatch    = 0
flagsMismatch  = 20816
----------------------
total          = 407040
```

Additional facts:

```text
blockInputEvents = 8
filterFNV        = a5923b21
resumeFNV        = b98452da
probe elapsed    = 1411 ms
filter heap drift= 0 B
```

`keyMismatch=0` is an observed property of the MAP_INTRO command corpus under the exhaustive matrix, not a removed code path. Player-key injection is still part of the recovered filter contract.

The 1411 ms measurement belongs to the exhaustive test oracle/hashing sweep; it is not the runtime cost of one event.

Integrity result:

```text
heap8        = 68944 -> 68844 only for 100 B script-state allocation
largest8     = 36852 -> 36852
frameFNV     = b8924a47 -> b8924a47
arenaFNV     = c3882516 -> c3882516
mapStateFNV  = cd99b98e -> cd99b98e
scriptFNV    = f9e3d9df final
scriptExec   = no
entities     = 0
monsters     = 0
ST_PLAYING   = no
```

Later `[ALIVE]` stayed stable at `heap8=68844`, `largest8=36852`.

## Persistent RAM comparison

```text
immutable map arena = 14112 B actual heap
mutable tile state  =  1040 B actual heap
mutable script state=   100 B actual heap
------------------------------------------
current total       = 15252 B
```

Measured legacy structural allocation:

```text
55341 B
```

Current native foundation remains `40089 B` smaller (~72.4%) while already owning immutable map structure, mutable spatial state and mutable script state.

## Native regression fingerprints

```text
source BSP FNV = d5cc751f
arenaFNV       = c3882516
decodedFNV     = a426dd18
stateFNV       = cd99b98e
lookupFNV      = 63430151
descriptorFNV  = 27115328
linkageFNV     = 5727902c
scriptFNV      = f9e3d9df
filterFNV      = a5923b21
resumeFNV      = b98452da
```

## Execution path now proven

```text
validated intro disposal
    -> native BSP inventory/plan
    -> compact resident arena
    -> allocation-free native accessor sweep
    -> native mutable tile-state build
    -> allocation-free tile-event lookup sweep
    -> allocation-free event descriptor + command-link sweep
    -> compact mutable script-state build
    -> exhaustive side-effect-free event eligibility/filter sweep
    -> PARK
```

Still absent:

```text
shapeData/mediaTexels
legacy Render.beginLoadMapData ownership
legacy Render.tileEvents/mapByteCode ownership
actual native opcode side effects
game-driven event state transitions
world/door/sprite/entity mutation
entity/monster activation
player spawn
native gameplay rendering
ST_PLAYING
continuous gameplay loop
```

## Merge recommendation

**MERGE `agent/esp32-map1-native-event-filter`.**

The real CYD validated the 81-byte mutable script overlay and all 407040 side-effect-free filter decisions. Documentation commits after the hardware run do not require another flash as long as firmware files remain unchanged.

## Next bounded milestone after merge

Native code can now answer:

```text
which tile?
 -> which event?
 -> which command range?
 -> what is the current event state?
 -> for this trigger/key context, which commands WOULD execute?
```

Next, audit the opcode IDs actually present in MAP_INTRO and classify each by side-effect/risk. Build a tiny fail-closed native executor dispatcher and hardware-prove **one deliberately harmless opcode/effect first**. Unsupported or world-mutating opcodes must remain refused. Do not enable the full script engine, entities or `ST_PLAYING` in one jump.
