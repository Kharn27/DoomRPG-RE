#ifndef DOOMRPG_ESP32_NATIVE_INDEXED_IMAGE_H
#define DOOMRPG_ESP32_NATIVE_INDEXED_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Permanent compact indexed image owner, independent of SDL and the renderer.
 * Only packed pixels and a quantized palette survive a successful decode.
 * The importer transfers both allocations without copying; the source image
 * is then empty and safe to dispose. */
typedef struct EspNativeImageColor_s {
    uint8_t r, g, b, a;
} EspNativeImageColor;

typedef struct EspNativeIndexedImage_s {
    int width;
    int height;
    int pitch;
    uint8_t bitsPerPixel;
    uint8_t hasColorKey;
    uint16_t paletteSize;
    uint8_t* pixels;
    EspNativeImageColor* palette;
} EspNativeIndexedImage;

/* Read a range from one immutable BMP resource; the callback must reject
 * out-of-bounds reads. alloc/release may be supplied to exercise OOM on host;
 * production passes NULL to use malloc/free. */
typedef struct EspNativeImageInput_s {
    void* context;
    uint32_t size;
    int (*readRange)(void*, uint32_t, void*, uint32_t);
    void* (*allocate)(size_t);
    void (*release)(void*);
} EspNativeImageInput;

typedef enum EspNativeImageStatus_e {
    ESP_NATIVE_IMAGE_OK = 0,
    ESP_NATIVE_IMAGE_INVALID,
    ESP_NATIVE_IMAGE_UNSUPPORTED,
    ESP_NATIVE_IMAGE_TRUNCATED,
    ESP_NATIVE_IMAGE_OOM,
    ESP_NATIVE_IMAGE_PACK_UNAVAILABLE
} EspNativeImageStatus;

EspNativeImageStatus EspNativeImage_decode(const EspNativeImageInput* input,
                                           int transparentMask,
                                           EspNativeIndexedImage* output);

/* Consumes the exclusive PAK lease and closes it on every exit path.
 * No ZIP, intermediate BMP allocation, SDL_RWops or SDL_Surface. */
EspNativeImageStatus EspNativeImage_loadPack(const char* name,
                                             int transparentMask,
                                             EspNativeIndexedImage* output);

/* For unadopted images only. Adopted allocations are owned by the texture. */
void EspNativeImage_dispose(EspNativeIndexedImage* image);

#ifdef __cplusplus
}
#endif
#endif
