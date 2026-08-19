#ifndef DOOMRPG_ESP32_NATIVE_SPRITE_RENDER_CONSUMER_H
#define DOOMRPG_ESP32_NATIVE_SPRITE_RENDER_CONSUMER_H

struct Render_s;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Hardware bring-up for the first real ESP32-native sprite consumer.
 *
 * Loads one selected Doom RPG sprite from DoomRPG-ESP32.pak into a bounded
 * frame object containing only its source mask and packed texels, rasterizes it
 * directly into the shared 160x120 framebuffer, presents it on the CYD, then
 * releases the frame again.
 *
 * The path deliberately does not use legacy render->shapeData or
 * render->mediaTexels. A successful probe therefore validates the core data
 * contract that a future runtime sprite cache/rasterizer can use.
 */
int DoomRPG_probeNativeSpriteRenderConsumer(struct Render_s* render);

#ifdef __cplusplus
}
#endif

#endif
