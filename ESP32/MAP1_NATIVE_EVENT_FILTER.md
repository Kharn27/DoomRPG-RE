# ESP32 MAP_INTRO native script state + event eligibility filter

Branch: `agent/esp32-map1-native-event-filter`

Base merged `main`:

```text
PR   = #47 — native event descriptor / bytecode linkage
main = a3e629ba0be6b4dcc6329b17f18a0c3ca9828958
```

Status: **REAL-CYD HARDWARE PASS; SCRIPT STATE + SIDE-EFFECT-FREE EVENT FILTER VALIDATED; MERGE-READY**.

## Objective

Recover the pre-execution semantics of `Game_runEvent()` without executing a single event command.

This milestone adds:

1. a compact mutable script-state overlay outside the immutable map arena;
2. a pure side-effect-free command eligibility filter;
3. an exhaustive real-CYD oracle comparison across synthetic states, triggers and key combinations.

It still does **not** call `Game_executeEvent()`, mutate the world, activate entities, render gameplay or enter `ST_PLAYING`.

## Real-CYD result

The normal optimized `esp32-cyd` firmware passed the full probe on the classic no-PSRAM CYD.

Measured script-state allocation:

```text
payload             = 81 B
event state bytes   = 47 B
removed bits bytes  = 34 B
actual heap use     = 100 B
allocator overhead  = 19 B
buildElapsed        = 1 ms
initialFNV          = f9e3d9df
mutatedFNV          = 99003167
restoredFNV         = f9e3d9df
```

Measured exhaustive filter matrix:

```text
contexts         = 142848
evaluations      = 407040
eligible         = 4784
eventBlocked     = 2048
stateMismatch    = 379392
keyMismatch      = 0
flagsMismatch    = 20816
blockInputEvents = 8
filterFNV        = a5923b21
resumeFNV        = b98452da
elapsed          = 1411 ms
```

Decision accounting is exact:

```text
4784 + 2048 + 379392 + 0 + 20816 = 407040
```

The 1411 ms value is the intentionally huge verification matrix plus independent oracle and hashing work. It is **not** a gameplay-frame or single-event execution cost.

The real MAP_INTRO corpus produced `keyMismatch=0`. Key ownership still changes `effectiveFlags` according to the recovered priority, but none of the 407040 tested MAP_INTRO decisions landed in the dedicated key-field mismatch class.

Integrity stayed exact:

```text
heap8          = 68944 -> 68844
heap used      = 100 B
largest8       = 36852 -> 36852
filter heap    = +0 B after script-state allocation
frameFNV       = b8924a47 -> b8924a47
arenaFNV       = c3882516 -> c3882516
mapStateFNV    = cd99b98e -> cd99b98e
scriptFNV      = f9e3d9df final
scriptExecution= no
entities       = 0
monsters       = 0
ST_PLAYING     = no
```

Later `[ALIVE]` remained stable at `heap8=68844`, `largest8=36852`.

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

## Mutable script state ownership

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

MAP_INTRO compact payload:

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

The real-CYD reversible ownership test passed:

```text
event 0 state       0 -> 15 -> 0
command 0 removed   0 -> 1 -> 0
scriptFNV           f9e3d9df -> 99003167 -> f9e3d9df
```

The immutable map arena stayed `c3882516` throughout.

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

`ELIGIBLE` means only that the recovered `Game_runEvent()` would reach the `Game_executeEvent()` call for that command. This milestone never calls it.

The explicit `REMOVED` class is the native representation of the legacy condition where a previously successful `0x200` command had its mutable `arg2` zeroed. The immutable command payload remains untouched.

## Exhaustive hardware oracle

The permanent filter was compared against a separately coded reference oracle derived from `Game_runEvent()`.

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

Previous hardware proved that the 93 events partition exactly 265 commands, therefore:

```text
93 * 1536  = 142848 event contexts
265 * 1536 = 407040 command evaluations
```

For every decision, the probe compared:

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

Canonical hardware fingerprints:

```text
filterFNV = a5923b21
resumeFNV = b98452da
```

Resume/start-offset checks were also validated for each event with offsets:

```text
0
commandCount / 2
commandCount
```

Out-of-range state/start/command parameters failed closed.

## Inherited native fingerprints

All inherited layers remained unchanged:

```text
arenaFNV       = c3882516
decodedFNV     = a426dd18
stateFNV       = cd99b98e
lookupFNV      = 63430151
descriptorFNV  = 27115328
linkageFNV     = 5727902c
```

This milestone adds:

```text
scriptFNV      = f9e3d9df
filterFNV      = a5923b21
resumeFNV      = b98452da
```

## Persistent RAM boundary

Previously measured native map/world heap:

```text
immutable map arena actual heap = 14112 B
mutable tile state actual heap  =  1040 B
----------------------------------------
combined                         = 15152 B
```

New real-CYD script-state allocation:

```text
payload            = 81 B
actual heap use    = 100 B
allocator overhead = 19 B
filter persistent  = 0 B
```

New combined persistent map/world/script heap:

```text
15152 B + 100 B = 15252 B
```

Measured legacy structural allocation was `55341 B`; even with native mutable tile + script state, the current native foundation remains `40089 B` smaller (~72.4%).

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

## Next boundary after PASS

The engine now has all prerequisites for a first bounded native script execution experiment:

```text
tile
 -> event
 -> descriptor
 -> current state
 -> filtered command list
 -> known ELIGIBLE/SKIP decision
```

The next step must **not** enable every opcode family at once. Audit the opcode IDs actually present in MAP_INTRO, classify them by side-effect/risk, implement a tiny fail-closed executor dispatcher, and choose one deliberately harmless opcode/effect as the first real execution proof. Unsupported or world-mutating opcodes must remain refused and the probe must PARK immediately after the bounded experiment.
