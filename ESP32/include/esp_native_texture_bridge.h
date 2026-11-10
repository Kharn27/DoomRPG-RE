#ifndef DOOMRPG_ESP32_NATIVE_TEXTURE_BRIDGE_H
#define DOOMRPG_ESP32_NATIVE_TEXTURE_BRIDGE_H

#include "esp_native_indexed_image.h"
#include <SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Temporary compatibility adapter: Image_t is still SDL_Texture* until
 * the SDL-D/SDL-F ABI migrations. The compact indexed payload is native.
 * On success pixels/palette move to the texture (no memcpy); on failure
 * the image is untouched and its caller retains both allocations. */
SDL_Texture* EspNativeTexture_adoptIndexed(EspNativeIndexedImage* image);

#ifdef __cplusplus
}
#endif
#endif
