#ifndef DOOMRPG_ESP32_NATIVE_MENU_WALL_FRAME_PROBE_H
#define DOOMRPG_ESP32_NATIVE_MENU_WALL_FRAME_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct Render_s;

/* Render one deterministic walls-only frame from the real menu.bsp runtime
 * structures using the menu spawn camera, original BSP visibility traversal,
 * and bounded GFXRM wall frames. No sprite or floor/ceiling texture sampling is
 * performed in this increment.
 */
int DoomRPG_probeNativeMenuWallFrame(struct Render_s* render);

#ifdef __cplusplus
}
#endif

#endif
