# ESP32 MAP_INTRO native script state + event eligibility filter

Branch: `agent/esp32-map1-native-event-filter`

Base merged `main`:

```text
PR   = #47 — native event descriptor / bytecode linkage
main = a3e629ba0be6b4dcc6329b17f18a0c3ca9828958
```

Status: **IMPLEMENTED; AWAITING REAL-CYD HARDWARE PASS**.

## Objective

Recover the pre-execution semantics of `Game_runEvent()` without executing a single event command.

This milestone adds:

1. a compact mutable script-state overlay outside the immutable map arena;
2. a pure side-effect-free command eligibility filter;
3. an exhaustive real-CYD oracle comparison across synthetic states, triggers and key combinations.

It still does **not** call `Game_executeEvent()`, mutate the world, activate entities, render gameplay or enter `ST_PLAYING`.

## Recovered `Game_runEvent()` filtering order

Reference behavior is reproduced in this order:

```text
1. EVENT_FLAG_BLOCKINPUT + run flag 0x400 -> reject whole event
2. choose state selector from current event state
3. inject one player-key execution bit by priority
4. skip command if state selector does not match arg2
5. skip command if arg2 key field does not equal selected key field
6. skip command if (effectiveFlags & arg2) == 0
7. otherwise the command WOULD be passed to Game_executeEvent()
```

No command is executed here.

### Event state selector

Recovered logic:

```text
state 0 -> selector 0
state 1 -> 0x00010000
state N -> 0x00010000 << (N - 1)
```

For state zero, commands carrying any of the legacy state bits `0x01ff0000` are skipped.

### Trigger/direction bits

Recovered constants:

```text
0x001 enter north
0x002 enter east
0x004 enter south
0x008 enter west
0x010 exit south
0x020 exit west
0x040 exit north
0x080 exit east
0x100 trigger
0x200 remove/modify-world command flag
```

The legacy `Game_eventFlagsForMovement()` produces the corresponding exit/enter directional pairs.

### Player-key execution bits

`Game_runEvent()` injects at most one key bit using this exact priority:

```text
red    key 0x08 -> run flag 0x8000
blue   key 0x04 -> run flag 0x4000
green  key 0x01 -> run flag 0x2000
yellow key 0x02 -> run flag 0x1000
```

The command key field is `arg2 & 0xf000`.

## Why mutable script state is separate

Two legacy mutations are now known:

```text
tileEvents[event].state bits 25..28 can change during gameplay
mapByteCode[command].arg2 becomes 0 after successful 0x200/remove behavior
```

Neither mutation belongs in immutable `EspMapRuntime`.

Permanent native component:

```text
ESP32/include/esp_map_script_state.h
ESP32/src/esp_map_script_state.c
```

MAP_INTRO compact payload plan:

```text
93 event states x 4 bits = ceil(93 / 2) = 47 B
265 removed-command bits = ceil(265 / 8) = 34 B
------------------------------------------------
total payload                              = 81 B
```

One `MALLOC_CAP_8BIT` allocation owns both regions.

Public API:

```text
EspMapScriptState_reset()
EspMapScriptState_buildFromRuntime()
EspMapScriptState_isReady()
EspMapScriptState_view()
EspMapScriptState_getEventState()
EspMapScriptState_setEventState()
EspMapScriptState_isCommandRemoved()
EspMapScriptState_setCommandRemoved()
```

Initial event states are decoded from descriptor `initialState`; removed-command bits start clear.

## Side-effect-free filter API

Permanent component:

```text
ESP32/include/esp_map_event_filter.h
ESP32/src/esp_map_event_filter.c
```

Public flow:

```text
EspMapEventFilter_prepare(descriptor,
                          currentState,
                          startCommandOffset,
                          inputFlags,
                          playerKeys,
                          &plan)

EspMapEventFilter_evaluate(descriptor,
                           &plan,
                           commandOffset,
                           removed,
                           &result)
```

Decision classes:

```text
ELIGIBLE
EVENT_BLOCKED
BEFORE_START
REMOVED
STATE_MISMATCH
KEY_MISMATCH
FLAGS_MISMATCH
```

`ELIGIBLE` means only that the reference `Game_runEvent()` would reach the `Game_executeEvent()` call for that command. This milestone never calls it.

The explicit `REMOVED` class is the native representation of the legacy condition where a previously successful `0x200` command had its mutable `arg2` zeroed. The immutable command payload remains untouched.

## Hardware probe

Temporary scaffold:

```text
ESP32/include/native_map1_event_filter_probe.h
ESP32/src/native_map1_event_filter_probe.c
```

It runs only after the hardware-proven descriptor/linkage probe.

### Script-state gates

The probe must establish:

```text
storageBytes        = 81
eventStateBytes     = 47
removedCommandBytes = 34
eventCount          = 93
byteCodeCount       = 265
all initial states  = 0
all removed bits    = 0
```

It measures actual heap consumption and allocator overhead.

It also performs a reversible ownership test:

```text
event 0 state: 0 -> 15 -> 0
command 0 removed: 0 -> 1 -> 0
```

The final script-state fingerprint must exactly return to the initial fingerprint.

No immutable map byte is touched.

### Exhaustive filter matrix

The permanent filter is compared against a separately coded reference oracle derived from `Game_runEvent()`.

Matrix:

```text
16 current states: 0..15
12 input flag contexts:
  0,
  8 directional single bits,
  trigger 0x100,
  block-input flag 0x400,
  trigger+block 0x500
8 key sets:
  none,
  each individual key,
  green+yellow,
  green+yellow+blue,
  all four keys
```

Total contexts per event:

```text
16 * 12 * 8 = 1536
```

Because the previous hardware pass proved that the 93 events partition exactly 265 commands, total command decisions are expected to be:

```text
265 * 1536 = 407040
```

For every decision, the probe compares:

```text
prepared effective flags
prepared state selector
event block-input result
global command index
command offset
command id
arg2
final decision class
```

The matrix establishes a canonical `filterFNV` and counts each skip/eligible reason.

### Resume/start-offset validation

For each of the 93 events the probe also tests start offsets:

```text
0
commandCount / 2
commandCount
```

against the same independent oracle and establishes `resumeFNV`.

Out-of-range state/start/command parameters must fail closed.

## Inherited hardware regressions

The candidate must retain:

```text
arenaFNV       = c3882516
decodedFNV     = a426dd18
stateFNV       = cd99b98e
lookupFNV      = 63430151
descriptorFNV  = 27115328
linkageFNV     = 5727902c
```

Current persistent foundation before this milestone:

```text
immutable map arena actual heap = 14112 B
mutable tile state actual heap  =  1040 B
----------------------------------------
combined                         = 15152 B
```

New planned persistent payload:

```text
script state = 81 B payload
filter       = 0 B persistent
```

The real CYD must establish the allocator cost of the 81-byte allocation.

## Expected log tail

```text
[MAPFILTERPROBE] ARMED ...

=== Doom RPG ESP32-native MAP_INTRO script state + event filtering ===
[MAPFILTERPROBE] CONTRACT ...

[MAPSCRIPT] READY bytes=81 eventStateBytes=47 removedBytes=34 events=93 byteCodes=265 initialFNV=........ buildElapsed=...ms
[MAPFILTER] READY filterFNV=........ resumeFNV=........ elapsed=...ms contexts=142848 evaluations=407040 eligible=... blocked=... stateSkip=... keySkip=... flagsSkip=... blockInputEvents=...
[MAPFILTERPROBE] MUTATION reversible=yes scriptFNV=........->........->........ removedFinal=0 statesRestored=yes
[MAPFILTERPROBE] RAM heap8=X->Y used=... payload=81 allocatorOverhead=... largest8=A->B filterDelta=0 frameFNV=F->F arenaFNV=c3882516->c3882516 mapStateFNV=cd99b98e->cd99b98e scriptFNV=........
[MAPFILTERPROBE] PARK state=9 page=3 nativeArena=yes nativeTileState=yes nativeEventLookup=yes nativeEventDescriptor=yes nativeScriptState=yes scriptBytes=81 filterReady=yes scriptExecution=no entities=0 monsters=0 noGameplay=yes
```

## Still forbidden

```text
Game_executeEvent() calls
actual command side effects
world/door/sprite/entity mutation
persistent command removal caused by execution
game-driven event-state transitions
save/resume continuation effects
entity/monster activation
player spawn
native gameplay rendering
ST_PLAYING
continuous gameplay loop
```

## Next boundary after hardware PASS

If this passes, the engine will possess all prerequisites for a first bounded native script execution experiment:

```text
tile -> event -> descriptor -> current state -> filtered command list
```

The next step should not execute all opcode families at once. Prefer a small executor-dispatch audit and a deliberately harmless first opcode/effect, with unsupported opcodes failing closed and PARK immediately after the bounded experiment.
