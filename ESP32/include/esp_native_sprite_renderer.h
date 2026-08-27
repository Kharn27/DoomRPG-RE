#ifndef DOOMRPG_ESP32_NATIVE_SPRITE_RENDERER_H
#define DOOMRPG_ESP32_NATIVE_SPRITE_RENDERER_H

/*
 * Production, map-generic facade for the native sprite renderer.
 *
 * The implementation was originally proven by the Junction milestones and its
 * historical symbol/file names are kept as compatibility ABI for those probes.
 * Gameplay code must depend on this facade instead of treating Junction as a
 * runtime precondition or a separate renderer.
 */
#include "esp_native_junction_sprite_renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef EspNativeJunctionSpriteStats EspNativeSpriteStats;

static inline int EspNativeSpriteRenderer_render(
    struct Render_s* render,
    EspNativeSpriteStats* outStats) {
    return EspNativeJunctionSprite_render(render, outStats);
}

#ifdef __cplusplus
}
#endif

#endif
