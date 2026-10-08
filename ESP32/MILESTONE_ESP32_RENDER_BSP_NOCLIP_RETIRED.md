# Render_renderBSPNoclip — real-CYD non-regression PASS

Tested code SHA: `d382610ec593640928e5f882e3e1b66ad0755704`, CI 37773098498 SUCCESS.
This legacy rendering bypass directly iterates Render.lines and
Render.mapSprites, both retired production arrays. The generated body
is excluded for the normal ESP32 firmware, and the permanent
esp_legacy_render_reject.c export fails closed. Desktop unchanged.

User's CYD serial confirms core/layout/startup, framebuffer 38400 B,
MENU_MAIN FNV 522dc605, native MAP_INTRO arena c3882516, first
world FNV 71ca7465, MOVE/TURN animations, crate transform/pickup,
dialog event 88 and opcode 19, HUB/SYS EXIT, 1036 B dialogue
journal release, 18008 B resident release empty=1, final heap8
164184 B largest8 110580 B, no checkpoint write.
shapeData/mediaTexels remain NULL.

No direct execution of retired bypass observed; this is
non-regression hardware PASS, not a test of fail-closed execution.
SAVE LOAD and active monster resume remain separate. Tested code frozen.
