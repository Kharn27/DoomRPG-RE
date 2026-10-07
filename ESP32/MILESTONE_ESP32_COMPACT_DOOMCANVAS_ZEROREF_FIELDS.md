# Milestone — Compact DoomCanvas zero-reference fields

Status: **REAL-CYD PASS**

Hardware-tested code boundary:
`596a62b667a2a41dd29789abc249068fca4265fa`

Branch:
`agent/esp32-doomcanvas-bridge`

## Goal

Remove only inherited `DoomCanvas_t` members proven to have zero accesses in
the normal ESP32 source/build closure, without changing behavior or widening the
compatibility bridge.

## Audit boundary

The starting ESP32 object was 1180 bytes after retirement of the desktop dialog
payload stores. A source-closure audit across the permanent DoomCanvas bridge,
all ESP32-native translation units, headers/inlines, and the remaining generated
compatibility TUs found 52 fields with no live source access.

Their declared payload totals 239 bytes. The compact layout also removes one
byte of alignment padding, giving an exact 240-byte object reduction:

```text
1180 B -> 940 B
```

The desktop definition remains unchanged. The ESP32 definition omits these
members entirely under `DOOMRPG_ESP32`, making accidental future reuse a
compile-time failure.

## Guard

`esp_legacy_doomcanvas_bridge.c` pins the compact layout with a
`_Static_assert` and reports both retired families at boot:

```text
retiredDialogStores=2560
retiredZeroRefLayout=240
```

## CI

Normal `esp32-cyd` CI #1640: **SUCCESS**.

```text
RAM:   45464 B
Flash: 780097 B
esp32-cyd SUCCESS
```

The static RAM report stays unchanged because DoomCanvas is heap allocated.

## Real-CYD acceptance

Boot proves the new object size and exact allocator charge:

```text
Engine structs: Render=5040 Game=4 Canvas=940 Total=6744 bytes
[DOOMCANVASBRIDGE] INIT exports=15 desktopTU=no bytes=940 retiredDialogStores=2560 retiredZeroRefLayout=240 clip=160x120
[CORE] DoomCanvas     used=956 heap=185892 largest=110580
```

The previous 1180-byte object consumed 1196 bytes including allocator overhead;
the new 940-byte object consumes 956 bytes. The real allocator delta is exactly
240 bytes.

Fresh START completes the full prologue/disposal/native-map handoff with the
canonical first-frame witness unchanged:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY map=1 angle=64 ... shapeData=0x0 mediaTexels=0x0
```

Fresh gameplay exercises movement, rotation, crate combat/transform, resource
pickups, a regular door animation, HUB/System and confirmed Exit To Menu.
Resident teardown remains exact.

The same firmware then loads an existing version-11 checkpoint and restores
resources, scripts, line state/texture, action removals, crate state, automap,
monster state, topology, positions, activation order and monster drops. Resumed
gameplay accepts movement and pickups, opens a door that activates a monster,
runs the ordered attack visualization, commits native retaliation
(`HP 34->32`, `armor 19->17`), and returns cleanly to the main menu.

The known renderer `LEGACY_GUARD -> RETRY -> RECOVERED` recovery path appears
and closes normally; it is an accepted unrelated boundary.

## Invariants

- desktop `DoomCanvas.c` remains absent from the ESP32 firmware;
- the DoomCanvas bridge remains the compatibility owner;
- `shapeData == NULL`;
- `mediaTexels == NULL`;
- PAK backing remains authoritative;
- no runtime ZIP path returns;
- no world/entity/render ownership moves back into DoomCanvas.

## Closure

Hardware-tested code ends at
`596a62b667a2a41dd29789abc249068fca4265fa`.

This closure commit is documentation-only.
