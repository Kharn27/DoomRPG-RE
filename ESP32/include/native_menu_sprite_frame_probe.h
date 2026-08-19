#ifndef DOOMRPG_ESP32_NATIVE_MENU_SPRITE_FRAME_PROBE_H
#define DOOMRPG_ESP32_NATIVE_MENU_SPRITE_FRAME_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct Render_s;

/* Adds the real BSP-sorted visible sprite list on top of the already validated
 * cached walls-only menu framebuffer. Sprite frames are deliberately uncached
 * so the first real request sequence can be measured on hardware.
 */
int DoomRPG_probeNativeMenuSpriteFrame(struct Render_s* render);

#ifdef __cplusplus
}
#endif

#endif
