# Render plane *_Test legacy functions — real-CYD non-regression PASS

Date: 2026-10-08. Code SHA: `2be8b0b47520d167724559d3c647a44620c0e1b9`.
CI: [37772211049](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37772211049), SUCCESS.

Normal ESP32 production excludes the three desktop-only planebuffer
test routines from generated Render.c and provides fail-closed
compatibility exports in esp_legacy_render_reject.c:
`Render_renderFloorAndCeilingBG_Test`,
`Render_drawPlane_Test`, `Render_spanPlane_Test`.
These routines depend on legacy mediaTexels / mediaTexelOffsets;
native PAK-backed renderer is the only runtime owner.

Physical ESP32-D0WD-V3 / 0 PSRAM: long idle menu with stable
heap8=159340, native map arena FNV=c3882516, first world
frame FNV=71ca7465, native MOVE and TURN previews, crate attack,
Armor Shard pickup, dialog event 88 and opcode 19 resume.
Double-confirm SYS EXIT reclaimed resident 18008 B and dialog
journal 1036 B, produced exact menu FNV=522dc605, heap8=164184,
largest8=110580, saveWrite=no, checkpoint unchanged.
shapeData/mediaTexels NULL throughout observed stages.

No old plane test call was triggered: hardware PASS only for
non-regression. No SAVE LOAD or monster behavior coverage here.
Hardware-tested code frozen; later documentation only.
