# Milestone — Retire desktop DoomCanvas translation unit

Status: **REAL-CYD PASS**

Hardware-tested code boundary:
`50dd03cdf9bf8a5531f6e4d9261a7e2fcce8709b`

Branch:
`agent/esp32-doomcanvas-bridge`

## Goal

Replace the generated/patched desktop `DoomCanvas.c` translation unit with a
permanent ESP32-owned compatibility bridge whose ABI is limited to the exact
15 source functions proven by the preceding linked-root milestone.

This milestone changes ownership, not gameplay architecture: native gameplay,
menus, dialogs, map loading, rendering composition and world mutation remain
owned by the existing ESP32-native subsystems.

## Starting boundary

The preceding accepted whitelist milestone proved that 61 of the desktop
`DoomCanvas_*` public functions were dead on ESP32 and that only these 15 source
roots survived:

```text
DoomCanvas_free
DoomCanvas_getOverall
DoomCanvas_drawImageSpecial
DoomCanvas_drawSoftKeys
DoomCanvas_drawString1
DoomCanvas_drawFont
DoomCanvas_initCredits
DoomCanvas_loadEpilogueText
DoomCanvas_loadPrologueText
DoomCanvas_renderScene
DoomCanvas_setAnimFrames
DoomCanvas_setState
DoomCanvas_startup
DoomCanvas_init
DoomCanvas_invalidateRectAndUpdateView
```

That milestone still compiled a generated copy of desktop `DoomCanvas.c`.
This milestone removes that translation unit completely.

## Implementation

`ESP32/src/esp_legacy_doomcanvas_bridge.c` now owns the retained compatibility
surface.

The bridge keeps only the responsibilities still required by current ESP32
callers:

- construct/free the inherited compatibility object;
- configure the 160x120 / 160x80 native layout;
- load/free the small font, cursor and intro compatibility assets;
- provide the existing bitmap-font/image helpers still used by native menu/UI
  code;
- preserve the narrow state transition hooks still called by native START/LOAD
  integration;
- retain dormant linked ABI helpers without reviving desktop gameplay/world
  ownership.

It deliberately does not own the desktop event loop, map loader, gameplay input,
entity runtime, Player/Hud/Combat ownership or any legacy map-wide media state.

`ESP32/scripts/build_engine.py` no longer creates a patched
`doomrpg_patched_sources/DoomCanvas.c` and no longer selects it in
`doomrpg_engine_patched`.

The generator validates that all 15 explicit bridge exports exist and fails if
`DoomCanvas_run()` or `DoomCanvas_loadMap()` appears in the bridge.

## CI and ELF audit

Normal `esp32-cyd` CI #1632 for the tested code boundary: **SUCCESS**.

```text
[ESP32] Desktop DoomCanvas.c retired; esp_legacy_doomcanvas_bridge.c owns 15 source ABI exports
RAM:   45464 B
Flash: 780389 B
esp32-cyd SUCCESS
```

The CI artifact ELF contains exactly the intended 15 `DoomCanvas_*` symbols.
Its source/DWARF records contain `esp_legacy_doomcanvas_bridge.c` and no
`DoomCanvas.c`, providing a binary-level witness that the desktop translation
unit is absent from the firmware.

Static RAM remains unchanged at 45464 B. The bridge milestone is architectural;
the current `DoomCanvas_t` object itself is intentionally unchanged at 3740 B.

## Real-CYD acceptance

Hardware boot proves the permanent bridge is the live constructor/layout owner:

```text
Engine structs: Render=5040 Game=4 Canvas=3740 Total=9544 bytes
[DOOMCANVASBRIDGE] INIT exports=15 desktopTU=no bytes=3740 clip=160x120
[CORE] DoomCanvas     used=3756 heap=183092 largest=110580
...
[DOOMCANVASBRIDGE] STARTUP desktopTU=no display=160x120 screen=160x80@0,20 startupMap=1 hud=native
[LAYOUT] READY real engine layout fits inside 160x120
```

The remaining startup layers are unchanged: native menu storage, Render startup,
config bridge and compact mappings all reach READY.

Fresh START then exercises the bridge through the full intro route. Intro
allocation, fitted frames, semantic input, disposal and the native loading
handoff all complete without regression. MAP_INTRO remains
`/intro.bsp` / Entrance, and the native first frame is byte-for-byte identical:

```text
[NATIVEBOOT] RESIDENT map=1 file=/intro.bsp source=21823 crc=623f34e4 ...
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY map=1 angle=64 ... shapeData=0x0 mediaTexels=0x0
```

The fresh session stabilizes at:

```text
[ALIVE] ... heap=165632 heap8=99708 largest8=86004 ...
```

The run continues through native action feedback, committed movement and a crate
attack/transform, then HUB/System -> confirmed Exit To Menu. Teardown proves the
resident owners are released exactly:

```text
[RESIDENTRESET] heap8=127596->145604 released=18008 before=1/1/1/1/1/1/1 after=0/0/0/0/0/0/0 empty=1
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty saveWrite=no checkpoint=unchanged
```

The same firmware immediately performs main-menu LOAD of a version-11
checkpoint. Resources, script, lines/textures, removals, crates, automap,
monster state/topology/positions/activation and monster drops restore exactly.
The resumed session becomes visible and accepts committed movement:

```text
[NATIVESAVE] LOAD ... version=11 ... monster-drops-restored-exact session=reprime-pending
[ENGINESESSION] RESUME checkpoint=restored ...
[ENGINESESSION] READY map=1 angle=0 ... shapeData=0x0 mediaTexels=0x0
[RESIDENTGAMEPLAY] MOVE ... committed=yes
```

The resumed session also settles at `heap8=99708/largest8=86004`, matching the
fresh session in this transcript.

A known native renderer compact-span guard is exercised during checkpoint resume:

```text
[NATIVEFRAME] LEGACY_GUARD ...
[NATIVEFRAME] RETRY ...
[NATIVEFRAME] RECOVERED ...
```

Recovery completes before gameplay continues; this is not a regression of the
DoomCanvas bridge.

## Scope limits

The hardware transcript exercises the production boot/layout, START/intro,
native map handoff, resident gameplay, Exit To Menu, LOAD/restore and resumed
gameplay paths.

Dormant compatibility branches such as epilogue/credits are retained because
the accepted linked-ABI boundary still contains them, but they were not newly
hardware-exercised in this run. They remain separate future migration targets;
this milestone does not claim new behavioral validation for those dormant
branches.

## Architectural result

The ESP32 firmware can no longer silently fall back to desktop DoomCanvas
state-machine code: the desktop translation unit is absent from the build and
the compatibility ABI is explicit.

The next coherent consolidation boundary is therefore the **3740-byte
`DoomCanvas_t` object itself**. Its fields should now be audited against the
15-function bridge plus direct native field users, then compacted only where
ownership is already demonstrably native.

## Closure

Hardware-tested code ends at
`50dd03cdf9bf8a5531f6e4d9261a7e2fcce8709b`.

This closure commit is documentation-only.
