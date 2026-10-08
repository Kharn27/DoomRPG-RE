# ESP32 Render P1 review fixes — production hardware validation

**Production normal esp32-cyd: REAL-CYD non-regression PASS (2026-10-08).**

Frozen tested code SHA: `cccd26d1e3560d1948db3004805d4c8d1a8317a2`. Normal firmware CI run 37775947093 succeeded.

Two blocking code review corrections applied:
1. `ESP32/src/esp_render_geometry_primitives.c` now defines `FIXED_VERSION=1` in that translation unit, matching the source-local legacy definition and selecting equivalent fixed-point branches.
2. The generator retains `Render_renderBSPNoclip` and `Render_relinkSprite` desktop implementations for `DOOMRPG_ESP32_BRINGUP_PROBES`, while production excludes them and exposes native fail-closed compatibility exports.

Physical classic CYD test: 160x120 shared RGB565 frame 38400 B, `MAPRT` FNV c3882516, initial gameplay FNV 71ca7465; cache rendering FNVs newly a9b263f5 (cold) and 20c09fe4 (warm), versus prior d4151456 and efb3a31b. Different FNVs are expected given the fixed-point correction; no general pixel-perfect equivalence is claimed. Native MOVE, TURN, crate transform, pickup, dialog 88 -> opcode 19, HUB and SYS EXIT work. After exit native resident cleanup recovers 18008 B (empty=1), dialogue journal 1036 B; final menu 522dc605, heap8 164184 B, largest8 110580 B, checkpoint untouched, shapeData/mediaTexels NULL.

**Important limitation:** the bringup profile has not yet been compiled on CI and remains unverified. Do not treat bringup guard P1 as fully closed until that environment builds. LOAD and active monster coverage were not exercised. Post-hardware updates only documentation, tested code unchanged.
