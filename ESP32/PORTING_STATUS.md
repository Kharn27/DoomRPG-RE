# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for stable build guidance, [`DOCUMENTATION.md`](DOCUMENTATION.md) for the documentation index, and milestone archives for detailed hardware evidence.

## Latest merged hardware baseline

```text
PR   = #54 — native NOTE notebook owner
main = 03002f79eb03bdcb4c9e430c43e4693dab47e44b
hardware-tested firmware content = f619aefc85402d28c4de6edab5ca32ea1eb514dd
```

Detailed merged evidence: [`MAP1_NATIVE_NOTEBOOK.md`](MAP1_NATIVE_NOTEBOOK.md).

## Current candidate

```text
branch = agent/esp32-map1-native-key-gate
base   = 03002f79eb03bdcb4c9e430c43e4693dab47e44b
firmware candidate content = 3b4844e8fa5d38d522e1adc70ffac646978f130d
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

Detailed active milestone: [`MAP1_NATIVE_KEY_GATE.md`](MAP1_NATIVE_KEY_GATE.md).

The candidate supports only real `41 / EV_CHECK_KEY` as a pure native script-control evaluator. It returns PASS/BLOCKED metadata from caller-supplied key bits and performs no persistent allocation, pack I/O or legacy Player/Hud/Sound/Game/world/render mutation.

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
 -> fail-closed native opcode executor
 -> allocation-free native string spans
 -> compact native UI/player intents
 -> bounded pack-backed one-string reader
 -> explicit native effect/player owners
 -> pure native dynamic gates
 -> native event/script loop
 -> ESP32-native gameplay + renderer
```

## Hardware-proven fingerprints through PR #54

```text
source BSP FNV     = d5cc751f
arenaFNV           = c3882516
decodedFNV         = a426dd18
mapStateFNV        = cd99b98e
lookupFNV          = 63430151
descriptorFNV      = 27115328
linkageFNV         = 5727902c
scriptFNV          = f9e3d9df
filterFNV          = a5923b21
resumeFNV          = b98452da
opcodeAuditFNV     = 6f28df45
firstExecFNV       = 646b565c
stringSpanFNV      = 713188eb
uiIntentFNV        = 7fdd6a79
stringContentFNV   = e995ee51
statusApplyFNV     = 52b25a5f
dialogApplyFNV     = d0254f3d
noteApplyFNV       = 43183162
notebookContentFNV = 599609e0
notebookStorageFNV = 75cf54e0
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

Real MAP_INTRO opcode IDs remain:

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
effect-owner probes   =     0 B persistent heap
-----------------------------------------
current proven total  = 15252 B
largest8              = 36852 preserved
```

Caller-owned value types already hardware-proven:

```text
EspMapStatusMessageState =   8 B
EspMapDialogOwnerState   =  12 B
EspMapNotebookState      = 514 B
```

Current candidate adds only a caller-local result type:

```text
EspMapKeyGateResult = 12 B expected
persistent owner    = none
persistent heap     = 0 B expected
pack I/O            = none
```

A future permanent gameplay/player owner embedding the notebook must explicitly account its 514 bytes. The current NOTE and gate probes keep all such values on stack.

Measured legacy structural allocation was `55341 B`; hardware-proven native structural/script heap ownership remains `40089 B` smaller (~72.4%).

## Hardware-proven state/UI/player boundary

State-only native opcode executor still supports only:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

All other opcodes remain fail-closed there.

Hardware-proven UI/string corpus:

```text
EV_DIALOG       = 76
EV_FORCEMESSAGE = 3
EV_DIALOGNOBACK = 8
EV_NOTE         = 7
total UI refs   = 94
pause intents   = 84
uiIntentFNV     = 7fdd6a79
```

Hardware-proven owners:

```text
FORCEMESSAGE:
  refs=3 set=1 clear=2 ownerBytes=8 statusApplyFNV=52b25a5f

DIALOG/NOBACK:
  refs=84 Back=76 noBack=8 pause=84 skipTurn=84 resumeExact=84
  ownerBytes=12 dialogApplyFNV=d0254f3d

NOTE:
  refs=7 sourceBytes=256 finalLen=270 ownerBytes=514
  noteApplyFNV=43183162 contentFNV=599609e0 storageFNV=75cf54e0
```

Latest NOTE firmware integrity:

```text
heap8             = 68772 -> 68772
largest8          = 36852 -> 36852
frameFNV          = a3e3cc8e -> a3e3cc8e
arenaFNV          = c3882516 -> c3882516
mapStateFNV       = cd99b98e -> cd99b98e
scriptFNV         = f9e3d9df -> f9e3d9df
legacyNotebookFNV = 4d7705c5 -> 4d7705c5
PAK transient     = 4364 B, fully recovered
```

Post-PARK heartbeat on the merged NOTE firmware:

```text
uptime=25893 ms
heap=134536
heap8=68772
largest8=36852
all reported subsystems = ready
```

## Current CHECK_KEY candidate contract

Recovered legacy key mapping:

```text
arg1=0 -> green  -> mask 0x1 -> "Need Green Key"
arg1=1 -> yellow -> mask 0x2 -> "Need Yellow Key"
arg1=2 -> blue   -> mask 0x4 -> "Need Blue Key"
arg1=3 -> red    -> mask 0x8 -> "Need Red Key"
```

Exact dynamic behavior:

```text
required key present:
  Game_executeEvent legacy return = false
  script continues
  Hud/Sound/saveTileEvent = untouched

required key absent:
  message = Need <Color> Key
  sound   = 5065
  Game_executeEvent legacy return = true
  saveTileEvent = true
  Game_runEvent saves current command offset + active flags and stops
```

Permanent API:

```text
EspMapKeyGate_evaluate(descriptor, commandOffset, keyBits, outResult)
EspMapKeyGate_message(result)
```

The evaluator revalidates the descriptor against the immutable runtime, accepts only opcode 41 and key selectors `0..3`, ignores key bits above the low nibble and returns no persistent state.

Expected result ABI:

```text
source event/global command/source offset
key index + mask
sound id
legacy return bit
stop-event bit
save-current-command bit
sizeof = 12 B
```

The permanent source depends only on native runtime/event APIs, not on legacy `Player`, `Hud`, `Sound`, `Game`, entity or render structures.

## Current CHECK_KEY hardware probe

The real CHECK_KEY count is intentionally not predeclared. The complete source bytecode is already protected by hardware-proven `opcodeAuditFNV=6f28df45`; this probe will establish family-specific counts on the real CYD.

For each real CHECK_KEY, all sixteen key contexts `0..15` are evaluated. Acceptance requires:

```text
refs > 0
color counts sum to refs
scenarios = refs * 16
PASS      = refs * 8
BLOCKED   = refs * 8
stateExecRefused = refs
resultBytes = 12
messages = 4 / 4 exact
higher key bits ignored
```

The first real-CYD PASS will establish:

```text
CHECK_KEY ref count
green/yellow/blue/red distribution
first real sample metadata
keyGateFNV
new-build absolute heap/framebuffer values
legacy Player.keys baseline
Hud witness FNV
```

Fail-closed set:

```text
unsupported non-CHECK_KEY command
bad command offset
non-canonical descriptor
NULL descriptor
NULL result
```

All non-NULL-result failures must return a zeroed result.

Hardware integrity must preserve:

```text
heap / largest block / framebuffer
arena/map/script fingerprints
legacy notebook FNV 4d7705c5
legacy Player.keys
Hud message witness
Game continuation fields
pack remains closed
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
 -> candidate: real EV_CHECK_KEY -> pure native dynamic gate
```

Still forbidden:

```text
actual DoomCanvas dialog/password presentation
legacy Game continuation mutation by native code
legacy Hud mutation
legacy Player key/notebook mutation
actual sound playback from native gate
full native Game_runEvent execution loop
world/door/line/sprite mutation
map transitions
savegame mutation
entity/monster activation
native gameplay rendering
ST_PLAYING
```

## Remaining MAP_INTRO families after current candidate

Not yet owned/executed natively:

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

PASSWORD is still a bounded UI/input candidate. SHOW/HIDE/GIVEMAP/UNLOCK/OPENLINE/CLOSELINE cross into the first explicit world/render overlays. CHANGEMAP and SAVEGAME remain later boundaries.

## Current validation target

Build/flash normal optimized:

```text
esp32-cyd
```

from `agent/esp32-map1-native-key-gate` and capture `[MAPKEY]` / `[MAPKEYPROBE]` plus a later stable `[ALIVE]` heartbeat.

No CI status is published for firmware candidate `3b4844e8fa5d38d522e1adc70ffac646978f130d`. Do not mark merge-ready until the real classic CYD supplies the PASS.

## Next bounded milestone after PASS + merge

Reread the then-current repository, recovery docs, merged CHECK_KEY milestone and exact remaining legacy behavior. Do not pre-authorize PASSWORD or a world-mutation family before that recovery.
