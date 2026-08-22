# ESP32 MAP_INTRO native PASSWORD owner milestone

Branch: `agent/esp32-map1-native-password-owner`

Base merged `main`:

```text
PR   = #55 — native CHECK_KEY dynamic gate
main = 03c4275f2abfd6671c8bf499c075435d7b61ab97
```

Firmware candidate content:

```text
e2d12085712324444f26528b77ea5122c871d85b
```

Status: **IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING**.

## Objective

Cross the last currently bounded MAP_INTRO UI/input pause family without opening world mutation:

```text
real EV_PASSWORD bytecode
 -> canonical event/command provenance
 -> expected-code map-string ref + prompt map-string ref
 -> static pause/continuation owner
 -> bounded submitted-input evaluator
 -> CORRECT / INCORRECT / EMPTY outcome metadata
```

This milestone supports only opcode `10 / EV_PASSWORD`. It does not present a password UI, mutate legacy `DoomCanvas.passCode`, assign `Game.passCode`, call `Game_runEvent()`, update Hud, play sounds, mutate world/entities/rendering or enter gameplay.

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

So the native submission API accepts at most 7 payload bytes plus the synthesized C-string terminator.

### Validation / continuation

When input reaches the expected code length, legacy sets:

```text
passwordTime = time + 300
```

A SELECT-style submission before the expected length uses the current time instead, i.e. no deliberate 300 ms feedback delay.

After the delay, PASSWORD closes the dialog and compares input with the expected code:

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

The static continuation saved by `Game_runEvent()` is the current password command; successful validation resumes from `sourceCommandOffset + 1`. Active event flags remain future invocation-context state and are not embedded in this static owner, matching the earlier DIALOG owner boundary.

## Permanent native API

New files:

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

Expected classic ESP32 ABI footprint:

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

Expected classic ESP32 ABI footprint:

```text
submit result = 12 B
```

`EspMapPassword_evaluateSubmit()` accepts caller-owned submitted bytes plus one bounded reader scratch buffer. It reads only the expected-code string from `/DoomRPG-ESP32.pak` and returns side-effect-free metadata:

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
  feedbackDelayMs=0 for early shorter submit

EMPTY incorrect:
  closeDialog=1
  resumeEvent=0
  forced message=none
  feedbackDelayMs=0
```

If a future map had an empty expected code, an empty submission is correctly classified as CORRECT with the matched-length 300 ms delay.

The permanent source depends only on native pack/runtime/event/string APIs. It has no `DoomCanvas`, `Game`, Hud, Player, Sound, entity or render dependency.

## Why PASSWORD before world mutation

After CHECK_KEY, MAP_INTRO still contains:

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
```

PASSWORD is still entirely representable as compact immutable refs + explicit pause/continuation metadata + bounded input evaluation. The remaining SHOW/HIDE/GIVEMAP/UNLOCK/OPENLINE/CLOSELINE group requires the first native world/render overlay ownership, while CHANGEMAP and SAVEGAME are larger boundaries.

## Temporary hardware probe

New files:

```text
ESP32/include/native_map1_password_probe.h
ESP32/src/native_map1_password_probe.c
```

The probe runs only after the hardware-proven CHECK_KEY stage.

It scans the canonical `93 event / 265 bytecode` MAP_INTRO corpus and discovers every real PASSWORD command rather than predeclaring a count. For each real ref it requires:

```text
state-only executor -> UNSUPPORTED
20 B owner
exact code ref
exact prompt ref
resume offset = source offset + 1
code C-string length <= 7
bounded code read
bounded prompt read
correct submission outcome
same-length wrong submission outcome
empty submission semantics
300 ms matched-length feedback delay
0 ms early-submit delay
```

The first real-CYD PASS will establish rather than predeclare:

```text
PASSWORD ref count
code/prompt source byte totals
max expected code length
first canonical command + refs
sample code content FNV
sample prompt content FNV
passwordOwnerFNV
passwordSubmitFNV
new-build heap/framebuffer values
passwordCanvas witness FNV
```

No secret/password text is printed by the probe; only IDs, offsets, lengths and fingerprints are emitted.

## Fail-closed proof

Owner-side failures must preserve the previous 20-byte owner atomically:

```text
non-PASSWORD source -> UNSUPPORTED
bad command offset  -> INVALID
noncanonical descriptor -> INVALID
NULL descriptor -> INVALID
NULL owner -> INVALID
```

Submission-side failures must return a zero result:

```text
mutated/noncanonical owner -> INVALID
submitted length >= 8      -> INVALID
scratch without terminator room -> BUFFER_TOO_SMALL
NULL submit owner          -> INVALID
NULL submit result         -> INVALID
valid owner with PAK closed -> IO_ERROR
```

A final reset must clear the complete owner deterministically.

## Hardware integrity boundary

Before/after the complete stage the probe requires exact stability of:

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
DoomCanvas passwordTime/passInput/passCode[8]/strPassCode[8] witness
Game.passCode pointer
Game.skipAdvanceTurn
Game.saveTileEvent
Game.tileEvent
Game.tileEventIndex
Game.tileEventFlags
```

The native pack may open only transiently for bounded expected-code/prompt reads and must be closed/recovered before PARK.

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

## Expected Serial family

```text
[MAPPASSWORDPROBE] ARMED ...

=== Doom RPG ESP32-native MAP_INTRO PASSWORD owner ===
[MAPPASSWORDPROBE] CONTRACT ...
[MAPPASSWORD] READY refs=... ownerBytes=20 submitResultBytes=12 stateExecRefused=... codeBytes=... promptBytes=... maxCodeLen=... resumeExact=... passwordOwnerFNV=... passwordSubmitFNV=... elapsed=...ms
[MAPPASSWORD] SAMPLE cmd=... event=... off=... resume=... arg1=... arg2=... code=...@...+... codeFNV=... prompt=...@...+... promptFNV=... codeLen=...
[MAPPASSWORD] OUTCOMES correct=... incorrect=... emptySemantics=... correctResume=... incorrectNoResume=... delayMatch=300ms earlySubmit=0ms correctMessage="Correct code!" invalidMessage="Invalid code!" guards=.../...
[MAPPASSWORD] FAILCLOSED unsupported=1 badOffset=1 badDescriptor=1 nullDescriptor=1 nullOwner=1 badOwner=1 tooLong=1 shortBuffer=1 nullSubmitOwner=1 nullSubmitResult=1 closedPack=1 ownerAtomic=yes reset=1
[MAPPASSWORD] IO entry=/intro.bsp size=21823 crc32=623f34e4 ... packIO=yes persistentHeapBytes=0
[MAPPASSWORDPROBE] RAM ... passwordCanvasFNV=...->... gamePassCodeStable=yes
[MAPPASSWORDPROBE] PARK ... nativePasswordOwner=yes ownerBytes=20 submitResultBytes=12 persistentBytes=0 ...
[ALIVE] ...
```

Use the normal optimized PlatformIO environment:

```text
esp32-cyd
```

No CI status is currently published for firmware candidate `e2d12085712324444f26528b77ea5122c871d85b`. Do not claim a local build or hardware PASS until the real classic CYD supplies it.

## Next boundary after hardware PASS + merge

Reread the then-current `main`, recovery docs, this milestone and exact remaining corpus. At that point the bounded non-world UI/control families will be exhausted, so the likely next architectural step is a first explicit native world/render overlay — but it must still be selected from the actual merged repository and legacy semantics.
