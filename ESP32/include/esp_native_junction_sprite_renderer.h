#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_SPRITE_RENDERER_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_SPRITE_RENDERER_H

/*
 * Transitional private compatibility shim.
 *
 * The renderer is now owned by esp_native_sprite_renderer.h and exports the
 * map-generic EspNativeSpriteRenderer_* API. The implementation translation
 * unit still contains historical Junction identifiers; these aliases make
 * those tokens compile directly to the generic ABI without preserving a
 * Junction-specific runtime symbol.
 *
 * New production code must include esp_native_sprite_renderer.h directly.
 */
#include "esp_native_sprite_renderer.h"

typedef EspNativeSpriteStats EspNativeJunctionSpriteStats;
#define EspNativeJunctionSprite_render EspNativeSpriteRenderer_render

#endif
