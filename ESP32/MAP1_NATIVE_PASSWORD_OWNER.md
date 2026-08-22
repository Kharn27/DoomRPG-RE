# ESP32 MAP_INTRO native PASSWORD owner milestone

Branch: `agent/esp32-map1-native-password-owner`

Base merged `main`:

```text
PR   = #55 — native CHECK_KEY dynamic gate
main = 03c4275f2abfd6671c8bf499c075435d7b61ab97
```

Hardware-tested firmware content:

```text
e2d12085712324444f26528b77ea5122c871d85b
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

Cross the last bounded MAP_INTRO UI/input pause family before opening native world mutation:

```text
real EV_PASSWORD bytecode
 -> canonical event/command provenance
 -> expected-code map-string ref + prompt map-string ref
 -> static pause/continuation owner
 -> bounded submitted-input evaluator
 -> CORRECT / INCORRECT / EMPTY outcome metadata
```

This milestone supports only opcode `10 / EV_PASSWORD`. It does not present password UI, mutate legacy `DoomCanvas.passCode`, assign `Game.passCode`, call `Game_runEvent()`, update Hud, play sounds, mutate world/entities/rendering or enter gameplay.

## Recovered legacy behavior

`Game_executeEvent()` handles PASSWORD as:

```text
Game.passCode = mapStringsIDs[arg1 & 255]
Game.tileEvent = event
DoomCanvas_startDialogPassword(mapStringsIDs[(arg1 >> 8) & 255])
Game.saveTileEvent = true
Game.skipAdvanceTurn = true
```

Therefore `arg1` packs two string IDs:

```text
low  8 bits = expected password string
high 8 bits = password prompt string
```

`DoomCanvas_startDialogPassword()` enters `ST_DIALOGPASSWORD`, prepares the prompt, clears the 8-byte input buffer and initializes controller input to `'0'`.

Recovered input storage:

```text
char passCode[8]
char strPassCode[8]
```

The native submission API therefore accepts at most 7 payload bytes plus the synthesized C-string terminator.

### Validation / continuation

When input reaches the expected code length, legacy sets:

```text
passwordTime = time + 300
```

A SELECT-style submission before the expected length uses the current time instead, so it has no deliberate 300 ms feedback delay.

After that timing gate, PASSWORD closes the dialog and compares input with the expected code:

```text
correct:
  Hud_addMessageForce("Correct code!", true)
  Game_runEvent(tileEvent, tileEventIndex + 1, tileEventFlags)

non-empty incorrect:
  Hud_addMessageForce("Invalid code!", true)
  no resume

empty incorrect:
  no forced status message
  no resume
```

The static continuation saved by the event loop is the current password command. Successful validation resumes from `sourceCommandOffset + 1`. Dynamic invocation flags remain future native event-loop context and are not embedded in this static owner.

## Permanent native API

Files:

```text
ESP32/include/esp_map_password.h
ESP32/src/esp_map_password.c
```

### Static owner

```c
typedef struct EspMapPasswordOwnerState_s {
    EspMapStringRef expectedCode;
    EspMapStringRef prompt;
    uint16_t sourceEventIndex;
    uint16_t globalCommandIndex;
    uint8_t sourceCommandOffset;
    uint8_t resumeCommandOffset;
    uint8_t flags;
    uint8_t active;
} EspMapPasswordOwnerState;
```

Real classic-CYD ABI footprint:

```text
owner value       = 20 B
text copied       = 0 B
persistent heap   = 0 B in probe
```

Exact owner flags:

```text
PAUSE_SCRIPT
SKIP_ADVANCE_TURN
SAVE_CONTINUATION
RESUME_ON_SUCCESS
```

`EspMapPasswordOwner_apply()` revalidates the canonical event descriptor and linked source command, resolves both packed string IDs through the native immutable string table, and commits atomically only after all provenance/ref checks succeed.

### Submission result

```c
typedef struct EspMapPasswordSubmitResult_s {
    uint16_t sourceEventIndex;
    uint16_t globalCommandIndex;
    uint16_t feedbackDelayMs;
    uint8_t sourceCommandOffset;
    uint8_t resumeCommandOffset;
    uint8_t kind;
    uint8_t closeDialog;
    uint8_t resumeEvent;
    uint8_t forceStatusMessage;
} EspMapPasswordSubmitResult;
```

Real classic-CYD ABI footprint:

```text
submit result = 12 B
```

`EspMapPassword_evaluateSubmit()` reads only the expected-code string from `/DoomRPG-ESP32.pak` into caller-owned scratch and returns side-effect-free metadata:

```text
CORRECT:
  closeDialog=1
  resumeEvent=1
  forced message="Correct code!"
  feedbackDelayMs=300

INCORRECT non-empty:
  closeDialog=1
  resumeEvent=0
  forced message="Invalid code!"
  feedbackDelayMs=300 when submitted length == expected length
  feedbackDelayMs=0 for an early shorter submit

EMPTY incorrect:
  closeDialog=1
  resumeEvent=0
  forced message=none
  feedbackDelayMs=0
```

If a future map had an empty expected code, an empty submission is classified as CORRECT with the matched-length 300 ms delay.

The permanent source depends only on native pack/runtime/event/string APIs. It has no `DoomCanvas`, `Game`, Hud, Player, Sound, entity or render dependency.

## Real-CYD corpus proof

Normal optimized environment:

```text
esp32-cyd
```

The real classic no-PSRAM CYD established the exact MAP_INTRO PASSWORD corpus:

```text
refs              = 2
ownerBytes        = 20
submitResultBytes = 12
stateExecRefused  = 2
codeBytes         = 8
promptBytes       = 72
maxCodeLen        = 4
resumeExact       = 2
passwordOwnerFNV  = 48f01689
passwordSubmitFNV = 90e8c574
elapsed           = 49 ms
```

Canonical first real PASSWORD command:

```text
global command = 17
event          = 6
command offset = 6
resume offset  = 7
arg1           = 00001d1c
arg2           = 00040100
expected code  = string 28 @ 13630 + 4
codeFNV        = 92444853
prompt         = string 29 @ 13636 + 41
promptFNV      = ddbe080a
codeLen        = 4
```

The probe intentionally does not print password text; only IDs, offsets, lengths and fingerprints are emitted.

## Submission / continuation proof

Every real PASSWORD command was exercised through the side-effect-free evaluator:

```text
correct           = 2
incorrect         = 2
emptySemantics    = 2
correctResume     = 2
incorrectNoResume = 2
delayMatch        = 300 ms
earlySubmit       = 0 ms
guards            = 10 / 10
correct message   = "Correct code!"
invalid message   = "Invalid code!"
```

This proves both static pause ownership and the asymmetric continuation rule: only a correct submitted code resumes the event at `sourceCommandOffset + 1`.

The state-only opcode executor still refuses both PASSWORD refs, preserving the split between the small state executor and this dedicated pause/input owner.

## Fail-closed proof

Hardware proved all expected refusals:

```text
unsupported       = 1
badOffset         = 1
badDescriptor     = 1
nullDescriptor    = 1
nullOwner         = 1
badOwner          = 1
tooLong           = 1
shortBuffer       = 1
nullSubmitOwner   = 1
nullSubmitResult  = 1
closedPack        = 1
ownerAtomic       = yes
reset             = 1
```

Owner-side failures preserve the previous 20-byte owner atomically. Submission-side failures leave the writable result zeroed. The closed-pack case returns IO failure without mutating owner or legacy state.

## Native-pack / RAM proof

PASSWORD transient pack access:

```text
entry              = /intro.bsp
size               = 21823
crc32              = 623f34e4
heapOpen           = 64384
transientHeapCost  = 4364 B
largestOpen        = 36852
packIO             = yes
persistentHeapBytes= 0
```

Before and after the complete PASSWORD stage:

```text
heap8             = 68748 -> 68748
largest8          = 36852 -> 36852
frameFNV          = 7a95b5b5 -> 7a95b5b5
arenaFNV          = c3882516 -> c3882516
mapStateFNV       = cd99b98e -> cd99b98e
scriptFNV         = f9e3d9df -> f9e3d9df
legacyNotebookFNV = 4d7705c5 -> 4d7705c5
legacyKeys        = 00000000 -> 00000000
hudFNV            = 505b1255 -> 505b1255
passwordCanvasFNV = 214171cf -> 214171cf
gamePassCodeStable= yes
```

The immediately preceding CHECK_KEY stage on the same firmware also remained stable at `heap8=68748`, `largest8=36852` and `frameFNV=7a95b5b5`, with canonical `keyGateFNV=9ace79cd`.

Absolute heap/frame values can vary slightly across firmware builds. Acceptance is based on exact before/after stability inside a build plus unchanged canonical structural fingerprints.

## Final PARK boundary

Hardware PARK proved:

```text
nativeArena                  = yes
nativeTileState              = yes
nativeEventLookup            = yes
nativeEventDescriptor        = yes
nativeScriptState            = yes
nativeFilter                 = yes
nativeOpcodeExec             = yes
nativeUiIntent               = yes
nativeStringReader           = yes
nativeStatusMessageOwner     = yes
nativeDialogOwner            = yes
nativeNotebookOwner          = yes
nativeKeyGate                = yes
nativePasswordOwner          = yes
ownerBytes                   = 20
submitResultBytes            = 12
persistentBytes              = 0
legacyPasswordMutation       = no
legacyHudMutation            = no
legacyGameContinuationMutation = no
worldMutation                = no
framebufferMutation          = no
entities                     = 0
monsters                     = 0
noGameplay                   = yes
```

Complete post-PARK heartbeat:

```text
uptime=170149 ms
heap=134512
heap8=68748
largest8=36852
SD/ZIP/VIDEO/CORE/LAYOUT/PRERENDER/RENDER/MAPPINGS/MENUBSP = ready
```

A later heartbeat began at `175150 ms` with the same heap values but its line was truncated after `VIDEO=rea`; the preceding complete heartbeat is sufficient steady-state evidence.

Permanent prohibitions remain:

```text
shapeData                          = NULL
mediaTexels                        = NULL
actual password UI                = no
legacy DoomCanvas password mutation = no
legacy Game.passCode mutation     = no
legacy Hud mutation               = no
legacy Game continuation mutation = no
actual sound playback             = no
full native Game_runEvent loop    = no
world/entity/render mutation      = no
map transition                    = no
savegame mutation                 = no
entities                           = 0
monsters                           = 0
ST_PLAYING                         = no
```

## Hardware acceptance status

The complete real PASSWORD corpus, both string refs per command, exact static resumes, correct/incorrect/empty submit semantics, 300 ms/0 ms timing behavior, fail-closed paths, PAK recovery, legacy integrity witnesses and stable post-PARK heartbeat are a **REAL-CYD HARDWARE PASS**.

This branch is **MERGE-READY**. The firmware-bearing content actually tested is:

```text
e2d12085712324444f26528b77ea5122c871d85b
```

Every later commit must remain documentation-only unless another firmware is flashed.

## Remaining MAP_INTRO opcode families

After PASSWORD, the bounded non-world UI/control families are exhausted. Still unowned:

```text
2  EV_CHANGEMAP
7  EV_SHOW
9  EV_GIVEMAP
13 EV_UNLOCK
15 EV_OPENLINE
16 EV_CLOSELINE
18 EV_HIDE
27 EV_SAVEGAME
```

The likely next architectural boundary is the first explicit native world/render overlay selected from SHOW/HIDE/GIVEMAP/UNLOCK/OPENLINE/CLOSELINE. `EV_CHANGEMAP` and `EV_SAVEGAME` remain larger later boundaries.

Do not preselect the exact family before merge recovery: reread the new `main`, recovery docs, this merged milestone and exact legacy behavior first.
