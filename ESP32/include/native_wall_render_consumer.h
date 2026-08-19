#ifndef DOOMRPG_ESP32_NATIVE_WALL_RENDER_CONSUMER_H
#define DOOMRPG_ESP32_NATIVE_WALL_RENDER_CONSUMER_H

#ifdef __cplusplus
extern "C" {
#endif

struct Render_s;

/*
 * Hardware bring-up step for the first ESP32-native wall-texture consumer.
 * Loads one real 64x64 4-bpp wall texture directly from the native pack,
 * closes the pack, then rasterizes it into the shared RGB565 framebuffer
 * without the legacy monolithic mediaTexels pool.
 */
int DoomRPG_probeNativeWallRenderConsumer(struct Render_s* render);

#ifdef __cplusplus
}
#endif

#endif
