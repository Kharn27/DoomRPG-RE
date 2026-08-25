#ifndef DOOMRPG_ESP32_NATIVE_INDEXED_BMP_H
#define DOOMRPG_ESP32_NATIVE_INDEXED_BMP_H

#include <stdint.h>

#include "esp_asset_pack.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_INDEXED_BMP_MAX_PALETTE 256U
#define ESP_NATIVE_INDEXED_BMP_MAX_ROW_BYTES 256U

typedef enum EspNativeIndexedBmpStatus_e {
    ESP_NATIVE_INDEXED_BMP_INVALID = 0,
    ESP_NATIVE_INDEXED_BMP_PACK_NOT_OPEN = 1,
    ESP_NATIVE_INDEXED_BMP_NOT_FOUND = 2,
    ESP_NATIVE_INDEXED_BMP_READ_FAILED = 3,
    ESP_NATIVE_INDEXED_BMP_UNSUPPORTED = 4,
    ESP_NATIVE_INDEXED_BMP_OK = 5
} EspNativeIndexedBmpStatus;

typedef struct EspNativeIndexedBmpStats_s {
    uint32_t packReads;
    uint32_t bytesRead;
    uint32_t rowsRead;
    uint32_t pixelsWritten;
} EspNativeIndexedBmpStats;

/*
 * Stack-friendly metadata for one uncompressed indexed BMP stored inside the
 * native PAK. Pixel payload is never retained: blits seek/read one bounded file
 * row at a time. The palette is converted once to the framebuffer's permanent
 * RGB565 representation.
 */
typedef struct EspNativeIndexedBmp_s {
    EspAssetPackEntry entry;
    uint32_t pixelOffset;
    uint16_t width;
    uint16_t height;
    uint16_t filePitch;
    uint16_t packedPitch;
    uint16_t paletteCount;
    uint8_t bitsPerPixel;
    uint8_t topDown;
    uint16_t paletteRgb565[ESP_NATIVE_INDEXED_BMP_MAX_PALETTE];
} EspNativeIndexedBmp;

EspNativeIndexedBmpStatus EspNativeIndexedBmp_open(
    const char* name,
    EspNativeIndexedBmp* outBmp,
    EspNativeIndexedBmpStats* stats);

/* Draw one source rectangle. Coordinates are clipped only at the framebuffer. */
EspNativeIndexedBmpStatus EspNativeIndexedBmp_blit(
    const EspNativeIndexedBmp* bmp,
    uint16_t* framebuffer,
    uint16_t framebufferWidth,
    uint16_t framebufferHeight,
    uint16_t sourceX,
    uint16_t sourceY,
    uint16_t width,
    uint16_t height,
    int16_t destinationX,
    int16_t destinationY,
    uint8_t transparentMagenta,
    EspNativeIndexedBmpStats* stats);

/* Repeat the complete source image across one destination rectangle. */
EspNativeIndexedBmpStatus EspNativeIndexedBmp_tile(
    const EspNativeIndexedBmp* bmp,
    uint16_t* framebuffer,
    uint16_t framebufferWidth,
    uint16_t framebufferHeight,
    int16_t destinationX,
    int16_t destinationY,
    uint16_t width,
    uint16_t height,
    uint8_t transparentMagenta,
    EspNativeIndexedBmpStats* stats);

#ifdef __cplusplus
}
#endif

#endif
