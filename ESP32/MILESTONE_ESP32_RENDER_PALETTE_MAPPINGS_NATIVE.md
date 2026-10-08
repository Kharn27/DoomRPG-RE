# Milestone — native palette/color and mappings ABI roots

Status: **CODE CANDIDATE — real-CYD validation pending**.
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

No physical PASS is claimed until the exact code SHA is tested.
