# ESP32 — Render_relinkSprite legacy topology retirement

**Real CYD non-regression PASS**, 2026-10-08.
Hardware-tested code SHA: `33fe48d1a01f1146b7a36f746db1dca0ce2f31a4`.
Normal `esp32-cyd` CI 37773404749 succeeded, firmware published.

The legacy `Render_relinkSprite` follows and mutates the desktop
pointer-linked BSP nodes and sprite chains; production instead owns
immutable compact map topology and separate mutable overlays.
The original generated function is disabled in the normal firmware;
a permanent fail-closed compatibility ABI export is in
`esp_legacy_render_reject.c`, with desktop/bringup unchanged.

Real hardware boot: ESP32-D0WD-V3 without PSRAM, Render 1532 B,
Game 4 B, Canvas 44 B, 160x120 RGB565/38400 B shared framebuffer.
`MAPRT` arena FNV c3882516, first world frame FNV 71ca7465;
animated FORWARD and both two-frame TURN directions; crate transform,
Armor Shard pickup, dialog 88 resumed opcode 19, HUB close/reopen
and system double-confirm EXIT. Journal release 1036 B;
native resident reset 18008 B with empty=1, final menu exact
FNV 522dc605 and heap8 164184 B, largest8 110580 B.
shapeData and mediaTexels remain NULL. Checkpoint unchanged.

Important nuance: the HUB close log contains `exactHud=NO`
for combined HUD bands and `exactBottom=yes`; top bar is
recomposed with world redraw, so this log alone does not
demonstrate an unresolved visual regression.
No direct call of the fail-closed relink function was observed.
This is non-regression coverage only; LOAD and monster-sequence
behavior were not tested. Code SHA frozen, docs-only closure.
