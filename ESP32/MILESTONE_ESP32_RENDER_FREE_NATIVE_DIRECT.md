# Milestone — native direct Render_free, framebuffer guard preserved

Status: **REAL-CYD NON-REGRESSION PASS; destructor execution NOT YET PROVEN**

Hardware-tested code SHA: `77f1de060142b869bbc83727ad17ae137b21789c`; GitHub Actions normal
`esp32-cyd` CI #37761071831 SUCCESS (static RAM 45056 B,
flash 774897 B).

## Actual CYD hardware results (2026-10-08)

Real production build booted without regression, with
`[RENDERCORE] INIT bytes=1532`, `SETUP ... arrays=1280B`,
`Render_startup direct=yes wrap=no`, shared RGB565
framebuffer 38400 B, 3280 palette entries, 8376 B mappings.
MENU_MAIN frame 522dc605, compact intro arena c3882516
and first world frame 71ca7465 matched previous samples.
Two successful FORWARD moves, two camera-interpolated rotations,
crate attack/transform, Armor Shard pickup, event 88 DIALOG
with opcode 19 resumed mutation, HUB and SYS EXIT succeeded.
`[DIALOGCHAIN] OWNER-RELEASE ... recovered=1036`,
`[RESIDENTRESET] ... released=18008 ... empty=1`,
`[SYSEXIT] MENU-READY ... session=off resident=empty`
and final heap8=164184, largest8=110580 confirmed.
The permanent null shapeData/mediaTexels invariants survived.

**Destructor caveat:** no `[RENDERFREE] ENTRY` line was logged.
The native SYS EXIT path is not an engine-object destructor.
Therefore linkage is CI-validated and normal firmware behavior is
CYD-validated, but direct `Render_free` execution, including
shared framebuffer detachment, remains **unvalidated on device**.
Do not promote that specific path to hardware PASS until a separate
safe bounded destructor probe passes. The inherited
`Render_freeRuntime` remains, and must not be retired blindly.
The next code milestone remains on this same active branch.


Branch: `agent/esp32-retire-desktop-source-registration`, no new branch and no merge.
Previous `Render_startup` code SHA `dd577ad94f66ae642cc1ecaca7dc331b68c5b8e5`
passed real CYD; docs-only closure `0a4cfddc846459fd54e9cc976d421798507651b9`.

## Implementation

The original `Render_free` was still compiled in generated
desktop `Render.c` and intercepted at link time by
`-Wl,--wrap=Render_free`, so a wrapper in
`ESP32/src/render_startup_bridge.c` first detached
`PlatformVideo`'s shared 160x120/38400 B framebuffer, then called
`__real_Render_free`. The resulting native `Render_free`
permanently owns exactly that same sequence, and directly calls
the still-linked `Render_freeRuntime` legacy cleanup.

- Remove the production wrap flag, old `__real_Render_free`
  external declaration and `__wrap_Render_free` implementation.
- Exclude only the generated desktop `Render_free` function under
  `DOOMRPG_ESP32`; retain original desktop `src/Render.c` unchanged.
- Pin original destructor body to CRC32 `0x5c51ec08` plus strict
  source anchor/census, fail closed on drift.
- Copy the original `Render_free` resource cleanup semantics
  into native source. **Detach the framebuffer only if its pointer
  equals `Esp32PlatformVideo_framebuffer()`**; then call
  `Render_freeRuntime`, free viewport arrays and palette,
  free only non-PlatformVideo framebuffer buffers, release
  `piDIB` if present, honor `freePtr` exactly as before.
- The remaining `Render_freeRuntime` implementation is still
  inherited and linked from desktop Render.c. This milestone
  does **not** retire that root, its map-wide legacy cleanup
  fields, or the generated Render.c TU.
- Do not change any native map owner, PAK cache, game logic,
  GPU presentation, allocation size, or public Render ABI.

Build marker:
`[ESP32] Desktop Render_free unlinked; owner=esp-native-render-startup direct=yes linkerWrap=no sharedFB=PlatformVideo guarded=yes`.

On actual call (not necessarily exercised by ordinary SYS EXIT):
`[RENDERFREE] ENTRY owner=esp-native-render-startup direct=yes wrap=no sharedFB=1 freePtr=...`.
**Do not claim ordinary SYS EXIT executed this destructor unless that
line appears**. Native SYS EXIT calls resident owner reset, not necessarily
the engine-level `Render_free`.

## Acceptance

First normal `esp32-cyd` CI with the exact code SHA must succeed.
Then real classic CYD normal firmware should demonstrate unchanged:
- Boot and shared framebuffer 38400 B, `RENDERCORE INIT bytes=1532`,
  `SETUP view=160x80 arrays=1280B`, palette entries=3280
  and mappings=8376 B.
- MENU_MAIN FNV 522dc605, MAP_INTRO arena FNV c3882516,
  first world frame FNV 71ca7465, `shapeData=mediaTexels=NULL`.
- Basic TURN and MOVE, native pickup/selection or dialog, and
  HUB double-confirm EXIT; `RESIDENTRESET empty=1` and
  final MENU_MAIN FNV 522dc605/heap8 164184 B.
- The source-linked teardown is only dynamically validated if an
  actual `[RENDERFREE] ENTRY` occurs. Otherwise mark owner/build
  validated but direct free invocation unexercised, and plan a
  deliberate bounded teardown test before retiring `Render_freeRuntime`.

Do not claim extra memory saved merely by removing a wrapper.
