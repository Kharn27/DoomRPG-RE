# ESP32 — retire dead Menu / ParticleSystem translation units

Status: **REAL-CYD PASS — 2026-10-02**

Base main:
`2d981fd14e3b7840ccf71575c47b0e3bd6d123fa`

Branch:
`agent/esp32-retire-dead-menu-particle-tus`

Hardware-tested code boundary:
`bc65cc337000d8c7ef54b5f0451e51d958cf7cef`

## Scope

This is a compile-graph retirement milestone only.

Earlier hardware milestones had already removed the legacy `Menu_t` and
`ParticleSystem_t` runtime owners from the ESP32 core graph. Direct final-ELF audits
also showed zero surviving `Menu_*` and `ParticleSystem_*` symbols.

However, `ESP32/scripts/build_engine.py` still built the desktop source directory with
a broad `+<*.c>` rule, so `src/Menu.c` and `src/ParticleSystem.c` were still
compiled into object files before linker garbage collection discarded them.

The permanent change is intentionally small:

```text
desktop src/*.c
 -> exclude Menu.c
 -> exclude ParticleSystem.c
 -> compile remaining currently-required legacy units
```

No type or field is removed in this milestone. No compatibility ABI is widened.

## CI proof

Normal `esp32-cyd` CI #1298 is SUCCESS.

```text
static RAM   = 44992 B
linked Flash = 769357 B
artifact id  = 11201100436
digest       = sha256:3626c14a436d18f6dd386dfa2554d9a7c36ec0f54eb52bb35fd6339b8daf82e0
```

The compile log no longer contains:

```text
Menu.c.o
ParticleSystem.c.o
```

The final image size is unchanged from the preceding hardware-tested main. That is
the expected result because the two translation units were already completely dead
at final link.

## Real-CYD proof

The real classic CYD was exercised beyond boot:

```text
MAIN
 -> Load Game
 -> V9 checkpoint valid
 -> Sector 1 BSP/runtime rebuild
 -> checkpoint owner restore
 -> resident gameplay READY
 -> player MOVE
 -> ordered active-monster sequence
 -> subtype-4 three-goal continuation
 -> three-loop attack visual
 -> retaliation COMMIT
```

Critical runtime invariants remain:

```text
shapeData=0x0
mediaTexels=0x0
heap=116232
heap8=50308
largest8=38900
```

The main-menu hardware sample before LOAD is stable at
`heap=122840 heap8=56916 largest8=32756`.

## Architectural result

These files are now absent from the ESP32 compile graph, not merely absent from the
final ELF:

```text
src/Menu.c
src/ParticleSystem.c
```

Their legacy headers/types may still be transitively visible to currently-retained
desktop-compatible structures. Removing those type/layout seams is a later ownership
milestone and must be done only when their actual consumers are retired.

## Next structural question

The next likely ownership boundary is `MenuSystem_t`, whose retained final symbols
were previously narrowed to:

```text
MenuSystem_init
MenuSystem_startup
MenuSystem_playSound
MenuSystem_free
```

Before changing it, the next branch must re-audit the merged main and separate:

- resource/menu-model storage still consumed by native UI;
- audio-semantic calls that should become a tiny native audio intent;
- fields kept only for compatibility layout;
- truly live runtime behavior.

A separate known presentation gap exists in player MOVE/TURN: the current native
camera commits directly from one settled pose to the next with no visible
interpolation. That should be addressed as a native camera/presentation milestone,
not by restoring desktop gameplay/render ownership.


## Follow-up closure on the same bounded retirement branch — REAL-CYD PASS

Hardware-tested code boundary:
`4c4a48230303cef7eeedc9722198fbe2c7506517`.

After the original dead Menu/ParticleSystem TU retirement, the same branch
closed two directly adjacent legacy menu helper surfaces without reintroducing
desktop ownership:

- `MenuSystem.c` retired from the ESP32 compile graph; final `MenuSystem_* = 0`;
- retained ESP32 menu storage compacted from 96 desktop item slots to 8, with
  HELP stored as compact raw text plus bounded offsets;
- `MenuItem.c` retired after the only remaining setters were absorbed by the
  native fixed menu builders; final `MenuItem_* = 0`.

The final branch hardware witness exercises MAIN, OPTIONS/Back and HELP
multi-page navigation repeatedly with exact framebuffer hashes and stable heap.
CI #1341 reports 45160 B static RAM and 769613 B linked Flash.

This closes the easy dead-helper retirement frontier. Image ownership still
referenced by linked DoomCanvas Story/Epilogue/scrollbar paths and full
`MenuSystem_t` root replacement are intentionally deferred to later milestones.

## Follow-up closure: MenuSystem shell retirement frontier

The later branch `agent/esp32-retire-menu-system-shell`, based from merged main
`643701bbf26461fb328e03a20302d37598b9e6c9`, extends this retirement work and
closes the easy compatibility-shell frontier.

Hardware-tested code boundary:
`d8d0622eb92d7f32b2e37033cb557b6ba35deb5e`.

Final real-CYD result:

```text
MenuSystem_t ESP32 sizeof = 496 B
CORE MenuSystem allocation = 512 B
MAIN heap8 = 62112
first ST_INTRO draw deltaHeap = 0
story hand release = +176 B
intro disposal = 69492 -> 103436 B
ENGINESESSION READY
MOVE committed
HUB opened
shapeData = NULL
mediaTexels = NULL
```

The shell no longer contains the desktop-only accessory images, bookkeeping,
binding/message residue, the DoomRPG backpointer, or the write-only paint flag.
The remaining fields are actively used by the native MAIN/OPTIONS/HELP model and
logo ownership. Further reduction therefore belongs to a dedicated replacement
of the `MenuSystem_t` root/type, not to this dead-field cleanup milestone.

Normal `esp32-cyd` CI #1413 is SUCCESS: RAM 45176 B, Flash 770189 B,
artifact 11221787233, digest
`sha256:aabcf3c22267d1b0866f4b642eee70fa2d81b14236cfed1a7b891bfed4dc0603`.
