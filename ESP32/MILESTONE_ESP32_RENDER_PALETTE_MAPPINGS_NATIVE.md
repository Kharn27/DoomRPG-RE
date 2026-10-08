# Milestone — native palette/color and mappings ABI roots

Status: **REAL-CYD HARDWARE PASS — code frozen**.

Normal firmware hardware-tested SHA: `ea221b4a7248d05ceda077f6c226e7da0851d302`.
CI #37707296749 **SUCCESS**; static RAM 45056 B,
flash 774693 B (12 B below preceding native init/setup milestone).

## Measured production classic CYD witness — 2026-10-08

The user flashed the normal production firmware and posted the full
boot/intro/map-start/gameplay/dialog/EXIT sequence. Exactly
3280 palette entries / 6560 B were loaded, and native mappings
installed 8376 B with existing framebuffer scratch, without inflating
map-wide texture storage. Heap8 remained 178500 after layout,
167780 after Render startup, 159340 after mappings. Starting menu
frame FNV 522dc605, MAP_INTRO arena c3882516 and first native world
frame 71ca7465 all matched their verified baselines.
One-midpoint FORWARD and two-preview TURN frames continued to present;
every committed MOVE advanced precisely one monster turn, rotation none.
Native event 88 dialog resumed bytecode opcode 19 state mutation.
Dialog chain journal reclaimed 1036 B at session teardown.
`[RESIDENTRESET]` logged `released=18008 empty=1`, and final
`[SYSEXIT] MENU-READY` returned exact menu FNV 522dc605 with
saveWrite=no and session=off. Follow-on ALIVE samples remained
heap8 164184, largest8 110580. `shapeData` and `mediaTexels`
remained NULL. No regression observed.

The excerpt does **not** re-exercise active-monster fighting, a new
LOAD, CHANGEMAP, blocked collision, or door flow on this SHA.
The native code SHA is frozen: subsequent changes to this milestone
must be docs-only. Further legacy retirements are separate
milestones on the *same* active branch.
Branch: `agent/esp32-retire-desktop-source-registration`; no merge or new branch.
Prior physically tested Render init/setup code:
`9316dd58ad65e916e35b317ad48ff37faffb2d1c`.
Docs-only closure: `399c38fe87004981889e6c88c1b5b8bdee25d5e5`.

## Change boundary

The next five inherited `Render.c` ABI definitions are excluded from
the ESP32 generated translation unit and moved into existing permanent
native source files, without modifying desktop originals:
`Render_loadPalettes`, `Render_make565RGB`,
`Render_RGB888_To_RGB565`, `Render_setGrayPalettes` to
`render_startup_bridge.c`; `Render_loadMappings` to
`esp_legacy_config_mappings_startup.c`.

The four palette/color function bodies are copied from the legacy source
region byte-for-byte, preserving packed palette decoding, RGB565 rounding,
gray conversion, allocation and PAK read semantics. `Render_loadMappings`
is the existing native ESP32 wrapper
`EspLegacyMappings_load(render) ? true : false`.
A strict five-export census and legacy source-region CRC32 `0x6f5b63d3`
fail the build on unexpected inherited changes.

Generated `Render.c` still includes `Render_startup`,
`Render_free`, geometry/projection, BSP fail-closed legacy stubs and
other linked behavior. This milestone does **not** claim full removal
of `Render.c`, desktop headers or compatibility objects.

Build instrumentation reports
`[ESP32] Render palette/mappings desktop roots retired; exports=5
palette=esp-render-startup mappings=esp-config-mappings`.
Normal firmware should preserve `[RENDER] palettes loaded: entries=3280
bytes=6560`, `[MAPPINGS] INSTALLED payload=8376`, and the same
frame FNVs without creating new framebuffer or map-wide texture buffers.

## Acceptance criteria

Normal `esp32-cyd` CI SUCCESS then actual CYD:
- `CORE Render used=1548`, first setup `arrays=1280B`;
  palettes 3280 entries, mappings 8376 B, heap8 after
  RENDERSTART 167780 and after CONFIGMAP 159340.
- Same native MAP_INTRO arena FNV c3882516,
  first gameplay frame 71ca7465 and menu 522dc605.
- Confirm moving/rotating, one pickup/dialog and clean SYS EXIT:
  `DIALOGCHAIN OWNER-RELEASE recovered=1036`,
  `RESIDENTRESET empty=1`, final heap8 164184.
- `shapeData==NULL`, `mediaTexels==NULL`, and zero unexpected
  `[NATIVEFRAME]` unrecovered errors.

Physical hardware PASS is recorded above against the exact tested SHA.
