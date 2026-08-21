# ESP32 MAP_INTRO bounded native string reader milestone

Branch: `agent/esp32-map1-native-string-reader`

Base merged `main`:

```text
PR   = #50 — allocation-free native string spans + UI intents
main = 9a5e8ade361180d220f2b3614a443e5efb0d27bd
```

Firmware candidate content:

```text
d13d5eb13c4657d5ec5c16fd82939cfc38989c86
```

Status: **IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING**.

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

`ESP32/include/esp_map_strings.h` now exposes:

```c
EspMapStringReadStatus EspMapStrings_read(
    const EspAssetPackEntry* sourceEntry,
    const EspMapStringRef* ref,
    char* destination,
    size_t capacity,
    size_t* outLength);
```

The reader is intentionally caller-owned and allocation-free. It keeps no global text cache and allocates no persistent buffer.

Success requires:

```text
source entry size == current EspMapRuntime sourceBytes
source entry CRC  == current EspMapRuntime sourceCrc32
ref                == freshly resolved canonical ref for ref.index
capacity           >= ref.length + 1
range              inside source entry
native pack         open for non-empty payload I/O
```

On success, exactly `ref.length` source bytes are read and `destination[ref.length]` is set to `\0` by the reader. The terminator is not read from the BSP and is not included in the source payload length.

Status values are deliberately fail-closed:

```text
INVALID
BUFFER_TOO_SMALL
IO_ERROR
OK
```

A zero-length payload requires no SD payload read but still receives the synthesized terminator after the same source/ref validation.

## Temporary MAP_INTRO hardware probe

`native_map1_string_reader_probe.c` runs only after the hardware-proven UI-intent probe.

It opens the native pack once, resolves the real `/intro.bsp` entry and requires the inherited source identity:

```text
source bytes = 21823
CRC32        = 623f34e4
strings      = 94
payload      = 7779 B
max payload  = 313 B
spanFNV      = 713188eb
```

For every one of the 94 strings the probe:

1. resolves the canonical `EspMapStringRef`;
2. reads the preceding on-disk little-endian length prefix and checks it against `ref.length`;
3. calls `EspMapStrings_read()` into a caller-owned `314 B` text capacity (`313 B + terminator`);
4. checks two guard bytes surrounding that buffer;
5. checks the synthesized terminator and returned length;
6. recomputes the already hardware-proven span fingerprint;
7. feeds the actual payload bytes into a new deterministic content fingerprint.

The content fingerprint is intentionally **not predeclared** before the first hardware run. The exact Serial result becomes canonical only after the real CYD proves the source identity, all 94 length prefixes, all 94 bounded reads and the no-mutation boundary.

The probe also reports raw payload FNVs for the four existing UI fixtures:

```text
string 1   — FORCE_MESSAGE sample
string 25  — DIALOG sample
string 30  — DIALOGNOBACK sample
string 85  — NOTE sample
```

## Fail-closed checks

Before PARK the probe must prove:

```text
max-string buffer without terminator room -> BUFFER_TOO_SMALL
mutated sourceOffset in an otherwise real ref -> INVALID
NULL ref -> INVALID
valid non-empty ref after closing pack -> IO_ERROR
```

No failed operation may escape the caller buffer or mutate native/legacy gameplay state.

## Expected memory/effect boundary

Permanent cost by construction:

```text
EspMapStrings_read state = 0 B persistent
caller buffer            = <=314 B transient stack/caller ownership for MAP_INTRO
native pack handle       = transient existing EspAssetPack ownership
```

The probe records the temporary heap cost while the PAK is open, then requires exact recovery after close.

The following must be unchanged before/after the full 94-string sweep:

```text
heap8
largest8
framebuffer FNV
arenaFNV
mapStateFNV
scriptFNV
Player.NotebookString FNV
Hud.statBarMessage
Game continuation fields
entities / monsters
```

The final boundary must still have:

```text
shapeData   == NULL
mediaTexels == NULL
legacy mapStringsIDs == NULL
legacy UI mutation    = no
world mutation        = no
framebuffer mutation  = no
entities              = 0
monsters              = 0
ST_PLAYING             = no
```

## Hardware acceptance

Use the normal optimized PlatformIO environment:

```text
esp32-cyd
```

A real-CYD PASS requires `[MAPTEXT] READY`, all four fail-closed checks, exact RAM/state/frame preservation and `[MAPTEXTPROBE] PARK ... nativeStringReader=yes` with no later regression in `[ALIVE]`.

Until those Serial logs are supplied, this milestone remains **hardware pending** and is not merge-ready.

## Next boundary after a hardware PASS

Only after the bounded reader is proven should one small native text/effect owner consume the existing UI intents. Keep presentation/state ownership explicit and do not route the permanent architecture back through legacy `DoomCanvas`, `Hud`, `Player` or `mapStringsIDs[]`.
