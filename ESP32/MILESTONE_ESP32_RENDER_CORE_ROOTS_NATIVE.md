# Milestone — Render init/setup ABI roots native

Status: **REAL-CYD HARDWARE PASS — documented/frozen code**

Hardware-tested code SHA `9316dd58ad65e916e35b317ad48ff37faffb2d1c`, GitHub Actions normal
`esp32-cyd` CI #37706764596 SUCCESS (static RAM 45056 B,
flash 774705 B). Docs-only closure follows.

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

## Actual hardware witness (2026-10-08)

Real classic CYD confirmed both `[RENDERCORE]` markers,
`Render=1532`, heap-used **1548** for Render, layout `arrays=1280B`,
heap8 after layout **178500**, mappings **159340**. New Game,
ST_INTRO/continue/dispose, native BSP `c3882516`, first frame
`71ca7465`, native 1-midpoint FORWARD and 2-thirds STRAFE previews,
TURN rotation, crate transform, Armor Shard pickup, native DIALOG
(event 88, resume opcode 19, stateMutation=1) and SYS EXIT all ran.
A known BSP span guard was automatically retried/recovered.
`[DIALOGCHAIN] OWNER-RELEASE` recovered **1036 B**,
`[RESIDENTRESET] ... released=18008 ... empty=1`, final
`MENU_MAIN=522dc605` with heap8=**164184**, largest8=**110580**,
`shapeData=mediaTexels=NULL` and no SAVE writes.
The observed different RNG table hash is expected from the seeded
runtime and is not a fixed replay fingerprint. Active monsters,
LOAD, CHANGEMAP and blocked collisions are outside this excerpt.

The hardware-tested code is frozen; changes immediately after its
PASS are documentation-only. Further native Render extractions on this
same branch must have independently tested SHA boundaries.

## Historical acceptance checklist

Normal production `esp32-cyd` CI SUCCESS, then physical classic CYD:
verify `Render=1532`, `CORE Render used=1548`,
`[LAYOUT] arrays=1280B`, `heap8=178500`, `MAPPINGS heap8=159340`,
MAP_INTRO first frame `71ca7465`, arena `c3882516`,
`shapeData=mediaTexels=NULL`, animations and interactions.
Exercise native dialogue, then HUB SYS EXIT, ensuring
`[DIALOGCHAIN] OWNER-RELEASE`, `[RESIDENTRESET] empty=1`,
MENU_MAIN FNV `522dc605`, `heap8=164184`.

This physical PASS is recorded above; it must not be extrapolated to
unexercised scenarios.
