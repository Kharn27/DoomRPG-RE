# ESP32 MAP_INTRO native NOTE notebook-owner milestone

Branch: `agent/esp32-map1-native-notebook-owner`

Base merged `main`:

```text
PR   = #53 — native DIALOG/NOBACK pause owner
main = 395418510207bf24ac45ddbb4c4c15db3ddc8998
```

Firmware candidate content:

```text
f619aefc85402d28c4de6edab5ca32ea1eb514dd
```

Status: **IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING**.

## Objective

Cross exactly one additional native effect boundary:

```text
real EV_NOTE bytecode
 -> EspMapUiIntent
 -> canonical provenance validation
 -> EspMapStrings_read()
 -> bounded native notebook append
```

This milestone supports only opcode `40 / EV_NOTE`. It does not mutate legacy `Player.NotebookString`, does not present the notes menu, does not run a full native event loop and does not mutate world/entities/rendering.

## Recovered legacy behavior

`Game_executeEvent()` handles opcode 40 as:

```text
str = player->NotebookString
snprintf(str, sizeof(player->NotebookString), "%s%s||",
         str, mapStringsIDs[arg1])
```

`Player_t` owns:

```text
char NotebookString[512]
```

`Player_setup()` resets that map-local notebook with:

```text
NotebookString[0] = '\0'
```

`Menu_setNotes()` later splits the accumulated text on `|`, so each `EV_NOTE` contributes its text followed by two separator characters.

The desktop C expression aliases destination and first `%s` input. The permanent ESP32 owner makes the intended bounded behavior explicit and deterministic:

```text
existing notebook text
+ source note up to its first C NUL
+ "||"
truncated to at most 511 payload bytes
+ synthesized trailing NUL
```

Once the notebook reaches 511 payload bytes, later note commands still validate/read successfully but cannot change the stored text.

## Permanent native API

New files:

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

Expected classic ESP32 ABI footprint:

```text
notebook value        = 514 B
text capacity         = 512 B
maximum text payload  = 511 B
persistent heap       = 0 B in this probe
```

The 512-byte bounded text storage is deliberate: unlike DIALOG and FORCE_MESSAGE, `EV_NOTE` creates persistent concatenated player text state rather than merely retaining a map string reference.

API:

```text
EspMapNotebook_reset()
EspMapNotebook_length()
EspMapNotebook_text()
EspMapNotebook_apply()
```

`EspMapNotebook_apply()` accepts:

```text
caller-owned EspMapNotebookState
current native-pack BSP entry
one EspMapUiIntent
caller-owned bounded string scratch
```

It supports only a fully validated NOTE intent:

```text
codeId = 40
kind   = ESP_MAP_UI_INTENT_APPEND_NOTE
flags  = ESP_MAP_UI_INTENT_FLAG_APPEND_NOTE_SEPARATOR
arg1   = text.index
```

Before reading or mutating state it revalidates:

```text
current state C-string/length consistency
canonical string ref
source event exists
source command exists
global command index matches descriptor
bytecode id/arg1/arg2 match the source command
```

The semantic notebook changes only after `EspMapStrings_read()` succeeds. Unsupported, malformed, short-buffer and I/O failures leave the owner unchanged.

## Inherited MAP_INTRO corpus

Already hardware-proven UI-intent data:

```text
EV_NOTE refs = 7
```

Canonical NOTE sample:

```text
global command = 103
event          = 40
command offset = 8
string         = 85 @ 18964 + 54
payload FNV    = ee639dc1
flags          = 10
```

The state-only opcode executor must continue to refuse all seven NOTE commands.

The legacy Player notebook is currently empty after intro disposal/setup:

```text
FNV32 over all 512 bytes = 4d7705c5
```

That legacy buffer is read only as an integrity witness in this milestone.

## Temporary hardware probe

New files:

```text
ESP32/include/native_map1_notebook_probe.h
ESP32/src/native_map1_notebook_probe.c
```

The probe runs only after the hardware-proven DIALOG/NOBACK owner stage.

It opens `/DoomRPG-ESP32.pak`, resolves `/intro.bsp`, walks all 93 event descriptors / 265 bytecodes and applies only the seven real `EV_NOTE` commands to a caller-owned native notebook.

Acceptance requires:

```text
NOTE refs            = 7
append matches       = 7 / 7
separator intents    = 7
reader guards        = 7 / 7
state executor       = UNSUPPORTED for all 7
owner value          = 514 B
text capacity        = 512 B
persistent heap      = 0 B
legacy notebook      = unchanged
```

The real-CYD run will establish, rather than predeclare:

```text
noteApplyFNV
sourceBytes across seven NOTE payloads
final native notebook length
final active-content FNV
final 512-byte storage FNV
new-build absolute heap/framebuffer values
```

## Explicit bounds proof

The probe additionally applies the canonical real NOTE in controlled local states to prove:

```text
empty state + sample -> note text + "||" + NUL
510-byte state + sample -> exactly 511-byte payload + NUL
511-byte full state + sample -> exact state unchanged
```

This validates separator handling and truncation independently of whether the seven real MAP_INTRO notes naturally fill the 512-byte notebook.

## Atomic fail-closed proof

Starting from the real accumulated native notebook, the probe requires all of these to be refused without mutation:

```text
DIALOG intent                   -> UNSUPPORTED
NOTE with bad flags             -> INVALID
NOTE with bad intent kind       -> INVALID
mutated canonical string ref    -> INVALID
bad source event                -> INVALID
bad global command index        -> INVALID
scratch without terminator room -> BUFFER_TOO_SMALL
NULL intent                     -> INVALID
valid NOTE with PAK closed      -> IO_ERROR
```

A final reset must clear all 514 bytes of owner state deterministically.

## Hardware integrity boundary

Before and after the complete probe the following must remain exact:

```text
heap8
largest8
framebuffer FNV
arenaFNV      = c3882516
mapStateFNV   = cd99b98e
scriptFNV     = f9e3d9df
legacy Player.NotebookString FNV = 4d7705c5
Hud.statBarMessage pointer
Game.skipAdvanceTurn
Game.saveTileEvent
Game.tileEvent
Game.tileEventIndex
Game.tileEventFlags
```

The pack must be closed again before PARK.

Permanent prohibitions remain:

```text
shapeData                          = NULL
mediaTexels                        = NULL
legacy Player notebook mutation   = no
legacy Hud mutation               = no
legacy Game continuation mutation = no
actual notes-menu presentation    = no
world/entity/render mutation      = no
entities                           = 0
monsters                           = 0
ST_PLAYING                         = no
```

## Expected Serial family

```text
[MAPNOTEPROBE] ARMED ...

=== Doom RPG ESP32-native MAP_INTRO NOTE notebook owner ===
[MAPNOTE] READY refs=7 separators=7 appendMatches=7 ownerBytes=514 textCapacity=512 stateExecRefused=7 sourceBytes=... finalLen=... noteApplyFNV=... contentFNV=... storageFNV=... elapsed=...ms
[MAPNOTE] SAMPLE cmd=103 event=40 off=8 string=85@18964+54 payloadFNV=ee639dc1
[MAPNOTE] BOUNDS separator=1 truncation=1 fullStable=1 guards=7/7 terminator=yes
[MAPNOTE] FAILCLOSED unsupported=1 badFlags=1 badKind=1 badRef=1 badEvent=1 badGlobal=1 shortBuffer=1 nullIntent=1 closedPack=1 ownerAtomic=yes reset=1
[MAPNOTE] IO entry=/intro.bsp size=21823 crc32=623f34e4 ... packIO=yes persistentHeapBytes=0
[MAPNOTEPROBE] RAM ... legacyNotebookFNV=4d7705c5->4d7705c5
[MAPNOTEPROBE] PARK ... nativeNotebookOwner=yes ownerValueBytes=514 textCapacity=512 legacyNotebookMutation=no ...
[ALIVE] ...
```

Use the normal optimized PlatformIO environment:

```text
esp32-cyd
```

Until the real classic CYD supplies the PASS log and a stable post-PARK heartbeat, this branch is **not merge-ready**.

## Next boundary after a hardware PASS

Do not preselect the next opcode family. After PASS + merge, reread the then-current `main`, recovery docs, this milestone and exact remaining MAP_INTRO legacy behavior before choosing the next bounded mutation/effect owner.
