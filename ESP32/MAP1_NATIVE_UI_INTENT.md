# ESP32 MAP_INTRO native UI/string intent milestone

Branch: `agent/esp32-map1-native-ui-intent`

Base merged `main`:

```text
PR   = #49 — first fail-closed native opcode execution
main = 6e43ef059db52783b7264e84579216cb2572a1e2
```

Hardware-tested firmware content:

```text
045b219dd7d6d06630eb446424e8d3d3fa3d249e
```

Status: **REAL-CYD HARDWARE PASS; ALLOCATION-FREE NATIVE STRING SPANS + UI INTENTS VALIDATED; MERGE-READY**.

## Objective

Expand native script ownership with one coherent non-world-mutating family:

```text
8  EV_DIALOG
24 EV_FORCEMESSAGE
26 EV_DIALOGNOBACK
40 EV_NOTE
```

These commands do not call legacy `DoomCanvas`, `Hud` or `Player`. They translate to compact caller-owned native intents preserving the recovered semantics and exact map-string linkage.

The existing state-mutating opcode executor remains unchanged and continues to support only `11/19/20`. UI opcodes still return `UNSUPPORTED` there.

## Recovered legacy semantics

### EV_DIALOG (8)

```text
text = mapStringsIDs[arg1]
start dialog with Back softkey
save tile-event continuation
skip advance turn
resume at following command when dialog closes
```

### EV_DIALOGNOBACK (26)

Same as `EV_DIALOG`, except no Back softkey.

### EV_FORCEMESSAGE (24)

```text
text = mapStringsIDs[arg1]
text[0] != '\0' -> Hud.statBarMessage = text
text[0] == '\0' -> Hud.statBarMessage = NULL
```

The native intent always carries `CLEAR_IF_EMPTY`. Actual first-byte inspection belongs to the bounded text reader/consumer; BSP length zero is structural evidence, not assumed to be the only representation of an empty C string.

### EV_NOTE (40)

```text
Player.NotebookString += text + "||"
```

Native code does not apply these legacy mutations in this milestone.

## Permanent allocation-free string span contract

API:

```text
EspMapStrings_getRef(stringId, &ref)
```

`EspMapStringRef` contains only:

```text
index
sourceOffset
length
```

The existing 188-byte packed string-offset table remains resident. String payloads stay in `/DoomRPG-ESP32.pak`; no map-wide text allocation is introduced.

Length is recovered from adjacent payload offsets; the final span ends immediately before the fixed blockmap + plane-map tail.

New persistent cost:

```text
0 B
```

## Permanent compact intent contract

API:

```text
EspMapUiIntent_supports(codeId)
EspMapUiIntent_build(eventDescriptor, commandOffset, &intent)
```

Intent kinds:

```text
DIALOG
FORCE_MESSAGE
APPEND_NOTE
```

Intent metadata preserves:

```text
real opcode + arg1/arg2
source event index
source/global command indexes
resume command offset
string ID + source span
dialog Back/no-Back
pause-script / skip-advance-turn semantics
FORCE_MESSAGE empty-clears semantic
NOTE append + "||" semantic
```

No queue/mailbox is allocated. The intent is a caller-owned value object.

## Real-CYD hardware result

The normal optimized `esp32-cyd` firmware passed the complete probe on the classic no-PSRAM CYD.

### String-span proof

All 94 MAP_INTRO strings were reconstructed allocation-free from the resident offset table:

```text
strings         = 94
payload bytes   = 7779
zero-length     = 1
max length      = 313
first payload   = 11554
last payload    = 19512 + 7
spanFNV         = 713188eb
persistentBytes = 0
```

The final span ends exactly at the string-data boundary immediately before the fixed blockmap/plane tail.

### Real UI-intent corpus

All real `8/24/26/40` commands in the 265-bytecode stream translated successfully:

```text
refs                = 94
EV_DIALOG           = 76
EV_FORCEMESSAGE     = 3
EV_DIALOGNOBACK     = 8
EV_NOTE             = 7
pause intents       = 84
force-empty semantic= 3
zero-length force   = 2
state-exec refused  = 94
intentFNV           = 7fdd6a79
probe elapsed       = 1 ms
persistentBytes     = 0
```

Accounting is exact:

```text
76 + 3 + 8 + 7 = 94
76 + 8         = 84 pause/resume dialogs
```

Every UI-family command remained `UNSUPPORTED` in the old state-mutating opcode executor, proving the two effect families stay separated.

### Canonical real samples

```text
EV_DIALOG
  command = 11
  event   = 6
  offset  = 0
  resume  = 1
  flags   = 07
  string  = 25 @ 13558 + 23

EV_FORCEMESSAGE
  command = 4
  event   = 2
  offset  = 0
  resume  = 1
  flags   = 08
  string  = 1 @ 11569 + 14

EV_DIALOGNOBACK
  command = 19
  event   = 6
  offset  = 8
  resume  = 9
  flags   = 06
  string  = 30 @ 13679 + 14

EV_NOTE
  command = 103
  event   = 40
  offset  = 8
  resume  = 9
  flags   = 10
  string  = 85 @ 18964 + 54
```

## Hardware integrity proof

Across the entire string-span and intent sweep:

```text
heap8        = 68820 -> 68820
largest8     = 36852 -> 36852
frameFNV     = b8b39f0f -> b8b39f0f
arenaFNV     = c3882516 -> c3882516
mapStateFNV  = cd99b98e -> cd99b98e
scriptFNV    = f9e3d9df -> f9e3d9df
notebookFNV  = 4d7705c5 -> 4d7705c5
persistent   = 0 B
legacy UI    = unchanged
world        = unchanged
entities     = 0
monsters     = 0
ST_PLAYING   = no
```

Later `[ALIVE]` lines remained stable at:

```text
heap8    = 68820
largest8 = 36852
```

The absolute `heap8` is 8 B lower than the previous build's `68828`; this is a build-to-build code/layout effect. The UI-intent stage itself has exact `68820 -> 68820` zero drift.

Final PARK boundary:

```text
nativeArena           = yes
nativeTileState       = yes
nativeEventLookup     = yes
nativeEventDescriptor = yes
nativeScriptState     = yes
nativeFilter          = yes
nativeOpcodeExec      = yes
nativeUiIntent        = yes
persistentBytes       = 0
legacyUiMutation      = no
worldMutation         = no
framebufferMutation   = no
entities              = 0
monsters              = 0
noGameplay            = yes
```

## Still forbidden

```text
actual DoomCanvas dialog rendering
actual Hud status-message mutation
actual Player notebook mutation
full native Game_runEvent loop
world/door/line/sprite mutation
map transitions
entity/monster activation
ST_PLAYING
```

## Merge recommendation

**MERGE `agent/esp32-map1-native-ui-intent`.**

The hardware-tested firmware content is `045b219dd7d6d06630eb446424e8d3d3fa3d249e`. Post-test commits must remain documentation-only unless another flash is performed.

## Next boundary after merge

The most coherent next milestone is a **bounded native string reader over `/DoomRPG-ESP32.pak`**:

```text
EspMapStringRef
 -> bounded read of exactly one payload
 -> max MAP_INTRO payload = 313 B
 -> canonical text/content fingerprint
 -> no map-wide mapStringsIDs[]
 -> no runtime ZIP access
```

Use it first as a validation/consumer boundary. Do not wire legacy `DoomCanvas`, `Hud` or `Player` mutation back into the permanent intent layer.
