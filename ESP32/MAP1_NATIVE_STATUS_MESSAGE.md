# ESP32 MAP_INTRO native FORCE_MESSAGE status-owner milestone

Branch: `agent/esp32-map1-native-force-message-owner`

Base merged `main`:

```text
PR   = #51 — bounded native map string reader
main = 526640b12d978fdbe9c8a9239c37db2fca95cddd
```

Hardware-tested firmware content:

```text
d782681c3cd267b9f16c290a593c1b6e5b34df1c
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

Cross exactly one native effect boundary after the hardware-proven UI intents and bounded string reader:

```text
real EV_FORCEMESSAGE bytecode
 -> EspMapUiIntent
 -> EspMapStrings_read()
 -> recovered first-byte empty/non-empty decision
 -> compact caller-owned native status-message state
```

This milestone supports only opcode `24 / EV_FORCEMESSAGE`. Dialog pause/resume, notebook append, actual HUD presentation, world mutation, entities, rendering and `ST_PLAYING` remain outside the boundary.

## Recovered legacy behavior

`Game_executeEvent()` handles opcode 24 as:

```text
if mapStringsIDs[arg1][0] != '\0'
    Hud.statBarMessage = mapStringsIDs[arg1]
else
    Hud.statBarMessage = NULL
break
```

`Hud_drawTopBar()` later uses `statBarMessage` as a fallback text source when the normal message queue is empty.

Legacy therefore stores a pointer to map-owned text rather than copying the string. The native equivalent keeps an immutable `EspMapStringRef` plus an active bit instead of a persistent 314-byte text buffer.

## Permanent native API

Files:

```text
ESP32/include/esp_map_status_message.h
ESP32/src/esp_map_status_message.c
```

State:

```c
typedef struct EspMapStatusMessageState_s {
    EspMapStringRef text;
    uint8_t active;
    uint8_t reserved;
} EspMapStatusMessageState;
```

Real classic-CYD ABI footprint:

```text
owner value            = 8 B
persistent copied text = 0 B
persistent heap bytes  = 0 B in this probe
```

API:

```text
EspMapStatusMessage_reset()
EspMapStatusMessage_isActive()
EspMapStatusMessage_getRef()
EspMapStatusMessage_apply()
```

`EspMapStatusMessage_apply()` accepts only a validated `EV_FORCEMESSAGE` intent:

```text
codeId = 24
kind   = ESP_MAP_UI_INTENT_FORCE_MESSAGE
flags  = ESP_MAP_UI_INTENT_FLAG_CLEAR_IF_EMPTY
arg1   = text.index
```

It uses the already-proven bounded `EspMapStrings_read()` API. The semantic owner is committed only after successful I/O.

Recovered rule:

```text
scratch[0] != '\0' -> active=1, retain canonical EspMapStringRef
scratch[0] == '\0' -> reset/clear owner
```

This preserves the exact legacy first-C-byte behavior.

## Real-CYD hardware proof

Normal optimized environment:

```text
esp32-cyd
```

The real classic no-PSRAM CYD executed the complete probe successfully.

Real MAP_INTRO force-message corpus:

```text
refs       = 3
set        = 1
clear      = 2
transition = 1
ownerBytes = 8
textCopyBytes = 0
stateExecRefused = 3
statusApplyFNV = 52b25a5f
```

Canonical set fixture:

```text
command      = 4
event        = 2
command off  = 0
string       = 1 @ 11569 + 14
payload FNV  = f6da01bb
```

Canonical first clear fixture:

```text
command      = 5
event        = 2
command off  = 1
string       = 2 @ 11585 + 0
```

The one non-empty set and two zero-length clears exactly match the earlier UI-intent/string-reader corpus. All three commands remain `UNSUPPORTED` in the state-only opcode executor, preserving effect-family separation.

## Atomic fail-closed proof

All deliberately invalid paths were refused on hardware while preserving the active owner state:

```text
unsupported DIALOG intent = 1
bad FORCE_MESSAGE flags    = 1
mutated canonical ref      = 1
short scratch buffer       = 1
NULL intent                = 1
closed PAK                 = 1
ownerAtomic                = yes
```

The closed-pack case proves that a storage failure cannot partially clear or replace an existing native status owner.

## Storage / RAM proof

The probe opened the same native backing store entry:

```text
entry  = /intro.bsp
size   = 21823
CRC32  = 623f34e4
```

Transient PAK ownership:

```text
heap before open    = 68796 B
heap while open     = 64432 B
transient heap cost = 4364 B
largest while open  = 36852 B
elapsed             = 37 ms
heapPersistentBytes = 0 B
```

After close, every measured boundary recovered exactly:

```text
heap8        = 68796 -> 68796
largest8     = 36852 -> 36852
frameFNV     = faa62417 -> faa62417
arenaFNV     = c3882516 -> c3882516
mapStateFNV  = cd99b98e -> cd99b98e
scriptFNV    = f9e3d9df -> f9e3d9df
notebookFNV  = 4d7705c5 -> 4d7705c5
```

The previous merged reader firmware had `heap8=68804` and `frameFNV=805df09e`. This candidate build reports `68796` and `faa62417`, but every stage has exact before/after stability and all inherited native fingerprints remain canonical. These are therefore build-to-build layout/content differences, not persistent owner allocation or framebuffer mutation.

## Final effect boundary

Hardware PARK proved:

```text
nativeArena              = yes
nativeTileState          = yes
nativeEventLookup        = yes
nativeEventDescriptor    = yes
nativeScriptState        = yes
nativeFilter             = yes
nativeOpcodeExec         = yes
nativeUiIntent           = yes
nativeStringReader       = yes
nativeStatusMessageOwner = yes
ownerValueBytes          = 8
textCopyBytes            = 0
legacyHudMutation        = no
worldMutation            = no
framebufferMutation      = no
entities                 = 0
monsters                 = 0
noGameplay               = yes
```

A full later heartbeat from the same tested firmware remained stable:

```text
uptime=210115 ms
heap=134560
heap8=68796
largest8=36852
SD/ZIP/VIDEO/CORE/LAYOUT/PRERENDER/RENDER/MAPPINGS/MENUBSP = ready
```

A subsequent `[ALIVE] uptime=215116 ms` marker was also seen, but the supplied line was truncated; it is not required for acceptance because the complete post-PARK heartbeat already proves steady-state recovery.

Permanent invariants remain:

```text
shapeData   == NULL
mediaTexels == NULL
legacy mapStringsIDs == NULL
legacy Hud mutation  = no
world mutation       = no
framebuffer mutation = no
entities             = 0
monsters             = 0
ST_PLAYING            = no
```

## Hardware acceptance status

The complete `EV_FORCEMESSAGE` owner probe plus post-PARK heartbeat is a **REAL-CYD HARDWARE PASS**.

This branch is **MERGE-READY**. The firmware-bearing commit is `d782681c3cd267b9f16c290a593c1b6e5b34df1c`; all later commits must remain documentation-only unless another flash is performed.

## Next bounded milestone after merge

Prefer a compact native `EV_DIALOG` / `EV_DIALOGNOBACK` owner carrying only the data required to reproduce the recovered pause/resume contract:

```text
immutable text ref
Back / no-Back mode
source event
resume command offset
pause-script / skip-turn semantics
```

That next milestone should still stop before actual dialog presentation, legacy `DoomCanvas` mutation, or a full native event execution loop.
