# Milestone — DoomCanvas linked-ABI whitelist

Status: **REAL-CYD PASS**

Hardware-tested code boundary:
`d34cf007663bbb213f5c4e3f3eea7ae694339217`

Branch:
`agent/esp32-compact-game-entity-storage`

## Goal

Prove the actual ESP32 dependency closure of the inherited desktop
`DoomCanvas.c` translation unit before replacing that generated desktop TU
with a permanent ESP32-native compatibility bridge.

## Source audit

The desktop `src/DoomCanvas.c` contains 76 public `DoomCanvas_*` function
definitions.

The previously accepted ESP32 ELF retained only the following 15 source
functions:

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

GCC additionally emits `DoomCanvas_drawFont$part$0`, so the ELF exposes 16
DoomCanvas binary symbols.

## Implementation

The ESP32 generator now fail-closed prunes the other **61 public desktop
functions before compilation**. This is intentionally stronger than relying on
link-time garbage collection: dead desktop gameplay/menu/state-machine
definitions no longer even enter the ESP32 compiler.

The retained set compiles and links without restoring any retired Game fields
or desktop world ownership.

## CI

Normal `esp32-cyd` CI for the tested code boundary: **SUCCESS**.

```text
[ESP32] DoomCanvas generated ... 61 dead public function(s) pruned + 15 source ABI root(s) retained ...
RAM:   45464 B
Flash: 780193 B
```

An ELF audit after the pruning shows the same linked 16-symbol DoomCanvas
binary surface as before the change.

## Real-CYD acceptance

The hardware run validates the retained closure through both fresh and restored
sessions.

Fresh path:

- main menu -> START;
- full intro;
- native MAP_INTRO load;
- exact first frame `71ca7465`;
- movement/rotation/collision;
- crate transform and multiple pickups;
- door animation;
- enter-dialog opcode 26;
- dialog continuation through opcode 19;
- standalone weapon-help dialog;
- monster activation and a full visualized/committed retaliation;
- HUB -> System -> EXIT.

Teardown remains exact:

```text
[RESIDENTRESET] ... released=18008 ... empty=1
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty ...
```

The same firmware then loads a version-11 checkpoint, restores all current
native world owners, resumes with the 12-member active monster order, and
continues normal movement, pickups, door activation and monster combat.

Critical invariants:

- `shapeData == NULL`;
- `mediaTexels == NULL`;
- PAK-backed runtime only;
- no runtime ZIP;
- no restored desktop Game/Entity/Monster/Player/Combat ownership.

The existing compact renderer guard is observed during both fresh and resumed
play and recovers normally.

## Conclusion

The 61 pruned desktop functions are proven unnecessary for the current ESP32
runtime. The remaining DoomCanvas dependency surface is now small, explicit and
hardware-validated.

This creates the next architectural boundary: replace the generated desktop
`DoomCanvas.c` TU with an explicit ESP32-owned bridge containing only the
hardware-proven retained ABI, then audit `DoomCanvas_t` itself for structural
compaction.

## Closure

Hardware-tested code ends at
`d34cf007663bbb213f5c4e3f3eea7ae694339217`.

This closure commit is documentation-only.
