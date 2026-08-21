# ESP32 MAP_INTRO native DIALOG pause-owner milestone

Branch: `agent/esp32-map1-native-dialog-owner`

Base merged `main`:

```text
PR   = #52 — native FORCE_MESSAGE status owner
main = 40b61af5e2115266d4d03dddcc3175850538b0f5
```

Hardware-tested firmware content:

```text
85aa89c4218a819e7f18cbf77f64dfbef3c5bac9
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

Cross exactly one additional native UI-effect ownership boundary:

```text
real EV_DIALOG / EV_DIALOGNOBACK bytecode
 -> EspMapUiIntent
 -> canonical provenance validation
 -> compact caller-owned pause/presentation owner
```

This milestone stops before actual dialog presentation and before a full native event execution loop.

## Recovered legacy behavior

`Game_executeEvent()` handles both dialog opcodes through the same path:

```text
DoomCanvas_startDialog(mapStringsIDs[arg1], codeId == EV_DIALOG)
game->saveTileEvent = true
game->tileEvent = event
game->skipAdvanceTurn = true
```

`Game_runEvent()` records the current command offset when `saveTileEvent` is observed. When the dialog closes, `DoomCanvas` resumes with:

```text
Game_runEvent(game, game->tileEvent, game->tileEventIndex + 1,
              game->tileEventFlags)
```

Therefore the static semantics owned here are:

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

The dynamic activation flags represented by legacy `tileEventFlags` remain future native event-loop invocation context and are intentionally not invented by this owner.

## Permanent native API

Files:

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

Real classic-CYD ABI footprint:

```text
owner value            = 12 B
persistent copied text = 0 B
persistent heap bytes  = 0 B in this probe
```

Allocation-free API:

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

It retains only the immutable `EspMapStringRef`. A future native dialog presenter will resolve that ref through the hardware-proven `EspMapStrings_read()` boundary when text is actually needed for presentation.

## Real-CYD hardware proof

Normal optimized environment:

```text
esp32-cyd
```

The real classic no-PSRAM CYD executed the complete probe successfully.

Real MAP_INTRO corpus:

```text
refs             = 84
Back             = 76
noBack           = 8
pause            = 84
skipTurn         = 84
resumeExact      = 84
ownerBytes       = 12
textCopyBytes    = 0
stateExecRefused = 84
dialogApplyFNV   = d0254f3d
elapsed          = 2 ms
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

All 84 commands remain refused by the state-only opcode executor, preserving the state/effect-family split.

## Atomic fail-closed proof

Starting from a real active dialog owner, every deliberately invalid path was refused on hardware while preserving the owner:

```text
unsupported FORCE_MESSAGE = 1
bad flags                 = 1
bad kind                  = 1
mutated text ref          = 1
bad source event          = 1
bad global command index  = 1
bad resume offset         = 1
NULL intent               = 1
ownerAtomic               = yes
reset                     = 1
```

The final reset cleared the owner exactly.

## RAM / integrity proof

The owner stage performs no pack I/O and no persistent heap allocation:

```text
packIO              = no
persistentHeapBytes = 0
heap8                = 68780 -> 68780
largest8             = 36852 -> 36852
frameFNV             = ef79123a -> ef79123a
arenaFNV             = c3882516 -> c3882516
mapStateFNV          = cd99b98e -> cd99b98e
scriptFNV            = f9e3d9df -> f9e3d9df
notebookFNV          = 4d7705c5 -> 4d7705c5
```

The prior merged FORCE_MESSAGE firmware reported `heap8=68796` and `frameFNV=faa62417`. This dialog-owner build reports `68780` and `ef79123a`; every stage has exact before/after stability and all inherited structural fingerprints remain canonical, so these are build-to-build layout/content differences rather than owner allocations or framebuffer mutation.

## Final effect boundary

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
ownerValueBytes              = 12
textCopyBytes                = 0
legacyDialogMutation         = no
legacyGameContinuationMutation = no
worldMutation                = no
framebufferMutation          = no
entities                     = 0
monsters                     = 0
noGameplay                   = yes
```

A complete post-PARK heartbeat from the same firmware remained stable:

```text
uptime=21790 ms
heap=134544
heap8=68780
largest8=36852
SD/ZIP/VIDEO/CORE/LAYOUT/PRERENDER/RENDER/MAPPINGS/MENUBSP = ready
```

A subsequent `[ALIVE] uptime=` line was truncated in the supplied capture and is not needed for acceptance because the complete post-PARK heartbeat already proves steady-state recovery.

Permanent prohibitions remain:

```text
actual DoomCanvas dialog presentation = no
legacy Game continuation mutation     = no
legacy Hud mutation                   = no
world/entity/render mutation          = no
shapeData                             = NULL
mediaTexels                           = NULL
entities                              = 0
monsters                              = 0
ST_PLAYING                            = no
```

## Hardware acceptance status

The complete `EV_DIALOG` / `EV_DIALOGNOBACK` pause-owner probe plus post-PARK heartbeat is a **REAL-CYD HARDWARE PASS**.

This branch is **MERGE-READY**. The firmware-bearing commit is `85aa89c4218a819e7f18cbf77f64dfbef3c5bac9`; all later commits must remain documentation-only unless another flash is performed.

## Next bounded milestone after merge

After merge, reread the then-current repository and exact legacy behavior before selecting the next boundary. `EV_NOTE` remains a likely small explicit owner candidate, but it is not pre-authorized as the next implementation.
