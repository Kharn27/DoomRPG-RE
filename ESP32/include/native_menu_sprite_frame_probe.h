#ifndef DOOMRPG_ESP32_NATIVE_MENU_SPRITE_FRAME_PROBE_H
#define DOOMRPG_ESP32_NATIVE_MENU_SPRITE_FRAME_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct Render_s;

/* Retired scene-probe declaration, retained for the dormant dashboard bridge.
 * Its implementation is removed and neither profile links the bridge.
 * Do not make it a prerequisite for the current opaque native menu.
 */
int DoomRPG_probeNativeMenuSpriteFrame(struct Render_s* render);

#ifdef __cplusplus
}
#endif

#endif
