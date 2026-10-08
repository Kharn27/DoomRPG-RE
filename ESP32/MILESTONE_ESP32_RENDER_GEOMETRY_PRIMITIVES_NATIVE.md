# ESP32 milestone — Render geometry primitives native-owned

Status: **REAL CLASSIC CYD PASS** (2026-10-08).

## Proven boundary

Hardware-tested code SHA: `7352c965434797f2ac842f0a2f37a21b225b2b26`.
Base main SHA: `4147d602f1174e1de791665e2509d6f3e6fb260c`.
Normal esp32-cyd GitHub Actions run
[37770142955](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37770142955)
SUCCESS. Static RAM 45056 B, linked flash 775081 B (previous main 774897 B,
+184 B), firmware.bin uploaded and physically booted.

Seven active Render ABI primitives have a permanent owner in
`ESP32/src/esp_render_geometry_primitives.c`:
`Render_initColumnScale`, `Render_cullBoundingBox`,
`Render_transform2DVerts`, `Render_clipLine`,
`Render_clipVertex`, `Render_projectVertex`,
`Render_occludeClippedLine`.
The normal generated Render.c excludes the originals; desktop and
bringup reference implementations remain retained. The generator
pins legacy function bodies with exact CRC32/source guards and checks
the native export census. No PAK or SD access, new allocation, runtime
owner or shape/texel mirror was introduced.

## Real hardware evidence

The user supplied the actual serial trace from ESP32-D0WD-V3,
240 MHz, 4 MB flash, 0 PSRAM. Render_t 1532 B, Game_t 4 B,
DoomCanvas_t 44 B. Shared 160x120 RGB565 framebuffer 38400 B,
palettes 3280 entries / 6560 B, mappings payload 8376 B.
MENU_MAIN FNV 522dc605, intro native arena FNV c3882516,
first gameplay frame FNV 71ca7465 (8 walls, 4430 pixels),
shapeData=0x0, mediaTexels=0x0.

Fresh gameplay demonstrated committed FORWARD movement with one
`[VIEWANIM] FRAME mode=move profile=axial-midpoint`, both left/right
TURN with two intermediate frames, STRAFE_RIGHT with two
`profile=strafe-thirds` frames, a subtype-2 crate transform,
Armor Shard pickups increasing armor 0->4->8->12,
dialog event 88 with opcode 19 state mutation and closed/resumed
script, and a regular door line 275 with 4-frame animation.
No VIEWANIM FALLBACK in this fresh-session sample.

One renderer packed-wall boundary guard occurred:
`[NATIVEFRAME] LEGACY_GUARD`, then `RETRY` and `RECOVERED`.
This is an existing bounded recovery, not proof of a regression.
After lazy dialog allocation the trace reports
heap8=117252 and largest8=86004, remaining unchanged at the
last ALIVE; the memory reserve gate reported HEADROOM_OK.

## Limits and next steps

This sample did **not** exercise checkpoint LOAD, SYS EXIT /
Render_free destructor, or active monster attack sequencing.
The existing post-LOAD movement-preview fallback and active-monster
sequencing caveat remain separate and unresolved; do not claim
those paths pass based on the fresh-session witness.
No hardware evidence of display artifacts was provided beyond
the serial trace; the non-regression verdict concerns the observed
runtime behavior. The legacy generated Render.c is still needed
for other surviving compatibility functions.

This milestone's tested code boundary is frozen. Documentation
changes after user hardware PASS are docs-only. Any subsequent
Render disengagement on this same branch is a **new independent
code boundary** requiring its own CI + real-CYD confirmation.
No merge to main performed.
