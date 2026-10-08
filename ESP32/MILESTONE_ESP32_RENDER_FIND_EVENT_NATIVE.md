# ESP32 milestone — Render_findEventIndex retired legacy lookup

**Status: real-CYD non-regression PASS (2026-10-08).**

Tested code SHA `8672b1ed4fc9b0379a63851d17726b5bc11bfadf`;
normal CI run 37771382195 SUCCESS. The original desktop ABI
binary-searched `Render.tileEvents`, which production native maps
do not populate. Its generated implementation is excluded in
production and retained for desktop; permanent native compatibility
export fail-closes to -1, without reading the legacy array.
The actual native event lookup and script engine are unchanged.

User-supplied real CYD serial: 160x120 RGB565 38400 B;
Render 1532 B, Game 4 B, Canvas 44 B; initial menu FNV
522dc605; native MAP_INTRO arena c3882516 and first
render frame 71ca7465. Native FORWARD previews and two-frame
TURN_RIGHT/LEFT, crate transform, Armor Shard pickup,
dialog event 88 with opcode 19 state mutation, HUB and SYS
double-confirm EXIT all succeed. Dialog journal release 1036 B,
resident release 18008 B, empty=1, menu FNV 522dc605,
heap8 164184 B, largest8 110580 B, saveWrite=no.
Both shapeData and mediaTexels observed NULL.

**Proof limit:** old Render_findEventIndex was not exercised
directly; this is real-hardware non-regression for the relocated
symbol and functioning native event pipeline, not a positive
runtime witness of invoking the old API. Save LOAD and monster
resume caveats remain open. This is documentation-only closure
after hardware PASS; code SHA frozen.
