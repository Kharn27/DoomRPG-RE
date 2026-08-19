#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "native_sprite_texel_probe.h"

/* Keep ESP-IDF headers after DoomRPG.h: stdbool false/true macros collide
 * with the engine's legacy boolean enum. */
#include <esp_heap_caps.h>

#define BITSHAPE_FILE_HEADER_BYTES 4U
#define BITSHAPE_FIXED_HEADER_BYTES 12U
#define TEXEL_FILE_HEADER_BYTES 4U
#define MEDIA_BITSHAPE_OFFSET_INT_COUNT 1300
#define MAX_COLUMN_MASK_BYTES 32U

static uint32_t heap8Free(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8Block(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static uint16_t readLe16(const byte* data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
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

static int spriteIndexIsValid(int spriteIndex) {
    return spriteIndex >= 0 &&
           (spriteIndex * 2 + 1) < MEDIA_BITSHAPE_OFFSET_INT_COUNT;
}

static int sourceOffsetForSprite(Render_t* render, int spriteIndex, int* outOffset) {
    int offset;

    if (render == NULL || outOffset == NULL ||
        render->mediaBitShapeOffsets == NULL || !spriteIndexIsValid(spriteIndex)) {
        return 0;
    }

    offset = render->mediaBitShapeOffsets[spriteIndex * 2];
    if (offset < 0) {
        return 0;
    }

    *outOffset = offset;
    return 1;
}

static int readShapeHeader(const EspAssetPackEntry* bitshapes,
                           int sourceOffset,
                           byte header[BITSHAPE_FIXED_HEADER_BYTES]) {
    uint32_t relativeOffset;

    if (bitshapes == NULL || header == NULL || sourceOffset < 0) {
        return 0;
    }

    relativeOffset = BITSHAPE_FILE_HEADER_BYTES + (uint32_t)sourceOffset;
    if (relativeOffset > bitshapes->size ||
        BITSHAPE_FIXED_HEADER_BYTES > bitshapes->size - relativeOffset) {
        return 0;
    }

    return EspAssetPack_readRange(bitshapes,
                                  relativeOffset,
                                  header,
                                  BITSHAPE_FIXED_HEADER_BYTES);
}

static int shapeGeometry(const byte header[BITSHAPE_FIXED_HEADER_BYTES],
                         int* outWidth,
                         int* outHeight,
                         int* outPitch) {
    int xMin;
    int xMax;
    int yMin;
    int yMax;
    int width;
    int height;
    int pitch;

    if (header == NULL || outWidth == NULL || outHeight == NULL || outPitch == NULL) {
        return 0;
    }

    /* A zero legacy word count means this is not a valid bitshape record. */
    if (readLe16(header + 6) == 0) {
        return 0;
    }

    xMin = header[8];
    xMax = header[9];
    yMin = header[10];
    yMax = header[11];
    if (xMax < xMin || yMax < yMin) {
        return 0;
    }

    width = (xMax - xMin) + 1;
    height = (yMax - yMin) + 1;
    pitch = (height + 7) / 8;

    if (width <= 0 || width > 256 || height <= 0 || height > 256 ||
        pitch <= 0 || pitch > (int)MAX_COLUMN_MASK_BYTES) {
        return 0;
    }

    *outWidth = width;
    *outHeight = height;
    *outPitch = pitch;
    return 1;
}

static int countActivePixels(const EspAssetPackEntry* bitshapes,
                             int sourceOffset,
                             int width,
                             int height,
                             int pitch,
                             uint32_t* outActivePixels) {
    byte columnMask[MAX_COLUMN_MASK_BYTES];
    uint32_t activePixels = 0;
    int w;

    if (bitshapes == NULL || outActivePixels == NULL ||
        sourceOffset < 0 || width <= 0 || height <= 0 ||
        pitch <= 0 || pitch > (int)sizeof(columnMask)) {
        return 0;
    }

    for (w = 0; w < width; ++w) {
        uint32_t relativeOffset =
            BITSHAPE_FILE_HEADER_BYTES + (uint32_t)sourceOffset +
            BITSHAPE_FIXED_HEADER_BYTES + ((uint32_t)w * (uint32_t)pitch);
        int h;

        if (relativeOffset > bitshapes->size ||
            (uint32_t)pitch > bitshapes->size - relativeOffset ||
            !EspAssetPack_readRange(bitshapes,
                                    relativeOffset,
                                    columnMask,
                                    (size_t)pitch)) {
            return 0;
        }

        for (h = 0; h < height; ++h) {
            if ((columnMask[h / 8] & (1U << (h & 7))) != 0) {
                ++activePixels;
            }
        }
    }

    *outActivePixels = activePixels;
    return 1;
}

int DoomRPG_probeNativeSpriteTexels(struct Render_s* renderBase) {
    Render_t* render = (Render_t*)renderBase;
    EspAssetPackEntry bitshapes;
    EspAssetPackEntry wtexels;
    EspAssetPackEntry stexels;
    byte wallHeader[TEXEL_FILE_HEADER_BYTES];
    byte spriteHeader[TEXEL_FILE_HEADER_BYTES];
    byte* spriteData = NULL;
    uint64_t totalPackedBytes = 0;
    uint64_t totalActivePixels = 0;
    uint32_t uniqueShapes = 0;
    uint32_t wallDataSize;
    uint32_t spriteFileDataSize;
    uint32_t spriteBaseTexelOffset;
    uint32_t maxPackedBytes = 0;
    uint32_t maxActivePixels = 0;
    uint32_t maxTexelOffset = 0;
    uint32_t maxReadOffset = 0;
    uint32_t maxSourceOffset = 0;
    uint32_t heapBeforeOpen;
    uint32_t largestBeforeOpen;
    uint32_t heapAfterOpen;
    uint32_t heapBeforePayload;
    uint32_t largestBeforePayload;
    uint32_t heapWithPayload;
    uint32_t heapAfterPayload;
    uint32_t heapAfterClose;
    uint32_t largestAfterClose;
    uint32_t payloadHash;
    int maxSpriteIndex = -1;
    int maxWidth = 0;
    int maxHeight = 0;
    int maxGroupRefs = 0;
    int i;

    printf("\n=== Doom RPG ESP32-native sprite texel probe ===\n");

    if (render == NULL || render->mapSpriteTexels == NULL ||
        render->mapSpriteTexelsCount <= 0 || render->mediaBitShapeOffsets == NULL) {
        printf("[SPRITETEX] Selected sprite references are unavailable\n");
        return 0;
    }

    if (render->shapeData != NULL) {
        printf("[SPRITETEX] FAILED legacy shapeData unexpectedly resident at %p\n",
               (void*)render->shapeData);
        return 0;
    }

    if (render->mediaTexels != NULL) {
        printf("[SPRITETEX] FAILED monolithic mediaTexels unexpectedly resident at %p\n",
               (void*)render->mediaTexels);
        return 0;
    }

    heapBeforeOpen = heap8Free();
    largestBeforeOpen = largest8Block();
    printf("[SPRITETEX] Begin refs=%d heap8=%u largest8=%u shapeData=%p mediaTexels=%p\n",
           render->mapSpriteTexelsCount,
           (unsigned int)heapBeforeOpen,
           (unsigned int)largestBeforeOpen,
           (void*)render->shapeData,
           (void*)render->mediaTexels);

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        printf("[SPRITETEX] FAILED opening %s\n", ESP_ASSET_PACK_DEFAULT_PATH);
        return 0;
    }

    heapAfterOpen = heap8Free();
    if (!EspAssetPack_findEntry("bitshapes.bin", &bitshapes) ||
        !EspAssetPack_findEntry("wtexels.bin", &wtexels) ||
        !EspAssetPack_findEntry("stexels.bin", &stexels)) {
        printf("[SPRITETEX] FAILED locating bitshape/texel resources in native pack\n");
        EspAssetPack_close();
        return 0;
    }

    if (!EspAssetPack_readRange(&wtexels, 0, wallHeader, sizeof(wallHeader)) ||
        !EspAssetPack_readRange(&stexels, 0, spriteHeader, sizeof(spriteHeader))) {
        printf("[SPRITETEX] FAILED reading texel source headers\n");
        EspAssetPack_close();
        return 0;
    }

    wallDataSize = readLe32(wallHeader);
    spriteFileDataSize = readLe32(spriteHeader);
    if (wallDataSize > UINT32_MAX / 2U) {
        printf("[SPRITETEX] FAILED wall texel logical base overflow\n");
        EspAssetPack_close();
        return 0;
    }
    spriteBaseTexelOffset = wallDataSize * 2U;

    printf("[SPRITETEX] Pack open heap8=%u largest8=%u cost=%dB\n",
           (unsigned int)heapAfterOpen,
           (unsigned int)largest8Block(),
           (int)heapBeforeOpen - (int)heapAfterOpen);
    printf("[SPRITETEX] Source bases wallData=%uB spriteBaseTexel=%u stexelsHeader=%u stexelsSize=%uB\n",
           (unsigned int)wallDataSize,
           (unsigned int)spriteBaseTexelOffset,
           (unsigned int)spriteFileDataSize,
           (unsigned int)stexels.size);

    /* mapSpriteTexels was sorted by DoomRPG_loadNativeBitShapes(). Walk one
     * unique source shape at a time, derive its exact packed payload size, and
     * prove that its texel range is directly addressable inside stexels.bin.
     */
    i = 0;
    while (i < render->mapSpriteTexelsCount) {
        byte header[BITSHAPE_FIXED_HEADER_BYTES];
        uint32_t activePixels;
        uint32_t packedBytes;
        uint32_t texelOffset;
        uint32_t relativeTexelOffset;
        uint32_t readOffset;
        int spriteIndex = render->mapSpriteTexels[i];
        int sourceOffset;
        int width;
        int height;
        int pitch;
        int groupRefs = 1;
        int j;

        if (!sourceOffsetForSprite(render, spriteIndex, &sourceOffset) ||
            !readShapeHeader(&bitshapes, sourceOffset, header) ||
            !shapeGeometry(header, &width, &height, &pitch)) {
            printf("[SPRITETEX] FAILED resolving bitshape sprite=%d\n", spriteIndex);
            EspAssetPack_close();
            return 0;
        }

        for (j = i + 1; j < render->mapSpriteTexelsCount; ++j) {
            int nextOffset;
            if (!sourceOffsetForSprite(render, render->mapSpriteTexels[j], &nextOffset)) {
                printf("[SPRITETEX] FAILED resolving grouped sprite=%d\n",
                       render->mapSpriteTexels[j]);
                EspAssetPack_close();
                return 0;
            }
            if (nextOffset != sourceOffset) {
                break;
            }
            ++groupRefs;
        }

        if (!countActivePixels(&bitshapes,
                               sourceOffset,
                               width,
                               height,
                               pitch,
                               &activePixels)) {
            printf("[SPRITETEX] FAILED decoding source mask sprite=%d\n", spriteIndex);
            EspAssetPack_close();
            return 0;
        }

        packedBytes = ((activePixels + 1U) & ~1U) / 2U;
        texelOffset = readLe32(header);
        if (packedBytes == 0 || texelOffset < spriteBaseTexelOffset ||
            ((texelOffset - spriteBaseTexelOffset) & 1U) != 0) {
            printf("[SPRITETEX] FAILED invalid texel range sprite=%d texelOffset=%u packed=%u\n",
                   spriteIndex,
                   (unsigned int)texelOffset,
                   (unsigned int)packedBytes);
            EspAssetPack_close();
            return 0;
        }

        relativeTexelOffset = texelOffset - spriteBaseTexelOffset;
        readOffset = TEXEL_FILE_HEADER_BYTES + (relativeTexelOffset / 2U);
        if (readOffset > stexels.size || packedBytes > stexels.size - readOffset) {
            printf("[SPRITETEX] FAILED stexels range sprite=%d offset=%u bytes=%u size=%u\n",
                   spriteIndex,
                   (unsigned int)readOffset,
                   (unsigned int)packedBytes,
                   (unsigned int)stexels.size);
            EspAssetPack_close();
            return 0;
        }

        totalActivePixels += (uint64_t)activePixels * (uint64_t)groupRefs;
        totalPackedBytes += (uint64_t)packedBytes * (uint64_t)groupRefs;
        ++uniqueShapes;

        if (packedBytes > maxPackedBytes) {
            maxPackedBytes = packedBytes;
            maxActivePixels = activePixels;
            maxTexelOffset = texelOffset;
            maxReadOffset = readOffset;
            maxSourceOffset = (uint32_t)sourceOffset;
            maxSpriteIndex = spriteIndex;
            maxWidth = width;
            maxHeight = height;
            maxGroupRefs = groupRefs;
        }

        i += groupRefs;
    }

    if (maxSpriteIndex < 0 || maxPackedBytes == 0 || uniqueShapes == 0) {
        printf("[SPRITETEX] FAILED no selected sprite payload discovered\n");
        EspAssetPack_close();
        return 0;
    }

    printf("[SPRITETEX] Selected unique=%u refs=%d packedTotal=%lluB activePixels=%llu\n",
           (unsigned int)uniqueShapes,
           render->mapSpriteTexelsCount,
           (unsigned long long)totalPackedBytes,
           (unsigned long long)totalActivePixels);
    printf("[SPRITETEX] Largest sprite=%d sourceOffset=%u texelOffset=%u stexelsByteOffset=%u bounds=%dx%d active=%u packed=%uB refs=%d\n",
           maxSpriteIndex,
           (unsigned int)maxSourceOffset,
           (unsigned int)maxTexelOffset,
           (unsigned int)maxReadOffset,
           maxWidth,
           maxHeight,
           (unsigned int)maxActivePixels,
           (unsigned int)maxPackedBytes,
           maxGroupRefs);

    heapBeforePayload = heap8Free();
    largestBeforePayload = largest8Block();
    printf("[SPRITETEX] Payload preflight heap8=%u largest8=%u required=%uB fitAggregate=%s fitContiguous=%s\n",
           (unsigned int)heapBeforePayload,
           (unsigned int)largestBeforePayload,
           (unsigned int)maxPackedBytes,
           maxPackedBytes <= heapBeforePayload ? "yes" : "NO",
           maxPackedBytes <= largestBeforePayload ? "yes" : "NO");

    if (maxPackedBytes > heapBeforePayload || maxPackedBytes > largestBeforePayload) {
        printf("[SPRITETEX] REFUSED largest selected sprite payload does not fit bounded buffer\n");
        EspAssetPack_close();
        return 0;
    }

    spriteData = (byte*)SDL_malloc(maxPackedBytes);
    if (spriteData == NULL) {
        printf("[SPRITETEX] FAILED allocating largest sprite payload=%uB\n",
               (unsigned int)maxPackedBytes);
        EspAssetPack_close();
        return 0;
    }

    heapWithPayload = heap8Free();
    printf("[SPRITETEX] Largest payload resident heap8=%u largest8=%u used=%uB\n",
           (unsigned int)heapWithPayload,
           (unsigned int)largest8Block(),
           (unsigned int)(heapBeforePayload >= heapWithPayload
                              ? heapBeforePayload - heapWithPayload
                              : 0));

    if (!EspAssetPack_readRange(&stexels,
                                maxReadOffset,
                                spriteData,
                                maxPackedBytes)) {
        printf("[SPRITETEX] FAILED direct stexels read sprite=%d\n", maxSpriteIndex);
        SDL_free(spriteData);
        EspAssetPack_close();
        return 0;
    }

    payloadHash = fnv1a32(spriteData, maxPackedBytes);
    if (maxPackedBytes >= 4U) {
        printf("[SPRITETEX] READ sprite=%d bytes=%u fnv1a=%08x first=%02x%02x%02x%02x last=%02x%02x%02x%02x\n",
               maxSpriteIndex,
               (unsigned int)maxPackedBytes,
               (unsigned int)payloadHash,
               spriteData[0], spriteData[1], spriteData[2], spriteData[3],
               spriteData[maxPackedBytes - 4U],
               spriteData[maxPackedBytes - 3U],
               spriteData[maxPackedBytes - 2U],
               spriteData[maxPackedBytes - 1U]);
    }
    else {
        printf("[SPRITETEX] READ sprite=%d bytes=%u fnv1a=%08x\n",
               maxSpriteIndex,
               (unsigned int)maxPackedBytes,
               (unsigned int)payloadHash);
    }

    SDL_free(spriteData);
    spriteData = NULL;
    heapAfterPayload = heap8Free();
    printf("[SPRITETEX] Released payload heap8=%u largest8=%u delta=%d\n",
           (unsigned int)heapAfterPayload,
           (unsigned int)largest8Block(),
           (int)heapBeforePayload - (int)heapAfterPayload);

    if (heapAfterPayload != heapBeforePayload ||
        largest8Block() != largestBeforePayload) {
        printf("[SPRITETEX] FAILED bounded sprite read changed heap layout\n");
        EspAssetPack_close();
        return 0;
    }

    EspAssetPack_close();
    heapAfterClose = heap8Free();
    largestAfterClose = largest8Block();
    printf("[SPRITETEX] Pack closed heap8=%u largest8=%u deltaFromStart=%d\n",
           (unsigned int)heapAfterClose,
           (unsigned int)largestAfterClose,
           (int)heapBeforeOpen - (int)heapAfterClose);

    if (heapAfterClose != heapBeforeOpen || largestAfterClose != largestBeforeOpen) {
        printf("[SPRITETEX] FAILED sprite probe did not restore starting heap layout\n");
        return 0;
    }

    printf("[SPRITETEX] READY largest selected sprite payload read directly from stexels.bin\n");
    printf("[SPRITETEX] READY sprite working-set ceiling measured without shapeData or mediaTexels\n");
    printf("[SPRITETEX] Cache size intentionally NOT chosen until hardware result is known\n");
    return 1;
}
