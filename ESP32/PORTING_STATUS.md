# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for stable build guidance, [`DOCUMENTATION.md`](DOCUMENTATION.md) for the documentation index, and milestone archives for detailed hardware evidence.

## Latest merged hardware baseline

```text
PR   = #55 — native CHECK_KEY dynamic gate
main = 03c4275f2abfd6671c8bf499c075435d7b61ab97
hardware-tested firmware content = 3b4844e8fa5d38d522e1adc70ffac646978f130d
```

Detailed merged evidence: [`MAP1_NATIVE_KEY_GATE.md`](MAP1_NATIVE_KEY_GATE.md).

## Current candidate

```text
branch = agent/esp32-map1-native-password-owner
base   = 03c4275f2abfd6671c8bf499c075435d7b61ab97
firmware candidate content = e2d12085712324444f26528b77ea5122c871d85b
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

Detailed active milestone: [`MAP1_NATIVE_PASSWORD_OWNER.md`](MAP1_NATIVE_PASSWORD_OWNER.md).

The candidate supports only `10 / EV_PASSWORD` as a compact two-string-ref pause/continuation owner plus a bounded native submission evaluator. It does not present password UI, mutate legacy DoomCanvas/Game/Hud state, run Game continuation, mutate world/entities/rendering or enter gameplay.

## Permanent target / ownership

```text
board        = ESP32-2432S028R classic CYD
MCU          = ESP32-D0WD-V3 dual-core 240 MHz
flash        = 4 MB
PSRAM        = none
display      = ILI9341 320x240
touch        = XPT2046
storage      = microSD
framebuffer  = 160x120 RGB565 = 38400 B
presentation = nearest-neighbor 2x
audio        = deferred
```

Permanent invariants:

```text
shapeData   == NULL
mediaTexels == NULL
runtime ZIP dependency = forbidden
native backing store   = /DoomRPG-ESP32.pak
DoomRPG-RE desktop engine = executable specification/reference only
final CYD engine          = ESP32-native ownership
```

Current native direction:

```text
BSP source in native pack
 -> compact immutable EspMapRuntime
 -> allocation-free accessors
 -> mutable EspMapState
 -> tile/event lookup
 -> event descriptor + bytecode linkage
 -> mutable EspMapScriptState
 -> side-effect-free event filter
 -> fail-closed native state-opcode executor
 -> allocation-free native string spans
 -> compact native UI/player intents
 -> bounded pack-backed one-string reader
 -> explicit native effect/player owners
 -> pure native dynamic gates
 -> bounded native pause/input owners
 -> native event/script loop
 -> explicit native world/render overlays
 -> ESP32-native gameplay + renderer
```

## Hardware-proven fingerprints through PR #55

```text
source BSP FNV       = d5cc751f
arenaFNV             = c3882516
decodedFNV           = a426dd18
mapStateFNV          = cd99b98e
lookupFNV            = 63430151
descriptorFNV        = 27115328
linkageFNV           = 5727902c
scriptFNV            = f9e3d9df
filterFNV            = a5923b21
resumeFNV            = b98452da
opcodeAuditFNV       = 6f28df45
firstExecFNV         = 646b565c
stringSpanFNV        = 713188eb
uiIntentFNV          = 7fdd6a79
stringContentFNV     = e995ee51
statusApplyFNV       = 52b25a5f
dialogApplyFNV       = d0254f3d
noteApplyFNV         = 43183162
notebookContentFNV   = 599609e0
notebookStorageFNV   = 75cf54e0
keyGateFNV           = 9ace79cd
```

MAP_INTRO `/intro.bsp` / `Entrance`:

```text
source bytes = 21823
CRC32        = 623f34e4
nodes        = 223
lines        = 480
mapSprites   = 344
events       = 93
byteCodes    = 265
strings      = 94
stringData   = 7779 B
maxString    = 313 B
```

Real opcode IDs:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

## Persistent native RAM ownership

Hardware-proven persistent heap ownership remains:

```text
immutable arena       = 14112 B actual heap
mutable tile state    =  1040 B actual heap
mutable script state  =   100 B actual heap
opcode executor       =     0 B persistent heap
string spans/intents  =     0 B persistent heap
bounded string reader =     0 B persistent heap
effect/gate probes    =     0 B persistent heap
-----------------------------------------
current proven total  = 15252 B
largest8              = 36852 preserved
```

Caller-owned/value types hardware-proven through PR #55:

```text
EspMapStatusMessageState =   8 B
EspMapDialogOwnerState   =  12 B
EspMapNotebookState      = 514 B
EspMapKeyGateResult      =  12 B
```

Current PASSWORD candidate target:

```text
EspMapPasswordOwnerState  = 20 B expected
EspMapPasswordSubmitResult= 12 B expected
text copied into owner    = 0 B
persistent heap           = 0 B expected
```

## Hardware-proven native execution/effect boundary

State-only native opcode executor still supports only:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

All other opcodes remain fail-closed there until their dedicated family owns semantics.

Hardware-proven effect/player/control families:

```text
FORCEMESSAGE:
  refs=3 set=1 clear=2 ownerBytes=8 statusApplyFNV=52b25a5f

DIALOG/NOBACK:
  refs=84 Back=76 noBack=8 pause=84 skipTurn=84 resumeExact=84
  ownerBytes=12 dialogApplyFNV=d0254f3d

NOTE:
  refs=7 sourceBytes=256 finalLen=270 ownerBytes=514
  noteApplyFNV=43183162 contentFNV=599609e0 storageFNV=75cf54e0

CHECK_KEY:
  refs=1 green/yellow/blue/red=0/1/0/0
  scenarios=16 pass=8 blocked=8 resultBytes=12
  stateExecRefused=1 keyGateFNV=9ace79cd
```

Canonical CHECK_KEY sample:

```text
cmd38 event11 off0 key=1 mask=02 arg2=00000100
missingMessage="Need Yellow Key"
sound=5065
saveOffset=current
```

## Latest merged tested-build integrity

CHECK_KEY firmware:

```text
heap8             = 68756 -> 68756
largest8          = 36852 -> 36852
frameFNV          = c56f998b -> c56f998b
arenaFNV          = c3882516 -> c3882516
mapStateFNV       = cd99b98e -> cd99b98e
scriptFNV         = f9e3d9df -> f9e3d9df
legacyNotebookFNV = 4d7705c5 -> 4d7705c5
legacyKeys        = 00000000 -> 00000000
hudFNV            = 505b1255 -> 505b1255
persistentBytes   = 0
pack I/O          = none
```

Complete post-PARK heartbeat:

```text
uptime=25410 ms
heap=134520
heap8=68756
largest8=36852
all reported subsystems = ready
```

Absolute heap/frame values may vary across firmware builds; acceptance is based on exact before/after stability plus unchanged canonical structural fingerprints.

## Current PASSWORD candidate contract

Recovered legacy static setup:

```text
arg1 low byte  = expected-code map string ID
arg1 high byte = prompt map string ID
Game.passCode  = expected string
Game.tileEvent = current event
start ST_DIALOGPASSWORD(prompt)
saveTileEvent  = true
skipAdvanceTurn= true
```

Recovered password input storage:

```text
DoomCanvas.passCode[8]
DoomCanvas.strPassCode[8]
max submitted payload = 7 B
```

Recovered completion semantics:

```text
submitted length == expected length:
  feedback delay = 300 ms

shorter SELECT-style submission:
  feedback delay = 0 ms

correct:
  close password dialog
  forced "Correct code!"
  resume source event at sourceCommandOffset + 1

non-empty incorrect:
  close password dialog
  forced "Invalid code!"
  no resume

empty incorrect:
  close password dialog
  no forced message
  no resume
```

Permanent native API:

```text
EspMapPasswordOwner_reset()
EspMapPasswordOwner_isActive()
EspMapPasswordOwner_apply()
EspMapPassword_evaluateSubmit()
EspMapPassword_resultMessage()
```

The 20-byte owner retains only two `EspMapStringRef`s + static provenance/continuation metadata. The 12-byte submit result carries outcome, close/resume/message bits and `feedbackDelayMs`. Dynamic invocation flags remain future event-loop context rather than static owner state.

The expected password itself is never copied into persistent state. Submission evaluation reads only the expected-code string through the proven bounded native-pack reader.

## PASSWORD hardware probe target

The real PASSWORD corpus count is intentionally not predeclared. Full source identity remains independently protected by `opcodeAuditFNV=6f28df45`.

For every real PASSWORD command, hardware acceptance requires:

```text
stateExecRefused = refs
ownerBytes       = 20
submitResultBytes= 12
resumeExact      = refs
correct outcomes = refs
incorrect outcomes = refs
empty semantics  = refs
correctResume    = refs
incorrectNoResume= refs
matched-length delay = 300 ms
shorter submit delay = 0 ms
code C-string length <= 7
guarded bounded string reads
```

The real CYD will establish:

```text
PASSWORD refs
codeBytes total
promptBytes total
maxCodeLen
first command/event/offset/resume metadata
code ref + content FNV
prompt ref + content FNV
passwordOwnerFNV
passwordSubmitFNV
new-build heap/framebuffer absolute values
passwordCanvasFNV witness
native-pack transient open cost
```

The probe never prints password text; only refs, spans, lengths and fingerprints.

Fail-closed target:

```text
unsupported=1
badOffset=1
badDescriptor=1
nullDescriptor=1
nullOwner=1
badOwner=1
tooLong=1
shortBuffer=1
nullSubmitOwner=1
nullSubmitResult=1
closedPack=1
ownerAtomic=yes
reset=1
```

Hardware integrity must preserve exactly:

```text
heap/largest/framebuffer
arena/map/script fingerprints
legacy notebook FNV 4d7705c5
legacy Player.keys
Hud witness
DoomCanvas password buffers/timing witness
Game.passCode pointer
Game continuation fields
entities=0
monsters=0
ST_PLAYING not reached
```

## Current hardware-proven execution path

```text
validated intro disposal
 -> native BSP inventory
 -> compact resident arena
 -> allocation-free access
 -> mutable tile state
 -> tile/event lookup
 -> descriptor/linkage
 -> compact script state
 -> exhaustive event filter
 -> real state-opcode execution/rollback
 -> string spans + UI intents
 -> bounded native-pack string reader
 -> real EV_FORCEMESSAGE -> native status owner
 -> real EV_DIALOG/NOBACK -> native pause owner
 -> real EV_NOTE -> bounded native notebook owner
 -> real EV_CHECK_KEY -> pure native dynamic gate
 -> candidate: real EV_PASSWORD -> two-ref pause owner + bounded submit evaluator
```

Still forbidden:

```text
actual DoomCanvas dialog/password presentation
legacy Game continuation mutation by native code
legacy Hud mutation
legacy Player key/notebook mutation
legacy DoomCanvas password mutation
legacy Game.passCode mutation
actual password/input sound playback
full native Game_runEvent execution loop
world/door/line/sprite mutation
map transitions
savegame mutation
entity/monster activation
native gameplay rendering
ST_PLAYING
```

## Remaining MAP_INTRO families after current candidate

If PASSWORD passes, still unowned:

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

The compact non-world UI/control families will then be exhausted. SHOW/HIDE/GIVEMAP/UNLOCK/OPENLINE/CLOSELINE are candidates for the first explicit native world/render overlay; CHANGEMAP and SAVEGAME remain larger later boundaries.

## Current validation target

Build/flash normal optimized:

```text
esp32-cyd
```

from `agent/esp32-map1-native-password-owner` and capture `[MAPPASSWORD]` / `[MAPPASSWORDPROBE]` plus a later stable `[ALIVE]` heartbeat.

Firmware candidate to identify the tested content:

```text
e2d12085712324444f26528b77ea5122c871d85b
```

No CI status is published for this SHA. Do not mark merge-ready until the real classic CYD supplies the PASS.

## Next bounded milestone after PASS + merge

Reread the then-current repository, recovery docs, merged PASSWORD milestone and exact remaining MAP_INTRO legacy behavior before selecting the first native world/render overlay family.
