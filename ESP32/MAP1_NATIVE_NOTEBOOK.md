# ESP32 MAP_INTRO native NOTE notebook-owner milestone

Branch: `agent/esp32-map1-native-notebook-owner`

Base merged `main`:

```text
PR   = #53 — native DIALOG/NOBACK pause owner
main = 395418510207bf24ac45ddbb4c4c15db3ddc8998
```

Hardware-tested firmware content:

```text
f619aefc85402d28c4de6edab5ca32ea1eb514dd
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

Cross exactly one additional native effect boundary:

```text
real EV_NOTE bytecode
 -> EspMapUiIntent
 -> canonical provenance validation
 -> EspMapStrings_read()
 -> bounded native notebook append
```

This milestone supports only opcode `40 / EV_NOTE`. It does not mutate legacy `Player.NotebookString`, present the notes menu, run a full native event loop or mutate world/entities/rendering.

## Recovered legacy behavior

`Game_executeEvent()` handles opcode 40 as:

```text
str = player->NotebookString
snprintf(str, sizeof(player->NotebookString), "%s%s||",
         str, mapStringsIDs[arg1])
```

`Player_t` owns `char NotebookString[512]`. `Player_setup()` resets it with `NotebookString[0] = '\0'`. `Menu_setNotes()` later splits the accumulated text on `|`, so each `EV_NOTE` contributes its text followed by two separator characters.

The permanent ESP32 owner makes the intended bounded behavior explicit and deterministic:

```text
existing notebook text
+ source note up to its first C NUL
+ "||"
truncated to at most 511 payload bytes
+ trailing NUL
```

Once the notebook reaches 511 payload bytes, later valid NOTE commands can still validate/read but cannot change the stored text.

## Permanent native API

Files:

```text
ESP32/include/esp_map_notebook.h
ESP32/src/esp_map_notebook.c
```

State:

```c
typedef struct EspMapNotebookState_s {
    char text[512];
    uint16_t length;
} EspMapNotebookState;
```

Real classic-CYD ABI footprint:

```text
notebook value        = 514 B
text capacity         = 512 B
maximum text payload  = 511 B
persistent heap       = 0 B in this probe
```

The 512-byte bounded text storage is deliberate: unlike DIALOG and FORCE_MESSAGE, `EV_NOTE` creates persistent concatenated player text state rather than merely retaining a map-string reference.

Allocation-free API:

```text
EspMapNotebook_reset()
EspMapNotebook_length()
EspMapNotebook_text()
EspMapNotebook_apply()
```

`EspMapNotebook_apply()` supports only a validated NOTE intent and revalidates current-state C-string/length consistency, canonical string ref, source event/command, global command index, opcode and args before reading or mutating. Semantic state is committed only after `EspMapStrings_read()` succeeds. Unsupported, malformed, short-buffer and I/O failures leave the owner unchanged.

## Real-CYD hardware proof

Normal optimized environment:

```text
esp32-cyd
```

The real classic no-PSRAM CYD executed the complete NOTE probe successfully.

Real MAP_INTRO corpus:

```text
refs             = 7
separators       = 7
appendMatches    = 7
ownerBytes       = 514
textCapacity     = 512
stateExecRefused = 7
sourceBytes      = 256
finalLen         = 270
noteApplyFNV     = 43183162
contentFNV       = 599609e0
storageFNV       = 75cf54e0
elapsed          = 83 ms
```

Canonical NOTE sample:

```text
global command = 103
event          = 40
command offset = 8
string         = 85 @ 18964 + 54
payload FNV    = ee639dc1
```

All seven NOTE commands remain refused by the state-only opcode executor, preserving the state/effect-owner split.

## Bounds proof

Hardware proved the controlled local-state cases:

```text
separator  = 1
truncation = 1
fullStable = 1
guards     = 7 / 7
terminator = yes
```

Therefore the owner preserves the exact `text + "||"` shape, truncates at 511 payload bytes with a trailing NUL, and becomes stable once full.

## Atomic fail-closed proof

Starting from the real accumulated native notebook, every deliberately invalid path was refused atomically:

```text
unsupported DIALOG = 1
bad flags          = 1
bad kind           = 1
bad ref            = 1
bad source event   = 1
bad global index   = 1
short scratch      = 1
NULL intent        = 1
closed pack        = 1
ownerAtomic        = yes
reset              = 1
```

The final reset cleared the complete 514-byte owner deterministically.

## Native-pack I/O / RAM proof

The owner reads only the seven individual NOTE strings through the proven bounded reader. The native-pack open cost was fully transient:

```text
entry             = /intro.bsp
size              = 21823
crc32             = 623f34e4
heapOpen          = 64408
transientHeapCost = 4364 B
largestOpen       = 36852
packIO            = yes
persistentHeap    = 0 B
```

Before/after the complete NOTE stage:

```text
heap8              = 68772 -> 68772
largest8           = 36852 -> 36852
frameFNV           = a3e3cc8e -> a3e3cc8e
arenaFNV           = c3882516 -> c3882516
mapStateFNV        = cd99b98e -> cd99b98e
scriptFNV          = f9e3d9df -> f9e3d9df
legacyNotebookFNV  = 4d7705c5 -> 4d7705c5
```

The absolute `heap8` and framebuffer FNV differ from earlier firmware builds, but this stage has exact before/after stability and all inherited structural fingerprints remain canonical. This is therefore a build-to-build layout/content difference rather than persistent NOTE-owner allocation or framebuffer mutation.

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
nativeNotebookOwner          = yes
ownerValueBytes              = 514
textCapacity                 = 512
legacyNotebookMutation       = no
legacyHudMutation            = no
legacyGameContinuationMutation = no
worldMutation                = no
framebufferMutation          = no
entities                     = 0
monsters                     = 0
noGameplay                   = yes
```

A complete post-PARK heartbeat from the same tested firmware remained stable:

```text
uptime=25893 ms
heap=134536
heap8=68772
largest8=36852
SD/ZIP/VIDEO/CORE/LAYOUT/PRERENDER/RENDER/MAPPINGS/MENUBSP = ready
```

A later heartbeat was truncated after `VIDEO=`; the complete heartbeat above is sufficient for acceptance.

Permanent prohibitions remain:

```text
shapeData                          = NULL
mediaTexels                        = NULL
legacy Player notebook mutation   = no
legacy Hud mutation               = no
legacy Game continuation mutation = no
actual notes-menu presentation    = no
full native Game_runEvent loop    = no
world/entity/render mutation      = no
entities                           = 0
monsters                           = 0
ST_PLAYING                         = no
```

## Hardware acceptance status

The complete `EV_NOTE` notebook-owner probe plus post-PARK heartbeat is a **REAL-CYD HARDWARE PASS**.

This branch is **MERGE-READY**. The firmware-bearing commit is `f619aefc85402d28c4de6edab5ca32ea1eb514dd`; every later commit must remain documentation-only unless another flash is performed.

## Next bounded milestone after merge

Do not preselect the next opcode family. After merge, reread the then-current `main`, recovery docs, this merged NOTE milestone and exact remaining MAP_INTRO legacy behavior before choosing the next coherent native mutation/effect boundary.
