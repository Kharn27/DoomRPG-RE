#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "native_asset_pack_probe.h"

/* Keep ESP-IDF headers after DoomRPG.h: stdbool false/true macros collide
 * with the engine's legacy boolean enum. */
#include <esp_heap_caps.h>

extern DoomRPG_t* doomRpg;

#define PACKED_WALL_TEXTURE_BYTES 2048U
#define WTEXELS_HEADER_BYTES 4U
#define MEDIA_TEXEL_OFFSET_INT_COUNT 592

static int nativePackAttempted = 0;
static int nativePackReady = 0;

static uint32_t heap8Free(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8Block(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static uint32_t readLe32(const byte* data) {
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static uint32_t fnv1a32(const byte* data, uint32_t length) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    for (i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

static void printEntry(const EspAssetPackEntry* entry) {
    if (entry == NULL) {
        return;
    }
    printf("[ASSETPAK] %-13s offset=%u size=%u crc32=%08x flags=%u\n",
           entry->name,
           (unsigned int)entry->offset,
           (unsigned int)entry->size,
           (unsigned int)entry->crc32,
           (unsigned int)entry->flags);
}

int DoomRPG_probeNativeAssetPack(int resourcePlanReady) {
    Render_t* render;
    EspAssetPackEntry bitshapes;
    EspAssetPackEntry wtexels;
    EspAssetPackEntry stexels;
    byte wtexelsHeader[WTEXELS_HEADER_BYTES];
    byte* textureData;
    uint32_t heapBeforeOpen;
    uint32_t heapAfterOpen;
    uint32_t heapBeforeTexture;
    uint32_t heapWithTexture;
    uint32_t heapAfterTexture;
    uint32_t heapAfterClose;
    uint32_t largestBeforeTexture;
    uint32_t sourceTexelOffset;
    uint32_t sourceByteOffset;
    uint32_t textureReadOffset;
    uint32_t textureHash;
    uint32_t sourceDataSize;
    int sourceTexelOffsetSigned;
    int textureIndex;

    if (nativePackAttempted) {
        return nativePackReady;
    }
    nativePackAttempted = 1;

    printf("\n=== Doom RPG ESP32-native asset pack probe ===\n");

    if (!resourcePlanReady) {
        printf("[ASSETPAK] Resource memory plan is not ready; probe skipped safely\n");
        return 0;
    }

    if (doomRpg == NULL || doomRpg->render == NULL) {
        printf("[ASSETPAK] Render object unavailable; probe refused\n");
        return 0;
    }

    render = doomRpg->render;
    if (render->mapTextureTexels == NULL || render->mapTextureTexelsCount <= 0 ||
        render->mediaTexelOffsets == NULL) {
        printf("[ASSETPAK] Real menu texture reference list is unavailable\n");
        return 0;
    }

    heapBeforeOpen = heap8Free();
    printf("[ASSETPAK] Opening %s heap8=%u largest8=%u\n",
           ESP_ASSET_PACK_DEFAULT_PATH,
           (unsigned int)heapBeforeOpen,
           (unsigned int)largest8Block());

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        printf("[ASSETPAK] MISSING/INVALID %s\n", ESP_ASSET_PACK_DEFAULT_PATH);
        printf("[ASSETPAK] Build it offline with ESP32/tools/build_asset_pack.py and copy it to the SD root\n");
        return 0;
    }

    heapAfterOpen = heap8Free();
    printf("[ASSETPAK] READY index entries=%d fileSize=%u heapCost=%dB\n",
           EspAssetPack_entryCount(),
           (unsigned int)EspAssetPack_fileSize(),
           (int)heapBeforeOpen - (int)heapAfterOpen);

    if (!EspAssetPack_findEntry("bitshapes.bin", &bitshapes) ||
        !EspAssetPack_findEntry("wtexels.bin", &wtexels) ||
        !EspAssetPack_findEntry("stexels.bin", &stexels)) {
        printf("[ASSETPAK] FAILED required heavy asset entries are missing\n");
        EspAssetPack_close();
        return 0;
    }

    printEntry(&bitshapes);
    printEntry(&wtexels);
    printEntry(&stexels);

    if (!EspAssetPack_readRange(&wtexels, 0, wtexelsHeader, sizeof(wtexelsHeader))) {
        printf("[ASSETPAK] FAILED reading wtexels.bin header\n");
        EspAssetPack_close();
        return 0;
    }
    sourceDataSize = readLe32(wtexelsHeader);
    printf("[ASSETPAK] wtexels source header dataSize=%u\n",
           (unsigned int)sourceDataSize);

    textureIndex = render->mapTextureTexels[0];
    if (textureIndex < 0 ||
        (textureIndex * 2 + 1) >= MEDIA_TEXEL_OFFSET_INT_COUNT) {
        printf("[ASSETPAK] FAILED first referenced texture index=%d exceeds mapping table\n",
               textureIndex);
        EspAssetPack_close();
        return 0;
    }

    sourceTexelOffsetSigned = render->mediaTexelOffsets[textureIndex * 2];
    if (sourceTexelOffsetSigned < 0) {
        printf("[ASSETPAK] FAILED source texel offset=%d is negative\n",
               sourceTexelOffsetSigned);
        EspAssetPack_close();
        return 0;
    }

    sourceTexelOffset = (uint32_t)sourceTexelOffsetSigned;
    if ((sourceTexelOffset & 1U) != 0) {
        printf("[ASSETPAK] FAILED source texel offset=%u is not nibble-pair aligned\n",
               (unsigned int)sourceTexelOffset);
        EspAssetPack_close();
        return 0;
    }

    sourceByteOffset = sourceTexelOffset / 2U;
    textureReadOffset = WTEXELS_HEADER_BYTES + sourceByteOffset;

    if (textureReadOffset > wtexels.size ||
        PACKED_WALL_TEXTURE_BYTES > wtexels.size - textureReadOffset) {
        printf("[ASSETPAK] FAILED texture index=%d read range exceeds wtexels.bin\n",
               textureIndex);
        EspAssetPack_close();
        return 0;
    }

    printf("[ASSETPAK] Real menu texture index=%d texelOffset=%u byteOffset=%u read=%uB\n",
           textureIndex,
           (unsigned int)sourceTexelOffset,
           (unsigned int)sourceByteOffset,
           (unsigned int)PACKED_WALL_TEXTURE_BYTES);

    heapBeforeTexture = heap8Free();
    largestBeforeTexture = largest8Block();
    textureData = (byte*)SDL_malloc(PACKED_WALL_TEXTURE_BYTES);
    if (textureData == NULL) {
        printf("[ASSETPAK] FAILED bounded 2048-byte texture allocation\n");
        EspAssetPack_close();
        return 0;
    }

    heapWithTexture = heap8Free();
    printf("[ASSETPAK] Bounded texture buffer resident heap8=%u largest8=%u used=%u\n",
           (unsigned int)heapWithTexture,
           (unsigned int)largest8Block(),
           (unsigned int)(heapBeforeTexture >= heapWithTexture
                              ? heapBeforeTexture - heapWithTexture
                              : 0));

    if (!EspAssetPack_readRange(&wtexels,
                                textureReadOffset,
                                textureData,
                                PACKED_WALL_TEXTURE_BYTES)) {
        printf("[ASSETPAK] FAILED random-access texture read\n");
        SDL_free(textureData);
        EspAssetPack_close();
        return 0;
    }

    textureHash = fnv1a32(textureData, PACKED_WALL_TEXTURE_BYTES);
    printf("[ASSETPAK] READ texture=%d bytes=%u fnv1a=%08x first=%02x%02x%02x%02x last=%02x%02x%02x%02x\n",
           textureIndex,
           (unsigned int)PACKED_WALL_TEXTURE_BYTES,
           (unsigned int)textureHash,
           textureData[0], textureData[1], textureData[2], textureData[3],
           textureData[PACKED_WALL_TEXTURE_BYTES - 4U],
           textureData[PACKED_WALL_TEXTURE_BYTES - 3U],
           textureData[PACKED_WALL_TEXTURE_BYTES - 2U],
           textureData[PACKED_WALL_TEXTURE_BYTES - 1U]);

    SDL_free(textureData);
    textureData = NULL;
    heapAfterTexture = heap8Free();

    printf("[ASSETPAK] Released texture buffer heap8=%u largest8=%u delta=%d\n",
           (unsigned int)heapAfterTexture,
           (unsigned int)largest8Block(),
           (int)heapBeforeTexture - (int)heapAfterTexture);

    if (heapAfterTexture != heapBeforeTexture ||
        largest8Block() != largestBeforeTexture) {
        printf("[ASSETPAK] FAILED bounded texture read changed heap layout\n");
        EspAssetPack_close();
        return 0;
    }

    EspAssetPack_close();
    heapAfterClose = heap8Free();
    printf("[ASSETPAK] Closed pack heap8=%u deltaFromOpenStart=%d\n",
           (unsigned int)heapAfterClose,
           (int)heapBeforeOpen - (int)heapAfterClose);

    printf("[ASSETPAK] READY random-access real wall texture read with 2048B working set\n");
    printf("[ASSETPAK] No ZIP inflate and no monolithic mediaTexels allocation executed\n");

    nativePackReady = 1;
    return 1;
}
