# ESP32 milestone — eight obsolete Render entrypoints native-owned

Status: **CODE CANDIDATE — normal esp32-cyd CI PASS, REAL-CYD pending**
Tested source SHA: `6a6bdd565a526e27f53b45c66f69e7771a91c78d`
Normal PlatformIO `esp32-cyd` CI: [run 37765970962](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37765970962), **SUCCESS**.
Static RAM: **45056 B**, flash: **774897 B**, both unchanged from
the preceding physical PASS code `77f1de060142b869bbc83727ad17ae137b21789c`.

## Boundaries and owners

The following **eight already fail-closed/no-op ESP32 production exports**
move from generated desktop `Render.c` to the permanent
`ESP32/src/esp_legacy_render_reject.c`:

- `Render_beginLoadMap`, `Render_beginLoadMapData`
- `Render_render`, `Render_renderFloorAndCeilingBG`
- `Render_drawplane`, `Render_spanPlane`
- `Render_renderBSP`, `Render_walkNode`

The rejection diagnostics and false/no-op behavior are unchanged.
`ESP32/scripts/build_engine.py` now compiles the original desktop
definitions **only** under `DOOMRPG_ESP32_BRINGUP_PROBES`.
Normal `esp32-cyd` obtains all eight definitions from the permanent
native bridge. Source-shape guards for retired map flags, plane records
and BSP viewNodes continue to fail closed on unexpected changes.
A permanent-native export census guards the exact eight endpoints.
Build marker:
`[ESP32] Legacy Render map/world/BSP rejects native-owned;
exports=8 production=fail-closed bringup=desktop-original`.

The original `src/Render.c` is unchanged and remains the desktop reference.
The ESP32 build still compiles the generated `Render.c` for remaining
geometry and compatibility dependencies. This is **not** full
translation-unit retirement. The separate `Render_free()` direct root
was linker- and gameplay-tested previously, but no `[RENDERFREE] ENTRY`
occurred during ordinary SYS EXIT; destructor execution remains unproven.
The bringup build was **not** re-tested in this milestone.

## Hardware acceptance gate

With the normal `esp32-cyd` firmware, verify full boot and:
- shared 160x120 RGB565 framebuffer **38400 B**,
  `[RENDERSTART] OWNER ... direct=yes wrap=no`;
- palette **3280** entries and mapping payload **8376 B**;
- `MENU_MAIN` FNV `522dc605`, `MAPRT` arena
  `c3882516`, first gameplay framebuffer `71ca7465`;
- FORWARD and TURN, one native interaction (dialog/pickup/crate),
  and HUB -> SYS -> double-confirm EXIT;
- `[RESIDENTRESET] ... empty=1`, final menu FNV `522dc605`,
  heap8 **164184 B** with largest8 **110580 B**;
- `shapeData==NULL` and `mediaTexels==NULL`;
- **no** `[LEGACYMAP] REJECT`, `[LEGACYRENDER] REJECT`
  or `[LEGACYBSP] REJECT` messages in ordinary gameplay.

Only once the user supplies actual Serial evidence should the
milestone be marked **REAL-CYD PASS**. After that, close it with
documentation-only commits. User performs merge; no automatic main merge.
