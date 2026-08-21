# ESP32 MAP_INTRO native UI/string intent milestone

Branch: `agent/esp32-map1-native-ui-intent`

Base merged `main`:

```text
PR   = #49 — first fail-closed native opcode execution
main = 6e43ef059db52783b7264e84579216cb2572a1e2
```

Status: **IMPLEMENTED; AWAITING REAL-CYD HARDWARE PASS**.

## Objective

Expand native script ownership with one coherent non-world-mutating family:

```text
8  EV_DIALOG
24 EV_FORCEMESSAGE
26 EV_DIALOGNOBACK
40 EV_NOTE
```

These commands are not allowed to call legacy `DoomCanvas`, `Hud` or `Player` yet. Instead they translate to compact native UI/player intents that preserve the recovered semantics and string linkage.

The existing state-mutating opcode executor remains unchanged and continues to support only `11/19/20`. UI opcodes must still return `UNSUPPORTED` there.

## Recovered legacy semantics

### EV_DIALOG (8)

```text
text = mapStringsIDs[arg1]
start dialog with Back softkey
save tile-event continuation
skip advance turn
```

### EV_DIALOGNOBACK (26)

Same as `EV_DIALOG`, except no Back softkey.

### EV_FORCEMESSAGE (24)

```text
text = mapStringsIDs[arg1]
non-empty -> Hud.statBarMessage = text
empty     -> Hud.statBarMessage = NULL
```

### EV_NOTE (40)

```text
Player.NotebookString += text + "||"
```

Native code does not apply these legacy mutations in this milestone.

## Allocation-free string span contract

New permanent API:

```text
EspMapStrings_getRef(stringId, &ref)
```

`EspMapStringRef` contains:

```text
index
sourceOffset
length
```

Only the existing 188-byte packed string-offset table remains resident. String payloads stay in `/DoomRPG-ESP32.pak` and are not materialized map-wide.

The source format is length-prefixed. For all but the final string, native length is recovered from adjacent payload offsets; the final span ends immediately before the fixed blockmap + plane-map source tail. The resident runtime already rejects trailing bytes and unsupported section topology.

New persistent cost:

```text
0 B
```

## Compact native intent contract

New permanent API:

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
empty FORCE_MESSAGE clearing
NOTE append + "||" semantics
```

No queue/mailbox is allocated yet. The intent is a caller-owned value object. A future native UI/player owner can consume it without forcing this layer to depend on legacy UI structures.

## Hardware probe

The temporary probe runs only after the hardware-proven first opcode-execution stage.

It validates all 94 MAP_INTRO string refs:

```text
count                 = 94
payload bytes         = 7779
max string length     = 313
adjacent span topology= exact
last string end       = start of blockmap
```

Hardware establishes:

```text
empty string count
first/last source span
spanFNV
```

It then walks all 93 event descriptors and all 265 commands. Every real `8/24/26/40` command must translate successfully and be checked against an independent expected kind/flag mapping.

Hardware establishes:

```text
total UI refs
8/24/26/40 counts
pause-intent count
empty FORCE_MESSAGE clear count
intentFNV
first real sample of each opcode family
```

Every UI command is also sent to the old state-mutating `EspMapOpcodeExecutor_execute()` and must still return `UNSUPPORTED`.

## Integrity gate

Across the complete string + intent sweep:

```text
heap8        X -> X
largest8     Y -> Y
frameFNV     F -> F
arenaFNV     c3882516 -> c3882516
mapStateFNV  cd99b98e -> cd99b98e
scriptFNV    f9e3d9df -> f9e3d9df
NotebookString fingerprint unchanged
Hud.statBarMessage pointer unchanged
Game tile-event continuation fields unchanged
DoomCanvas remains ST_INTRO page 3
entities     = 0
monsters     = 0
ST_PLAYING   = no
```

## Expected log tail

```text
[MAPUIPROBE] ARMED first native opcode execution proven; allocation-free UI/string intent translation starts on next loop service

=== Doom RPG ESP32-native MAP_INTRO UI/string intents ===
[MAPUIPROBE] CONTRACT resolve compact string spans + translate EV_DIALOG/FORCEMESSAGE/DIALOGNOBACK/NOTE to native intents; 0 persistent bytes; no DoomCanvas/Hud/Player/world mutation
[MAPSTRING] READY strings=94 payload=7779 empty=? max=313 firstOffset=? last=?+? spanFNV=???????? persistentBytes=0
[MAPUI] READY refs=? dialog=? force=? noBack=? note=? pause=? clear=? stateExecRefused=? intentFNV=???????? elapsed=?ms persistentBytes=0
[MAPUIPROBE] SAMPLE dialog ...
[MAPUIPROBE] SAMPLE force ...
[MAPUIPROBE] SAMPLE noBack ...
[MAPUIPROBE] SAMPLE note ...
[MAPUIPROBE] RAM heap8=X->X ... arenaFNV=c3882516->c3882516 mapStateFNV=cd99b98e->cd99b98e scriptFNV=f9e3d9df->f9e3d9df notebookFNV=N->N
[MAPUIPROBE] PARK ... nativeUiIntent=yes persistentBytes=0 legacyUiMutation=no worldMutation=no framebufferMutation=no entities=0 monsters=0 noGameplay=yes
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

## Next boundary after PASS

Use the measured UI-intent corpus to choose the next owner. Likely choices are either:

1. a bounded native string reader + first real dialog/message presentation path; or
2. another non-entity opcode family if that keeps ownership boundaries cleaner.

Do not couple the permanent intent layer back to legacy `Render.mapStringsIDs` or runtime ZIP access.
