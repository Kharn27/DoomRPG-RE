#ifndef DOOMRPG_ESP32_NATIVE_GRAPHICS_CATALOG_H
#define DOOMRPG_ESP32_NATIVE_GRAPHICS_CATALOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_GRAPHICS_PALETTE_COLORS 16U

typedef enum EspNativeGraphicsCatalogStatus_e {
    ESP_NATIVE_GRAPHICS_CATALOG_INVALID = 0,
    ESP_NATIVE_GRAPHICS_CATALOG_RUNTIME_NOT_READY = 1,
    ESP_NATIVE_GRAPHICS_CATALOG_ALREADY_ACTIVE = 2,
    ESP_NATIVE_GRAPHICS_CATALOG_PACK_BUSY = 3,
    ESP_NATIVE_GRAPHICS_CATALOG_PACK_OPEN_FAILED = 4,
    ESP_NATIVE_GRAPHICS_CATALOG_SOURCE_MISSING = 5,
    ESP_NATIVE_GRAPHICS_CATALOG_SOURCE_INVALID = 6,
    ESP_NATIVE_GRAPHICS_CATALOG_RESOURCE_UNSUPPORTED = 7,
    ESP_NATIVE_GRAPHICS_CATALOG_ALLOC_FAILED = 8,
    ESP_NATIVE_GRAPHICS_CATALOG_READ_FAILED = 9,
    ESP_NATIVE_GRAPHICS_CATALOG_OK = 10
} EspNativeGraphicsCatalogStatus;

/*
 * One compact immutable mapping for a resource actually required by the
 * resident map. No texel payload is retained here: sourceOffset points into
 * the original bitshape/wall-texel backing store and paletteRgb565 carries
 * exactly the 16 colors needed to decode its packed 4-bit texels.
 */
typedef struct EspNativeGraphicsCatalogRecord_s {
    uint16_t resourceId;
    uint16_t paletteSourceOffset;
    uint32_t sourceOffset;
    uint16_t paletteRgb565[ESP_NATIVE_GRAPHICS_PALETTE_COLORS];
} EspNativeGraphicsCatalogRecord;

typedef struct EspNativeGraphicsCatalogView_s {
    const EspNativeGraphicsCatalogRecord* textures;
    const EspNativeGraphicsCatalogRecord* sprites;
    uint16_t textureCount;
    uint16_t spriteCount;
    uint32_t storageBytes;
    uint32_t stateFNV1a;
} EspNativeGraphicsCatalogView;

/* Release the one compact catalog arena, if any. */
void EspNativeGraphicsCatalog_reset(void);

/*
 * Build a sparse immutable catalog directly from mappings.bin + palettes.bin
 * in DoomRPG-ESP32.pak, using the current EspMapRuntime resource bitsets.
 *
 * Texture records are the union of textureRequired() and planeTextureUsed().
 * Sprite records come from spriteRequired(). The asset pack is closed before
 * return on both success and failure.
 */
EspNativeGraphicsCatalogStatus EspNativeGraphicsCatalog_buildFromRuntime(void);

int EspNativeGraphicsCatalog_isReady(void);
const EspNativeGraphicsCatalogView* EspNativeGraphicsCatalog_view(void);

const EspNativeGraphicsCatalogRecord*
EspNativeGraphicsCatalog_findTexture(uint16_t resourceId);
const EspNativeGraphicsCatalogRecord*
EspNativeGraphicsCatalog_findSprite(uint16_t resourceId);

#ifdef __cplusplus
}
#endif

#endif
