# ESP32 MAP_INTRO native opcode inventory + first bounded execution

Branch: `agent/esp32-map1-native-opcode-exec1`

Base merged `main`:

```text
PR   = #48 — native script state + side-effect-free Game_runEvent filtering
main = 0c8a52549ebb436139f7cd5c8b4ee63bdd175907
```

Status: **IMPLEMENTED; AWAITING REAL-CYD HARDWARE PASS**.

## Objective

Cross the native script-execution boundary for the first time without opening the full legacy `Game_executeEvent()` surface.

This milestone does two things:

1. audit every real opcode ID in the 265 compact MAP_INTRO bytecodes;
2. execute one real BSP command from a deliberately tiny fail-closed opcode family, then roll the mutation back before PARK.

No world, door, line, sprite, entity, HUD, sound, renderer, player or `ST_PLAYING` effect is allowed.

## Why event-state opcodes are first

Recovered `Game_executeEvent()` groups these three opcodes:

```text
EV_CHANGESTATE = 11
EV_NEXTSTATE   = 19
EV_PREVSTATE   = 20
```

Their legacy effect is contained:

```text
targetTile = (arg1 & 0xff) + (((arg1 >> 8) & 0xff) * 32)
find target event by targetTile
read current event state

11: state = (arg1 >> 16) & 0xff
19: if state < 9 -> state++
20: if state > 0 -> state--

rewrite event state bits
```

The native architecture already moved mutable event state out of immutable event records and into `EspMapScriptState`, so this family can be implemented faithfully without touching the map arena.

## Permanent native executor

Files:

```text
ESP32/include/esp_map_opcode_executor.h
ESP32/src/esp_map_opcode_executor.c
```

Supported opcodes:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

Every other opcode returns:

```text
ESP_MAP_OPCODE_EXEC_UNSUPPORTED
```

and performs no mutation.

Other fail-closed statuses:

```text
INVALID
TARGET_NOT_FOUND
STATE_OUT_OF_RANGE
OK
```

`EV_CHANGESTATE` is additionally constrained to the native 4-bit state domain `0..15`. A malformed source value outside that domain is refused instead of corrupting event flags.

The result records:

```text
codeId
arg1 / arg2
target tile
target event index
state before
state after
mutated yes/no
status
```

New persistent memory:

```text
0 B
```

The executor mutates only the already resident 81-byte `EspMapScriptState`.

## Temporary hardware probe

Files:

```text
ESP32/include/native_map1_opcode_exec_probe.h
ESP32/src/native_map1_opcode_exec_probe.c
```

It runs only after the hardware-proven event-filter stage.

### Full opcode inventory

The probe sweeps all 265 real compact bytecodes and establishes:

```text
unique opcode ID count
ID mask 0..31
ID mask 32..63
out-of-range references outside legacy IDs 1..42
references to opcode 11
references to opcode 19
references to opcode 20
canonical opcode audit FNV
```

Any bytecode ID outside the recovered legacy range `1..42` fails the hardware gate.

### Real execution candidate

The probe selects an actual compact MAP_INTRO command from the supported family, preferring:

```text
19 EV_NEXTSTATE
11 EV_CHANGESTATE
20 EV_PREVSTATE
```

The candidate must target a real event. `EV_CHANGESTATE` must also request a state <= 15.

If MAP_INTRO contains no valid real command from this first safe family, the probe fails closed and nothing else is executed.

### Guaranteed visible mutation of script state

Because all 93 MAP_INTRO events currently begin at state 0, the probe prepares only the target event so that the selected real opcode is guaranteed to change state:

```text
NEXTSTATE   -> prepare 0, expect 1
PREVSTATE   -> prepare 1, expect 0
CHANGESTATE -> prepare a value different from its real requested state
```

Then:

```text
execute real BSP command
verify exact target + before/after state
hash execution result
restore original event state
require scriptFNV == f9e3d9df again
```

The preparation and execution fingerprints are intentionally left for real hardware to establish.

### Unsupported and malformed safety tests

The probe also chooses a real unsupported MAP_INTRO bytecode and requires:

```text
status = UNSUPPORTED
script state unchanged
```

It then synthesizes only a malformed `EV_CHANGESTATE` request for state 16 and requires:

```text
status = STATE_OUT_OF_RANGE
script state unchanged
```

NULL input/output cases must return `INVALID`.

## Inherited hardware boundary

```text
arena payload / heap  = 14095 / 14112 B
arenaFNV              = c3882516

tile state payload    = 1024 B
tile state heap       = 1040 B
mapStateFNV           = cd99b98e

script state payload  = 81 B
script state heap     = 100 B
scriptFNV             = f9e3d9df

lookupFNV             = 63430151
descriptorFNV         = 27115328
linkageFNV            = 5727902c
filterFNV             = a5923b21
resumeFNV             = b98452da

combined native heap  = 15252 B
largest8              = 36852 on previous run
```

The new executor/probe must add no persistent allocation.

## Hardware integrity gate

Across opcode audit + real execution + rollback:

```text
heap8        X -> X
largest8     Y -> Y
frameFNV     F -> F
arenaFNV     c3882516 -> c3882516
mapStateFNV  cd99b98e -> cd99b98e
scriptFNV    f9e3d9df -> temporary mutation -> f9e3d9df
entities     0
monsters     0
ST_PLAYING   no
```

## Expected log tail

```text
[MAPOPCODEPROBE] ARMED event filtering proven; opcode inventory + first reversible native execution starts on next loop service

=== Doom RPG ESP32-native MAP_INTRO opcode audit + first execution ===
[MAPOPCODEPROBE] CONTRACT audit all 265 real opcodes; execute only EV_CHANGESTATE/NEXTSTATE/PREVSTATE against 81B script overlay; unsupported fail closed; rollback before PARK

[MAPOPCODE] AUDIT refs=265 uniqueIds=... idMaskLo=........ idMaskHi=........ outOfRange=0 stateRefs=... change=... next=... prev=... auditFNV=........
[MAPOPCODE] EXEC command=... id=... arg1=........ arg2=........ target=.../... prepared=... state=...->... mutated=yes execFNV=........
[MAPOPCODEPROBE] READY elapsed=...ms supportedRefs=... unsupportedSample=.../... invalidState=refused rollback=yes scriptFNV=f9e3d9df->........->........->f9e3d9df
[MAPOPCODEPROBE] RAM heap8=X->X delta=0 largest8=Y->Y delta=0 frameFNV=F->F arenaFNV=c3882516->c3882516 mapStateFNV=cd99b98e->cd99b98e scriptFNV=f9e3d9df->f9e3d9df
[MAPOPCODEPROBE] PARK ... nativeOpcodeExec=yes supportedOpcodes=3 worldMutation=no framebufferMutation=no entities=0 monsters=0 noGameplay=yes
```

## Still forbidden

```text
full Game_runEvent native execution loop
all non-state opcode families
world/door/line/sprite mutation
message/HUD/dialog effects
sound
player/stat/inventory mutation
map transitions
entity/monster activation
renderer activation
gameplay framebuffer mutation
ST_PLAYING
```

## Next boundary after hardware PASS

The hardware inventory will tell us exactly which opcode IDs MAP_INTRO actually uses and how many real state-opcode commands exist.

If the first execution passes, the next milestone should expand the executor by **one coherent effect family at a time**, chosen from the measured real corpus. Prefer native effects that can be represented in small explicit overlays before touching entities or rendering.
