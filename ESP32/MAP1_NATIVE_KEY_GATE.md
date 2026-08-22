# ESP32 MAP_INTRO native CHECK_KEY gate milestone

Branch: `agent/esp32-map1-native-key-gate`

Base merged `main`:

```text
PR   = #54 — native NOTE notebook owner
main = 03002f79eb03bdcb4c9e430c43e4693dab47e44b
```

Hardware-tested firmware content:

```text
3b4844e8fa5d38d522e1adc70ffac646978f130d
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

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

`Game_executeEvent()` maps `arg1` as:

```text
0 -> green  -> bit 0x1 -> "Need Green Key"
1 -> yellow -> bit 0x2 -> "Need Yellow Key"
2 -> blue   -> bit 0x4 -> "Need Blue Key"
3 -> red    -> bit 0x8 -> "Need Red Key"
```

Required key present:

```text
Game_executeEvent returns false
script continues
no Hud/Sound/save effect
```

Required key missing:

```text
Hud_addMessage("Need <Color> Key")
Sound_playSound(5065, 0, 2)
saveTileEvent = true
Game_executeEvent returns true
```

`Game_runEvent()` then saves the **current command offset** and active flags, clears the transient latch and stops the event loop.

Therefore the permanent native result owns only the decision metadata:

```text
PASS:
  legacyReturnValue = 0
  stopEvent = 0
  saveCurrentCommand = 0
  sound = 0
  message = none

BLOCKED:
  legacyReturnValue = 1
  stopEvent = 1
  saveCurrentCommand = 1
  saved offset = source command offset
  sound = 5065
  message = exact key-color text
```

Malformed key selectors outside `0..3` are rejected fail-closed.

## Permanent native API

Files:

```text
ESP32/include/esp_map_key_gate.h
ESP32/src/esp_map_key_gate.c
```

Result:

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

Real classic-CYD ABI footprint:

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

The evaluator revalidates the supplied descriptor against the current immutable runtime, reads only the linked canonical command, supports only opcode 41 with selectors `0..3`, and ignores key bits above the low nibble. The permanent implementation has no legacy `Player`, `Hud`, `Sound`, `Game`, entity or render dependency.

## Real-CYD corpus proof

Normal optimized environment:

```text
esp32-cyd
```

The real classic no-PSRAM CYD established the exact MAP_INTRO CHECK_KEY corpus:

```text
refs             = 1
green            = 0
yellow           = 1
blue             = 0
red              = 0
scenarios        = 16
PASS             = 8
BLOCKED          = 8
resultBytes      = 12
stateExecRefused = 1
keyGateFNV       = 9ace79cd
elapsed          = 1 ms
```

Canonical real command:

```text
global command = 38
event          = 11
command offset = 0
key selector   = 1 / yellow
required mask  = 0x02
arg2           = 0x00000100
missing message= "Need Yellow Key"
sound          = 5065
saved offset   = current command
```

The state-only opcode executor still refuses opcode 41, preserving the split between state mutation and dynamic control/effect evaluation.

## Full truth-table proof

The one real command was evaluated with every low-nibble key context `0..15`:

```text
perRef           = 16
passEach         = 8
blockedEach      = 8
messages         = 4 / 4 exact
extraBitsIgnored = 1
PASS effect      = none
BLOCKED effect   = message + sound + stop + save-current
```

Even though only yellow occurs in MAP_INTRO, the probe separately validates all four immutable message mappings.

## Fail-closed proof

Hardware proved:

```text
unsupported non-CHECK_KEY = 1
bad command offset        = 1
noncanonical descriptor   = 1
NULL descriptor           = 1
NULL result               = 1
```

All failures with a writable result leave that result zeroed.

## RAM / integrity proof

Before and after the complete CHECK_KEY stage:

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

The immediately preceding NOTE stage on the same firmware also remained stable:

```text
heap8             = 68756 -> 68756
largest8          = 36852 -> 36852
frameFNV          = c56f998b -> c56f998b
noteApplyFNV      = 43183162
contentFNV        = 599609e0
storageFNV        = 75cf54e0
heapOpen          = 64384
transientHeapCost = 4372 B
persistentHeap    = 0 B
```

The absolute heap/frame values differ slightly from prior firmware builds, but each probe has exact before/after stability and all inherited structural fingerprints remain canonical. These are build-layout/content differences, not persistent allocation or framebuffer mutation.

## Final boundary

PARK proved:

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
resultBytes                  = 12
persistentBytes              = 0
legacyKeyMutation            = no
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
uptime=25410 ms
heap=134520
heap8=68756
largest8=36852
SD/ZIP/VIDEO/CORE/LAYOUT/PRERENDER/RENDER/MAPPINGS/MENUBSP = ready
```

Permanent prohibitions remain:

```text
shapeData                          = NULL
mediaTexels                        = NULL
legacy Player key/notebook mutation = no
legacy Hud mutation               = no
legacy Game continuation mutation = no
actual sound playback from native gate = no
full native Game_runEvent loop    = no
world/entity/render mutation      = no
map transitions                   = no
savegame mutation                 = no
entities                           = 0
monsters                           = 0
ST_PLAYING                         = no
```

## Hardware acceptance status

The full real CHECK_KEY corpus, complete 16-context truth table, fail-closed paths, integrity witnesses and stable post-PARK heartbeat are a **REAL-CYD HARDWARE PASS**.

This branch is **MERGE-READY**. The firmware-bearing commit is `3b4844e8fa5d38d522e1adc70ffac646978f130d`; every later commit must remain documentation-only unless another flash is performed.

## Remaining MAP_INTRO opcode families

After CHECK_KEY, still unowned:

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

Do not preselect the next family. After merge, reread the new `main`, recovery docs, this milestone and exact remaining legacy behavior before choosing between password/input ownership and the first bounded world-mutation family.
