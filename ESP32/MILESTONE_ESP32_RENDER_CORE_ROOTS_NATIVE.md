# Milestone — Render init/setup ABI roots native

Status: **CODE CANDIDATE — real-CYD test pending**

Same branch: `agent/esp32-retire-desktop-source-registration`. Previous hardware-PASS closure:
`21ed38cba7257dae8a65da6437a50cf52f6a6671`.

`Render_init` and `Render_setup` now compile as permanent ESP32-owned
functions in `ESP32/src/render_startup_bridge.c`; their inherited
definitions are excluded from the generated Render.c translation unit
under `DOOMRPG_ESP32`. The original desktop `src/Render.c`
is untouched. A pinned source-region CRC32 `0xb89f15f1` plus signature
checks forces review if the desktop specification changes.

The native definitions replicate original allocation, zeroing,
field defaults, viewport geometry and error semantics. The only
additional behavior is one source-ownership Serial diagnostic per
startup function:
`[RENDERCORE] INIT owner=esp-native-bridge bytes=1532`
and `[RENDERCORE] SETUP owner=esp-native-bridge view=160x80@0,20 arrays=1280B`.
The existing `Render_init` log remains unchanged.

No renderer resources, gameplay entities, cache, plane, BSP, RNG,
script or framebuffer ownership changes. Remaining `Render_free`,
palette/mappings, projection and other Render roots still link
through generated desktop `Render.c`; it is **not yet retired**.
No memory improvement is promised from moving these two functions.

## Hardware acceptance

Normal production `esp32-cyd` CI SUCCESS, then physical classic CYD:
verify `Render=1532`, `CORE Render used=1548`,
`[LAYOUT] arrays=1280B`, `heap8=178500`, `MAPPINGS heap8=159340`,
MAP_INTRO first frame `71ca7465`, arena `c3882516`,
`shapeData=mediaTexels=NULL`, animations and interactions.
Exercise native dialogue, then HUB SYS EXIT, ensuring
`[DIALOGCHAIN] OWNER-RELEASE`, `[RESIDENTRESET] empty=1`,
MENU_MAIN FNV `522dc605`, `heap8=164184`.

No hardware results may be invented. Stay on same active branch,
then document this exact code SHA after the physical PASS.
