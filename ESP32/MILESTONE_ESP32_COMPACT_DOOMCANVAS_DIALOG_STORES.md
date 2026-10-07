# Milestone — Compact DoomCanvas dialog stores

Status: **REAL-CYD PASS**

Hardware-tested code boundary:
`2907e966f01c22cdecda048d7df0f2afdfef2549`

Branch:
`agent/esp32-doomcanvas-bridge`

## Goal

Remove the largest demonstrably dead storage family from the inherited
`DoomCanvas_t` compatibility object without changing the already
hardware-proven 15-export bridge ABI.

## Audit result

The exact Xtensa/32-bit layout at the starting boundary was 3740 bytes.
The two desktop dialog payload stores accounted for 2560 bytes:

```text
short dialogIndexes[1024]  2048 B
char  dialogBuffer[512]     512 B
                             ------
                             2560 B
```

They had no reader or writer in the permanent ESP32 bridge, ESP32-native
translation units, headers/inlines, or the remaining compiled compatibility
sources. The current dialog/event presentation path is ESP32-native.

## Implementation

For `DOOMRPG_ESP32`, the two fields are absent from `DoomCanvas_t`; the
desktop struct remains unchanged. This makes accidental future reuse fail at
compile time rather than silently restoring 2.5 KiB of permanent compatibility
storage.

`esp_legacy_doomcanvas_bridge.c` pins the compact layout with a
`_Static_assert` requiring exactly 1180 bytes and reports the retired byte
count at boot.

## CI

Normal `esp32-cyd` CI #1636: **SUCCESS**.

```text
RAM:   45464 B
Flash: 780161 B
esp32-cyd SUCCESS
```

The static RAM report is intentionally unchanged because `DoomCanvas_t` is a
heap object. The compile-time layout guard proves the new object size.

## Real-CYD acceptance

Boot confirms the exact structural reduction:

```text
Engine structs: Render=5040 Game=4 Canvas=1180 Total=6984 bytes
[DOOMCANVASBRIDGE] INIT exports=15 desktopTU=no bytes=1180 retiredDialogStores=2560 clip=160x120
[CORE] DoomCanvas     used=1196 heap=185652 largest=110580
```

The object allocation therefore drops by exactly 2560 bytes from the prior
3756-byte heap charge to 1196 bytes including allocator overhead.

Fresh START completes the full prologue/disposal/native-map handoff. The exact
first gameplay frame is preserved:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY map=1 angle=64 ... shapeData=0x0 mediaTexels=0x0
```

The same session exercises movement, rotation, native crate combat/transform,
resource pickups, a regular door animation, HUB/System and confirmed Exit To
Menu. Resident teardown remains exact:

```text
[RESIDENTRESET] ... released=18008 ... after=0/0/0/0/0/0/0 empty=1
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty saveWrite=no checkpoint=unchanged
```

The same firmware then loads an existing version-11 checkpoint, restores the
native world/session state including monster state/topology/position/activation
and monster drops, reaches resumed gameplay, accepts committed movement, and
exits cleanly again.

The known native renderer compact-span recovery path is observed and closes
normally through `LEGACY_GUARD -> RETRY -> RECOVERED`; this is an accepted,
unrelated renderer path.

## Invariants

- desktop `DoomCanvas.c` remains absent from the ESP32 firmware;
- the permanent DoomCanvas bridge remains the compatibility owner;
- `shapeData == NULL`;
- `mediaTexels == NULL`;
- native PAK backing remains authoritative;
- no runtime ZIP dependency returns;
- no dialog/world/gameplay ownership moves back into DoomCanvas.

## Closure

Hardware-tested code ends at
`2907e966f01c22cdecda048d7df0f2afdfef2549`.

This closure commit is documentation-only.
