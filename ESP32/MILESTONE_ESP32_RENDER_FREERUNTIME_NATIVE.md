# ESP32 milestone — native Render_freeRuntime ownership

Status: **REAL-CYD NON-REGRESSION PASS; direct destructor execution NOT observed**.

Hardware-tested code SHA `75cefcf402b6bacd8fbd768450cf4b70e8f3e8cd`.
CI run [37770825723](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37770825723):
normal esp32-cyd firmware build/artifact SUCCESS.

The permanent ESP32 `render_startup_bridge.c` owns
`Render_freeRuntime` directly. The generated desktop `Render.c`
definition is disabled for normal ESP32 builds but retained for
desktop/reference; a generator source-equivalence check guards the
body. The port replicates all pointer frees, resets and
`numMapSprites`, `linesLength`, `mapStringCount` handling.
Native arenas, overlays, packed SD cache and framebuffer keep their
existing independent ownership; this helper cleans compatibility
pointer fields rather than those native owners.

## Real CYD witness — 2026-10-08

Normal boot: ESP32-D0WD-V3, no PSRAM, `Render=1532`,
`Game=4`, `Canvas=44`, framebuffer 38400 B.
Initial and returned MENU_MAIN FNV `522dc605`.
Native intro arena `c3882516`, first gameplay frame `71ca7465`.
Fresh FORWARD midpoint and LEFT/RIGHT two-frame rotation previews.
Crate subtype-2 transformed into Armor Shard; ammo 8->7,
armor 0->4. Dialog event 88 opened, advanced and resumed
opcode 19 script mutation. SYS EXIT confirmed after HUB.
`[DIALOGCHAIN] OWNER-RELEASE` recovered 1036 B.
`[RESIDENTRESET]` released 18008 B, `empty=1`.
`[SYSEXIT] MENU-READY frame=522dc605 session=off
resident=empty saveWrite=no checkpoint=unchanged`.
Final ALIVE heap8=164184 B, largest8=110580 B.
`shapeData` and `mediaTexels` remained NULL at observed
critical boundaries.

## Proof boundary

There was **no** `[RENDERFREE] ENTRY`; SYS EXIT does not call
the full Render destructor, so this is a genuine hardware
**non-regression** PASS and *not* direct runtime coverage of
`Render_freeRuntime`. The exact cleanup equivalence is source
review / CI-verified only. LOAD/resume and monster sequencing
issues remain outside scope. Tested code frozen; docs-only closure.
Any next code changes on same branch require an independent test.
