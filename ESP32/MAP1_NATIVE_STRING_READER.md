# ESP32 MAP_INTRO bounded native string reader milestone

Branch: `agent/esp32-map1-native-string-reader`

Base merged `main`:

```text
PR   = #50 — allocation-free native string spans + UI intents
main = 9a5e8ade361180d220f2b3614a443e5efb0d27bd
```

Hardware-tested firmware content:

```text
d13d5eb13c4657d5ec5c16fd82939cfc38989c86
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

Cross exactly one storage boundary after the hardware-proven `EspMapStringRef` / UI-intent milestone:

```text
EspMapStringRef
 -> validate against the current native map source
 -> exact range read from /DoomRPG-ESP32.pak
 -> caller-owned bounded buffer
 -> synthesized C terminator
 -> content audit / fingerprint
```

This milestone does **not** present dialogs, mutate the HUD/notebook, execute additional opcodes, activate entities or enter gameplay.

## Permanent native API

`ESP32/include/esp_map_strings.h` exposes:

```c
EspMapStringReadStatus EspMapStrings_read(
    const EspAssetPackEntry* sourceEntry,
    const EspMapStringRef* ref,
    char* destination,
    size_t capacity,
    size_t* outLength);
```

The reader is caller-owned and allocation-free. It keeps no global text cache and allocates no persistent buffer.

Success requires:

```text
source entry size == current EspMapRuntime sourceBytes
source entry CRC  == current EspMapRuntime sourceCrc32
ref                == freshly resolved canonical ref for ref.index
capacity           >= ref.length + 1
range              inside source entry
native pack         open for non-empty payload I/O
```

On success exactly `ref.length` source bytes are read and `destination[ref.length]` is set to `\0`. The terminator is synthesized by the reader and is not part of the BSP payload.

Fail-closed statuses:

```text
INVALID
BUFFER_TOO_SMALL
IO_ERROR
OK
```

A zero-length payload requires no SD payload read but still receives the synthesized terminator after source/ref validation.

## Real-CYD hardware result

The normal optimized `esp32-cyd` firmware executed the complete bounded reader probe on the real classic no-PSRAM CYD.

Inherited source identity remained exact:

```text
entry          = /intro.bsp
source bytes   = 21823
CRC32          = 623f34e4
strings        = 94
payload        = 7779 B
max payload    = 313 B
spanFNV        = 713188eb
```

Every one of the 94 canonical refs matched its preceding on-disk little-endian source length prefix:

```text
prefixMatches = 94 / 94
```

All 94 strings were read into a bounded caller-owned buffer with guard bytes intact:

```text
guards           = 94 / 94
payload bytes    = 7779
zeroLength       = 1
cEmpty           = 1
sourceNulBytes   = 0
max              = 313 B
packPayloadReads = 93
```

The `93` payload reads for `94` strings are expected: the unique zero-length payload needs no source payload I/O.

## Canonical string-content fingerprints

The first real hardware content fingerprint is now canonical:

```text
stringContentFNV = e995ee51
```

Raw payload FNVs for the four existing UI fixtures:

```text
string 1  / FORCE_MESSAGE = f6da01bb
string 25 / DIALOG        = 84f743cf
string 30 / DIALOGNOBACK  = 3692ac94
string 85 / NOTE          = ee639dc1
```

Together with the already-proven span fingerprint:

```text
stringSpanFNV    = 713188eb   structural index/span topology
stringContentFNV = e995ee51   actual payload bytes read from native pack
```

MAP_INTRO contains no embedded source NUL bytes in its 7779 payload bytes. The single C-empty result comes from the unique zero-length source span.

## Fail-closed proof

All deliberately invalid paths were refused on hardware:

```text
shortBuffer = 1
badRef      = 1
nullRef     = 1
closedPack  = 1
guards      = 94 / 94
```

Meaning:

```text
max-string buffer without terminator room -> BUFFER_TOO_SMALL
mutated sourceOffset in an otherwise real ref -> INVALID
NULL ref -> INVALID
valid non-empty ref after closing pack -> IO_ERROR
```

No failed operation escaped the caller buffer or mutated native/legacy gameplay state.

## Storage / RAM proof

Opening the native pack for the exhaustive sweep had a bounded transient heap cost:

```text
heap before open    = 68804 B
heap while open     = 64440 B
transient heap cost =  4364 B
largest while open  = 36852 B
elapsed             = 41 ms
persistentBytes     = 0 B
```

After close the stage recovered exactly:

```text
heap8        = 68804 -> 68804
largest8     = 36852 -> 36852
frameFNV     = 805df09e -> 805df09e
arenaFNV     = c3882516 -> c3882516
mapStateFNV  = cd99b98e -> cd99b98e
scriptFNV    = f9e3d9df -> f9e3d9df
notebookFNV  = 4d7705c5 -> 4d7705c5
```

The absolute heap8 moved from `68820` on the previous UI-intent build to `68804` on this firmware, while the framebuffer fingerprint changed from `b8b39f0f` to `805df09e`. Both are build-to-build differences: this reader stage itself has exact zero drift before/after, and the inherited arena/map/script fingerprints remain canonical.

Final PARK boundary:

```text
nativeArena           = yes
nativeTileState       = yes
nativeEventLookup     = yes
nativeEventDescriptor = yes
nativeScriptState     = yes
nativeFilter          = yes
nativeOpcodeExec      = yes
nativeUiIntent        = yes
nativeStringReader    = yes
persistentBytes       = 0
legacyUiMutation      = no
worldMutation         = no
framebufferMutation   = no
entities              = 0
monsters              = 0
noGameplay            = yes
```

Permanent invariants remain:

```text
shapeData            == NULL
mediaTexels          == NULL
legacy mapStringsIDs == NULL
runtime ZIP strings   = forbidden
```

## Post-PARK stability

Three later heartbeats from the same hardware-tested firmware stayed exactly stable:

```text
uptime=140113 ms heap=134568 heap8=68804 largest8=36852
uptime=145114 ms heap=134568 heap8=68804 largest8=36852
uptime=150115 ms heap=134568 heap8=68804 largest8=36852
```

SD, ZIP, VIDEO, CORE, LAYOUT, PRERENDER, RENDER, MAPPINGS and MENUBSP all remained `ready` on every heartbeat.

This closes the acceptance condition that the reader's transient PAK ownership is fully released and the parked system remains steady after the probe.

## Hardware acceptance status

The complete `[MAPTEXT]` / `[MAPTEXTPROBE]` one-shot probe and post-PARK steady-state are a **REAL-CYD HARDWARE PASS**.

This branch is **MERGE-READY**. All commits after the hardware-tested firmware SHA must remain documentation-only unless another flash is performed.

## Next boundary after merge

After merge, the next coherent milestone is one **small explicit native text/effect owner** consuming the already-separated UI intents and bounded reader.

Prefer a bounded owner with no world/entity/render mutation. Do not route the permanent architecture back through legacy `DoomCanvas`, `Hud`, `Player`, `mapStringsIDs[]` or runtime ZIP access.
