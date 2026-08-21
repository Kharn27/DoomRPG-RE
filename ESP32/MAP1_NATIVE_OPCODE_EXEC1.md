# ESP32 MAP_INTRO native opcode inventory + first bounded execution

Branch: `agent/esp32-map1-native-opcode-exec1`

Base merged `main`:

```text
PR   = #48 — native script state + side-effect-free Game_runEvent filtering
main = 0c8a52549ebb436139f7cd5c8b4ee63bdd175907
```

Status: **REAL-CYD HARDWARE PASS; FIRST REAL NATIVE DOOM RPG OPCODE EXECUTED + ROLLED BACK; MERGE-READY**.

## Objective

Cross the native script-execution boundary for the first time without opening the full legacy `Game_executeEvent()` surface.

This milestone:

1. audits every real opcode ID in the 265 compact MAP_INTRO bytecodes;
2. executes one real BSP command from a deliberately tiny fail-closed opcode family;
3. proves the mutation is confined to `EspMapScriptState`;
4. rolls the mutation back before PARK.

No world, door, line, sprite, entity, HUD, sound, renderer, player or `ST_PLAYING` effect is allowed.

## Real-CYD hardware result

The normal optimized `esp32-cyd` firmware passed the complete probe on the classic no-PSRAM CYD.

### Complete MAP_INTRO opcode inventory

```text
bytecode refs = 265
unique IDs    = 16
idMaskLo      = 0d0daf84
idMaskHi      = 00000300
outOfRange    = 0
opcodeAuditFNV= 6f28df45
```

The masks decode to exactly these opcode IDs:

```text
2, 7, 8, 9, 10, 11, 13, 15,
16, 18, 19, 24, 26, 27, 40, 41
```

All real IDs stay inside the recovered legacy range `1..42`.

State-opcode references:

```text
EV_CHANGESTATE (11) = 41 refs
EV_NEXTSTATE   (19) = 35 refs
EV_PREVSTATE   (20) =  0 refs
--------------------------------
supported family     = 76 refs
```

### First real native opcode execution

The first selected real supported BSP bytecode was:

```text
command index = 50
opcode        = 19 / EV_NEXTSTATE
arg1          = 00000702
arg2          = 00000100
target tile   = 226
target event  = 16
prepared state= 0
state result  = 0 -> 1
mutated       = yes
execFNV       = 646b565c
```

This is a real command from `/intro.bsp`, not a synthetic opcode.

The native executor therefore performed the first actual Doom RPG script mutation owned by the ESP32-native engine:

```text
real compact bytecode
 -> native opcode dispatcher
 -> EspMapScriptState event 16
 -> state 0 -> 1
```

The immutable map arena was never modified.

### Rollback and fail-closed proof

The probe restored the script overlay before PARK.

```text
initial scriptFNV = f9e3d9df
prepared scriptFNV= f9e3d9df
executed scriptFNV= 9b636dec
restored scriptFNV= f9e3d9df
rollback           = yes
```

The execution-result fingerprint is independently:

```text
execFNV = 646b565c
```

A real unsupported command was also refused:

```text
unsupported sample command = 0
unsupported opcode         = 16 / EV_CLOSELINE
status                     = UNSUPPORTED
mutation                   = none
```

A synthetic malformed `EV_CHANGESTATE` requesting state 16 was refused:

```text
invalidState = refused
mutation     = none
```

### Timing and integrity

```text
probe elapsed = 1 ms

heap8       = 68828 -> 68828
largest8    = 36852 -> 36852
frameFNV    = 10f53ffb -> 10f53ffb
arenaFNV    = c3882516 -> c3882516
mapStateFNV = cd99b98e -> cd99b98e
scriptFNV   = f9e3d9df -> f9e3d9df
```

No new persistent allocation was introduced.

Later `[ALIVE]` lines remained stable at:

```text
heap8    = 68828
largest8 = 36852
```

and continued reporting SD/ZIP/VIDEO/CORE/LAYOUT/PRERENDER/RENDER/MAPPINGS/MENUBSP ready.

Final PARK boundary:

```text
nativeArena           = yes
nativeTileState       = yes
nativeEventLookup     = yes
nativeEventDescriptor = yes
nativeScriptState     = yes
nativeFilter          = yes
nativeOpcodeExec      = yes
supportedOpcodes      = 3
worldMutation         = no
framebufferMutation   = no
entities              = 0
monsters              = 0
noGameplay            = yes
```

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

Every other opcode returns `ESP_MAP_OPCODE_EXEC_UNSUPPORTED` and performs no mutation.

Other fail-closed statuses:

```text
INVALID
TARGET_NOT_FOUND
STATE_OUT_OF_RANGE
OK
```

`EV_CHANGESTATE` is constrained to the native 4-bit state domain `0..15`. A malformed source value outside that domain is refused instead of corrupting event flags.

The result records code ID, args, target tile/event, state before/after, mutation and status.

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

The probe sweeps all 265 real compact bytecodes, validates the opcode domain, selects a real supported state command, proves one actual mutation, tests unsupported/malformed refusal and rolls back before PARK.

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
opcodeAuditFNV        = 6f28df45
firstExecFNV          = 646b565c

combined native heap  = 15252 B
largest8              = 36852
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

## Next boundary after merge

The measured real MAP_INTRO corpus now gives a concrete expansion list rather than guesses.

The next milestone should audit the semantics and ownership requirements of the remaining real IDs:

```text
2, 7, 8, 9, 10, 13, 15, 16,
18, 24, 26, 27, 40, 41
```

Add only one coherent effect family at a time. Prefer effects representable in small native state/intent objects before touching entities or rendering. Unsupported opcodes must continue to fail closed.
