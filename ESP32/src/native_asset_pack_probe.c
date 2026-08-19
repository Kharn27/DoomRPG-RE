#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Render.h"
#include "Z_Zip.h"

#include "esp_asset_pack.h"
#include "native_asset_pack_probe.h"

/* Keep ESP-IDF headers after DoomRPG.h: stdbool false/true macros collide
 * with the engine's legacy boolean enum. */
#include <esp_heap_caps.h>

extern DoomRPG_t* doomRpg;

#define PACKED_WALL_TEXTURE_BYTES 2048U
#define WTEXELS_HEADER_BYTES 4U
#define MEDIA_TEXEL_OFFSET_INT_COUNT 592
#define ASSET_PACK_V2_HEADER_BYTES 24U
#define ASSET_PACK_V2_ENTRY_BYTES 20U

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

static void printEntry(const char* name, const EspAssetPackEntry* entry) {
    if (name == NULL || entry == NULL) {
        return;
    }
    printf("[ASSETPAK] %-16s hash=%08x offset=%u size=%u crc32=%08x flags=%u\n",
           name,
           (unsigned int)entry->nameHash,
           (unsigned int)entry->offset,
           (unsigned int)entry->size,
           (unsigned int)entry->crc32,
           (unsigned int)entry->flags);
}

static int validateFullPackAgainstZip(uint64_t* outPayloadBytes) {
    uint64_t payloadBytes = 0;
    int matched = 0;
    int i;

    if (outPayloadBytes == NULL || zipFile.entry == NULL || zipFile.entry_count <= 0) {
        printf("[ASSETPAK] FAILED original ZIP directory is unavailable for full-pack cross-check\n");
        return 0;
    }

    if (EspAssetPack_entryCount() != zipFile.entry_count) {
        printf("[ASSETPAK] FAILED full-pack entry count=%d ZIP entries=%d\n",
               EspAssetPack_entryCount(), zipFile.entry_count);
        return 0;
    }

    for (i = 0; i < zipFile.entry_count; ++i) {
        const zip_entry_t* zipEntry = &zipFile.entry[i];
        EspAssetPackEntry packEntry;

        if (zipEntry->name == NULL || zipEntry->name[0] == '\0' || zipEntry->usize < 0) {
            printf("[ASSETPAK] FAILED invalid ZIP directory entry at index=%d\n", i);
            return 0;
        }

        if (!EspAssetPack_findEntry(zipEntry->name, &packEntry)) {
            printf("[ASSETPAK] FAILED pack missing ZIP entry index=%d name=%s\n",
                   i, zipEntry->name);
            return 0;
        }

        if (packEntry.size != (uint32_t)zipEntry->usize) {
            printf("[ASSETPAK] FAILED size mismatch name=%s pack=%u ZIP=%d\n",
                   zipEntry->name,
                   (unsigned int)packEntry.size,
                   zipEntry->usize);
            return 0;
        }

        payloadBytes += packEntry.size;
        ++matched;
    }

    *outPayloadBytes = payloadBytes;
    printf("[ASSETPAK] FULL directory cross-check matched=%d/%d payload=%lluB\n",
           matched,
           zipFile.entry_count,
           (unsigned long long)payloadBytes);
    return 1;
}

int DoomRPG_probeNativeAssetPack(int resourcePlanReady) {
    Render_t* render;
    EspAssetPackEntry bitshapes;
    EspAssetPackEntry wtexels;
    EspAssetPackEntry stexels;
    EspAssetPackEntry mappings;
    EspAssetPackEntry menuBsp;
    byte wtexelsHeader[WTEXELS_HEADER_BYTES];
    byte* textureData;
    uint64_t fullPayloadBytes;
    uint64_t expectedPackBytes;
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
    uint32_t expectedDataOffset;
    int sourceTexelOffsetSigned;
    int textureIndex;

    if (nativePackAttempted) {
        return nativePackReady;
    }
    nativePackAttempted = 1;

    printf("\n=== Doom RPG ESP32-native FULL asset pack probe ===\n");

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
        printf("[ASSETPAK] MISSING/INVALID v2 %s\n", ESP_ASSET_PACK_DEFAULT_PATH);
        printf("[ASSETPAK] Regenerate it with ESP32/tools/build_asset_pack.py and copy it to the SD root\n");
        return 0;
    }

    heapAfterOpen = heap8Free();
    expectedDataOffset = ASSET_PACK_V2_HEADER_BYTES +
                         ((uint32_t)EspAssetPack_entryCount() * ASSET_PACK_V2_ENTRY_BYTES);

    printf("[ASSETPAK] READY v2 entries=%d fileSize=%u dataOffset=%u heapCost=%dB\n",
           EspAssetPack_entryCount(),
           (unsigned int)EspAssetPack_fileSize(),
           (unsigned int)EspAssetPack_dataOffset(),
           (int)heapBeforeOpen - (int)heapAfterOpen);
    printf("[ASSETPAK] Index stays on SD: header=%uB records=%d x %uB total=%uB\n",
           (unsigned int)ASSET_PACK_V2_HEADER_BYTES,
           EspAssetPack_entryCount(),
           (unsigned int)ASSET_PACK_V2_ENTRY_BYTES,
           (unsigned int)expectedDataOffset);

    if (EspAssetPack_dataOffset() != expectedDataOffset) {
        printf("[ASSETPAK] FAILED unexpected v2 data offset=%u expected=%u\n",
               (unsigned int)EspAssetPack_dataOffset(),
               (unsigned int)expectedDataOffset);
        EspAssetPack_close();
        return 0;
    }

    if (!validateFullPackAgainstZip(&fullPayloadBytes)) {
        EspAssetPack_close();
        return 0;
    }

    expectedPackBytes = (uint64_t)EspAssetPack_dataOffset() + fullPayloadBytes;
    if (expectedPackBytes != (uint64_t)EspAssetPack_fileSize()) {
        printf("[ASSETPAK] FAILED pack total=%u expected=%llu from index+ZIP payload\n",
               (unsigned int)EspAssetPack_fileSize(),
               (unsigned long long)expectedPackBytes);
        EspAssetPack_close();
        return 0;
    }

    printf("[ASSETPAK] FULL pack size proven index+payload=%lluB\n",
           (unsigned long long)expectedPackBytes);

    if (!EspAssetPack_findEntry("bitshapes.bin", &bitshapes) ||
        !EspAssetPack_findEntry("wtexels.bin", &wtexels) ||
        !EspAssetPack_findEntry("stexels.bin", &stexels) ||
        !EspAssetPack_findEntry("mappings.bin", &mappings) ||
        !EspAssetPack_findEntry("/MENU.BSP", &menuBsp)) {
        printf("[ASSETPAK] FAILED required representative entries are missing\n");
        EspAssetPack_close();
        return 0;
    }

    printEntry("bitshapes.bin", &bitshapes);
    printEntry("wtexels.bin", &wtexels);
    printEntry("stexels.bin", &stexels);
    printEntry("mappings.bin", &mappings);
    printEntry("/MENU.BSP", &menuBsp);
    printf("[ASSETPAK] Normalized lookup '/MENU.BSP' -> hash=%08x size=%u OK\n",
           (unsigned int)menuBsp.nameHash,
           (unsigned int)menuBsp.size);

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
    printf("[ASSETPAK] Closed FULL pack heap8=%u deltaFromOpenStart=%d\n",
           (unsigned int)heapAfterClose,
           (int)heapBeforeOpen - (int)heapAfterClose);

    if (heapAfterClose != heapBeforeOpen) {
        printf("[ASSETPAK] FAILED full-pack open/lookup/read cycle leaked heap\n");
        return 0;
    }

    printf("[ASSETPAK] READY complete ZIP mirrored as directly seekable ESP32 pack\n");
    printf("[ASSETPAK] READY random-access real wall texture read with 2048B working set\n");
    printf("[ASSETPAK] No full-pack index allocation, ZIP inflate, or monolithic mediaTexels executed\n");

    nativePackReady = 1;
    return 1;
}
