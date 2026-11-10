#ifndef DOOMRPG_ESP32_NATIVE_TEXTURE_H
#define DOOMRPG_ESP32_NATIVE_TEXTURE_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_native_indexed_image.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Permanent native pixel rectangle and texture representation.
 * Compatibility SDL_Texture and SDL_Rect become aliases in ESP32/SDL.h;
 * the old desktop headers/behavior remain untouched. */
typedef struct EspNativeRect_s {
    int x, y, w, h;
} EspNativeRect;

typedef struct EspNativeTexture_s {
    int width;
    int height;
    int pitch;
    uint16_t* pixels;
    uint8_t* indexedPixels;
    EspNativeImageColor* palette;
    int paletteSize;
    uint8_t indexedBitsPerPixel;
    bool indexed;
    uint8_t red, green, blue;
    bool hasColorKey;
    uint16_t colorKey;
} EspNativeTexture;

/* Takes the allocated packed indices and quantized native palette only
 * on success. Failure leaves both pointers in image; caller frees them. */
EspNativeTexture* EspNativeTexture_adoptIndexed(EspNativeIndexedImage* image);
void EspNativeTexture_destroy(EspNativeTexture* texture);
int EspNativeTexture_setColorMod(EspNativeTexture* texture,
                                  uint8_t red, uint8_t green, uint8_t blue);

/* Draw into the existing single PlatformVideo RGB565 framebuffer. Source
 * and destination are measured in logical pixels, nearest neighbor.
 * Palette, magenta key, modulation and clipping match the old SDL-A shim. */
int EspNativeTexture_blit(const EspNativeTexture* texture,
                          const EspNativeRect* source,
                          const EspNativeRect* destination);

/* Temporary clip bridge for the still-live SDL rectangle/line primitives.
 * SDL_RenderSetClipRect forwards to this native texture-owned state. */
void EspNativeTexture_setClipRect(const EspNativeRect* clip);

#ifdef __cplusplus
}
#endif
#endif
