# ESP32 MAP_INTRO native CHECK_KEY gate milestone

Branch: `agent/esp32-map1-native-key-gate`

Base merged `main`:

```text
PR   = #54 — native NOTE notebook owner
main = 03002f79eb03bdcb4c9e430c43e4693dab47e44b
```

Firmware candidate content:

```text
3b4844e8fa5d38d522e1adc70ffac646978f130d
```

Status: **IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING**.

## Objective

Cross the first native dynamic script-control boundary after the UI/player owners:

```text
real EV_CHECK_KEY bytecode
 -> canonical event/command provenance
 -> caller-supplied key bitfield
 -> PASS or BLOCKED result
```

This milestone supports only opcode `41 / EV_CHECK_KEY`. It does not mutate legacy Player keys, Hud, Sound, Game continuation, world/entities/rendering, and it does not yet run a full native event loop.

## Recovered legacy behavior

`Game_executeEvent()` checks `arg1` as a key selector:

```text
0 -> green  -> bit 0x1 -> "Need Green Key"
1 -> yellow -> bit 0x2 -> "Need Yellow Key"
2 -> blue   -> bit 0x4 -> "Need Blue Key"
3 -> red    -> bit 0x8 -> "Need Red Key"
```

If the required key is present, the opcode returns `false` immediately. In `Game_runEvent()` that means this command produces no handled effect and command processing continues.

If the required key is absent, legacy behavior is:

```text
Hud_addMessage("Need <Color> Key")
Sound_playSound(5065, 0, 2)
saveTileEvent = true
Game_executeEvent returns true
```

`Game_runEvent()` then stores the **current command offset** in `tileEventIndex`, stores the active flags, clears the transient `saveTileEvent` latch and stops the event loop.

Therefore the permanent native semantics owned here are:

```text
key present:
  status = PASS
  legacy executeEvent return = false
  stop event = no
  save current command = no
  message/sound = none

key absent:
  status = BLOCKED
  legacy executeEvent return = true
  stop event = yes
  save current command = yes
  saved offset = source command offset
  message = exact key-color text
  sound = 5065
```

Malformed key selectors outside `0..3` are rejected fail-closed by the native evaluator rather than inheriting the desktop fallback behavior for corrupt bytecode.

## Permanent native API

New files:

```text
ESP32/include/esp_map_key_gate.h
ESP32/src/esp_map_key_gate.c
```

Result value:

```c
typedef struct EspMapKeyGateResult_s {
    uint16_t sourceEventIndex;
    uint16_t globalCommandIndex;
    uint16_t soundId;
    uint8_t sourceCommandOffset;
    uint8_t keyIndex;
    uint8_t requiredMask;
    uint8_t legacyReturnValue;
    uint8_t stopEvent;
    uint8_t saveCurrentCommand;
} EspMapKeyGateResult;
```

Expected classic ESP32 ABI footprint:

```text
result value      = 12 B
persistent owner  = none
persistent heap   = 0 B
pack I/O          = none
```

API:

```text
EspMapKeyGate_evaluate(descriptor, commandOffset, keyBits, outResult)
EspMapKeyGate_message(result)
```

`EspMapKeyGate_evaluate()` revalidates that the supplied descriptor is exactly canonical for the current immutable runtime before reading its command. It supports only real `EV_CHECK_KEY` bytecode with `arg1=0..3`.

The caller-provided `keyBits` uses the legacy low-bit layout `1,2,4,8`; higher bits are deliberately ignored.

The permanent implementation includes no legacy `Player`, `Hud`, `Sound`, `Game`, entity or render dependency.

## Why this milestone now

The remaining MAP_INTRO opcode IDs after the already-proven state/UI/player families include:

```text
2  EV_CHANGEMAP
7  EV_SHOW
9  EV_GIVEMAP
10 EV_PASSWORD
13 EV_UNLOCK
15 EV_OPENLINE
16 EV_CLOSELINE
18 EV_HIDE
27 EV_SAVEGAME
41 EV_CHECK_KEY
```

Most of these cross into world/render/save/map-transition ownership. `EV_CHECK_KEY` instead introduces a real dynamic branch decision required by the future native event loop while keeping the current no-world-mutation boundary intact.

`EV_PASSWORD` remains a future pause/input owner candidate but is not included here.

## Temporary hardware probe

New files:

```text
ESP32/include/native_map1_key_gate_probe.h
ESP32/src/native_map1_key_gate_probe.c
```

The probe runs after the hardware-proven NOTE owner stage.

For every real `EV_CHECK_KEY` in the canonical `93 event / 265 bytecode` MAP_INTRO corpus, it evaluates all sixteen low-nibble key contexts:

```text
keyBits = 0 .. 15
```

For any one key selector, exactly half of these contexts contain the key and half do not. Therefore acceptance is independent of the not-yet-canonical real CHECK_KEY count:

```text
refs > 0
scenarios = refs * 16
PASS      = refs * 8
BLOCKED   = refs * 8
state-only opcode executor refuses every ref
color counts sum exactly to refs
```

The first real-CYD PASS will establish rather than predeclare:

```text
real CHECK_KEY ref count
green/yellow/blue/red ref distribution
first canonical CHECK_KEY sample
keyGateFNV
new-build heap/framebuffer absolute values
legacy Player.keys baseline value
Hud witness FNV
```

The complete bytecode corpus is already independently protected by hardware-proven `opcodeAuditFNV=6f28df45`, so discovering the family count here does not weaken source-corpus integrity.

## Table / side-effect proof

For each real command and each of 16 key contexts the probe requires exact output metadata:

```text
source event/global command/source offset
required key index and mask
PASS vs BLOCKED
legacy executeEvent return bit
stop-event bit
save-current-command bit
sound 5065 only when blocked
exact color message only when blocked
```

It separately proves:

```text
4 / 4 exact key messages
higher keyBits ignored
PASS has no effect metadata
BLOCKED carries message+sound+stop+save-current
```

## Fail-closed proof

The probe requires:

```text
non-CHECK_KEY source command -> UNSUPPORTED + zero result
out-of-range command offset  -> INVALID + zero result
non-canonical descriptor     -> INVALID + zero result
NULL descriptor              -> INVALID + zero result
NULL result                  -> INVALID
```

No persistent owner exists in this milestone; the 12-byte result is caller-local and overwritten on each evaluation.

## Hardware integrity boundary

Before and after the complete probe:

```text
heap8
largest8
framebuffer FNV
arenaFNV      = c3882516
mapStateFNV   = cd99b98e
scriptFNV     = f9e3d9df
legacy Player.NotebookString FNV = 4d7705c5
legacy Player.keys
Hud message witness FNV
Game.skipAdvanceTurn
Game.saveTileEvent
Game.tileEvent
Game.tileEventIndex
Game.tileEventFlags
```

must remain exact.

The pack must remain closed throughout the stage.

Permanent prohibitions remain:

```text
legacy key mutation               = no
legacy Hud mutation               = no
legacy Game continuation mutation = no
world/entity/render mutation      = no
map transitions                   = no
actual sound playback             = no
full native Game_runEvent loop    = no
entities                           = 0
monsters                           = 0
ST_PLAYING                         = no
shapeData                          = NULL
mediaTexels                        = NULL
```

## Expected Serial family

```text
[MAPKEYPROBE] ARMED ...

=== Doom RPG ESP32-native MAP_INTRO CHECK_KEY gate ===
[MAPKEYPROBE] CONTRACT ...
[MAPKEY] READY refs=... green=... yellow=... blue=... red=... scenarios=... pass=... blocked=... resultBytes=12 stateExecRefused=... keyGateFNV=... elapsed=...ms
[MAPKEY] SAMPLE cmd=... event=... off=... key=... mask=... arg2=... missingMessage="Need ... Key" sound=5065 saveOffset=current
[MAPKEY] TABLE perRef=16 passEach=8 blockedEach=8 messages=4/4 extraBitsIgnored=1 passEffect=none blockedEffect=message+sound+stop+saveCurrent
[MAPKEY] FAILCLOSED unsupported=1 badOffset=1 badDescriptor=1 nullDescriptor=1 nullResult=1
[MAPKEYPROBE] RAM ... legacyNotebookFNV=4d7705c5->4d7705c5 legacyKeys=...->... hudFNV=...->...
[MAPKEYPROBE] PARK ... nativeKeyGate=yes resultBytes=12 persistentBytes=0 ...
[ALIVE] ...
```

Use the normal optimized PlatformIO environment:

```text
esp32-cyd
```

No CI status is currently published for the firmware candidate. Do not claim a local build or hardware PASS until the real classic CYD supplies it.

## Next boundary after a hardware PASS

Do not preselect it. After PASS + merge, reread the then-current `main`, recovery docs, this milestone and the exact remaining MAP_INTRO legacy behavior before choosing between password/input ownership and the first bounded world mutation family.
