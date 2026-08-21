# ESP32 MAP_INTRO native DIALOG pause-owner milestone

Branch: `agent/esp32-map1-native-dialog-owner`

Base merged `main`:

```text
PR   = #52 — native FORCE_MESSAGE status owner
main = 40b61af5e2115266d4d03dddcc3175850538b0f5
```

Firmware candidate content:

```text
85aa89c4218a819e7f18cbf77f64dfbef3c5bac9
```

Status: **IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING**.

## Objective

Cross exactly one additional native UI-effect ownership boundary:

```text
real EV_DIALOG / EV_DIALOGNOBACK bytecode
 -> EspMapUiIntent
 -> canonical provenance validation
 -> compact caller-owned pause/presentation owner
```

This milestone deliberately stops before actual dialog presentation and before a full native event execution loop.

## Recovered legacy behavior

`Game_executeEvent()` handles both dialog opcodes through the same path:

```text
DoomCanvas_startDialog(mapStringsIDs[arg1], codeId == EV_DIALOG)
game->saveTileEvent = true
game->tileEvent = event
game->skipAdvanceTurn = true
```

`Game_runEvent()` then records the current command offset when `saveTileEvent` is observed. When the dialog closes, `DoomCanvas` resumes with:

```text
Game_runEvent(game, game->tileEvent, game->tileEventIndex + 1,
              game->tileEventFlags)
```

Therefore the static semantics owned by this milestone are:

```text
text ref
source event
source command offset
resume command offset = source + 1
pause script
skip advance turn
Back soft-key only for EV_DIALOG
no Back soft-key for EV_DIALOGNOBACK
```

The dynamic activation flags represented by legacy `tileEventFlags` belong to the future native event-loop invocation context. They are intentionally not invented or persisted by this static effect owner.

## Permanent native API

New files:

```text
ESP32/include/esp_map_dialog_owner.h
ESP32/src/esp_map_dialog_owner.c
```

State:

```c
typedef struct EspMapDialogOwnerState_s {
    EspMapStringRef text;
    uint16_t sourceEventIndex;
    uint8_t sourceCommandOffset;
    uint8_t resumeCommandOffset;
    uint8_t flags;
    uint8_t active;
} EspMapDialogOwnerState;
```

Expected classic ESP32 ABI footprint:

```text
owner value            = 12 B
persistent copied text = 0 B
persistent heap bytes  = 0 B in this probe
```

The API is allocation-free:

```text
EspMapDialogOwner_reset()
EspMapDialogOwner_isActive()
EspMapDialogOwner_getRef()
EspMapDialogOwner_apply()
```

`EspMapDialogOwner_apply()` supports only opcode 8 and 26 intents and validates:

```text
intent status/kind
exact flags
arg1 == text.index
resume == source + 1
canonical string ref
source event exists
source command exists
global command index matches descriptor
bytecode id/arg1/arg2 match the source command
```

Every failed apply leaves the existing owner unchanged.

## Storage boundary

Unlike `EV_FORCEMESSAGE`, dialog ownership has no recovered semantic decision based on the first text byte. The permanent owner therefore does **not** read the PAK when capturing the pause intent.

It retains only the immutable `EspMapStringRef`. A future native dialog presenter will resolve that ref through the already hardware-proven `EspMapStrings_read()` boundary when text is actually needed for presentation.

This keeps storage I/O out of script-pause ownership and avoids an unnecessary read on every dialog opcode.

## Inherited real MAP_INTRO corpus

Already hardware-proven UI intent counts:

```text
EV_DIALOG       = 76
EV_DIALOGNOBACK = 8
total dialogs   = 84
pause intents   = 84
```

Canonical Back sample:

```text
global command = 11
event          = 6
command offset = 0
resume offset  = 1
flags          = 07
string         = 25 @ 13558 + 23
```

Canonical no-Back sample:

```text
global command = 19
event          = 6
command offset = 8
resume offset  = 9
flags          = 06
string         = 30 @ 13679 + 14
```

The existing state-only opcode executor must continue to refuse all 84 dialog commands.

## Temporary hardware probe

New files:

```text
ESP32/include/native_map1_dialog_owner_probe.h
ESP32/src/native_map1_dialog_owner_probe.c
```

The probe runs only after the hardware-proven FORCE_MESSAGE owner probe.

It walks all 93 event descriptors / 265 bytecodes and applies only the 84 real dialog commands.

Acceptance requires:

```text
refs              = 84
Back               = 76
noBack             = 8
pause              = 84
skipTurn           = 84
resumeExact        = 84
ownerBytes         = 12
textCopyBytes      = 0
stateExecRefused   = 84
packIO             = no
persistentHeapBytes= 0
```

The probe computes a new `dialogApplyFNV` over real command order, refs, continuation metadata and before/after owner states. The first real-CYD PASS will make that value canonical.

## Fail-closed proof

Starting from a real active dialog owner, the probe requires all of these to be refused atomically:

```text
FORCEMESSAGE intent             -> UNSUPPORTED
wrong dialog flags              -> INVALID
wrong intent kind               -> INVALID
mutated text ref                -> INVALID
out-of-range source event       -> INVALID
mutated global command index    -> INVALID
mutated resume offset           -> INVALID
NULL intent                     -> INVALID
```

A final reset must clear the owner exactly.

## Hardware integrity boundary

Before and after the complete probe:

```text
heap8
largest8
framebuffer FNV
arenaFNV      = c3882516
mapStateFNV   = cd99b98e
scriptFNV     = f9e3d9df
notebook FNV
Hud.statBarMessage pointer
Game.skipAdvanceTurn
Game.saveTileEvent
Game.tileEvent
Game.tileEventIndex
Game.tileEventFlags
```

must remain unchanged.

Permanent prohibitions remain:

```text
actual DoomCanvas dialog mutation = no
legacy Game continuation mutation = no
world/entity/render mutation      = no
shapeData                          = NULL
mediaTexels                        = NULL
entities                           = 0
monsters                           = 0
ST_PLAYING                         = no
```

## Expected Serial family

```text
[MAPDIALOGPROBE] ARMED ...

=== Doom RPG ESP32-native MAP_INTRO DIALOG pause owner ===
[MAPDIALOG] READY refs=84 back=76 noBack=8 pause=84 skipTurn=84 resumeExact=84 ownerBytes=12 textCopyBytes=0 stateExecRefused=84 dialogApplyFNV=...
[MAPDIALOG] SAMPLE back ... noBack ...
[MAPDIALOG] FAILCLOSED unsupported=1 badFlags=1 badKind=1 badRef=1 badEvent=1 badGlobal=1 badResume=1 nullIntent=1 ownerAtomic=yes reset=1
[MAPDIALOGPROBE] RAM ... packIO=no persistentHeapBytes=0
[MAPDIALOGPROBE] PARK ... nativeDialogOwner=yes ownerValueBytes=12 ...
[ALIVE] ...
```

Use the normal optimized PlatformIO environment:

```text
esp32-cyd
```

Until the real classic CYD supplies the PASS log and a stable post-PARK heartbeat, this branch is **not merge-ready**.

## Next boundary after a hardware PASS

After this owner is hardware-proven, recover the next smallest native effect family from the actual MAP_INTRO corpus. `EV_NOTE` is a likely candidate, but the repository and legacy behavior must be reread after merge before choosing.
