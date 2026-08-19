#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "native_bitshape_loader.h"

/* Keep ESP-IDF headers after DoomRPG.h: stdbool false/true macros collide
 * with the engine's legacy boolean enum. */
#include <esp_heap_caps.h>

#define BITSHAPE_FILE_HEADER_BYTES 4U
#define BITSHAPE_FIXED_HEADER_BYTES 12U
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

static uint32_t fnv1aUpdate(uint32_t hash, const byte* data, uint32_t length) {
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

static int sortSelectedSpritesBySourceOffset(Render_t* render) {
    int i;
    int j;

    if (render == NULL || render->mapSpriteTexels == NULL ||
        render->mediaBitShapeOffsets == NULL || render->mapSpriteTexelsCount <= 0) {
        return 0;
    }

    for (i = 0; i < render->mapSpriteTexelsCount; ++i) {
        if (!spriteIndexIsValid(render->mapSpriteTexels[i])) {
            return 0;
        }
    }

    /* Keep the selected sprite references in source order, matching the useful
     * part of the original loader, but DO NOT rewrite mediaBitShapeOffsets.
     * On ESP32 those even entries remain source offsets into bitshapes.bin.
     */
    for (i = 0; i < render->mapSpriteTexelsCount - 1; ++i) {
        for (j = 0; j < (render->mapSpriteTexelsCount - 1) - i; ++j) {
            int lhs = render->mapSpriteTexels[j];
            int rhs = render->mapSpriteTexels[j + 1];
            int lhsOffset = render->mediaBitShapeOffsets[lhs * 2];
            int rhsOffset = render->mediaBitShapeOffsets[rhs * 2];

            if (lhsOffset < 0 || rhsOffset < 0) {
                return 0;
            }

            if (rhsOffset < lhsOffset) {
                render->mapSpriteTexels[j] = rhs;
                render->mapSpriteTexels[j + 1] = lhs;
            }
        }
    }

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
                         uint32_t* outLegacyWords,
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
    uint32_t legacyWords;

    if (header == NULL || outLegacyWords == NULL || outWidth == NULL ||
        outHeight == NULL || outPitch == NULL) {
        return 0;
    }

    legacyWords = (uint32_t)readLe16(header + 6);
    xMin = header[8];
    xMax = header[9];
    yMin = header[10];
    yMax = header[11];

    if (legacyWords == 0 || xMax < xMin || yMax < yMin) {
        return 0;
    }

    width = (xMax - xMin) + 1;
    height = (yMax - yMin) + 1;
    pitch = (height + 7) / 8;

    /* Coordinates are bytes in the source format, so a column can never need
     * more than 32 bytes of mask storage (256 vertical pixels / 8).
     */
    if (width <= 0 || width > 256 || height <= 0 || height > 256 ||
        pitch <= 0 || pitch > (int)MAX_COLUMN_MASK_BYTES) {
        return 0;
    }

    *outLegacyWords = legacyWords;
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
                             uint32_t* outActivePixels,
                             uint32_t* ioHash) {
    byte columnMask[MAX_COLUMN_MASK_BYTES];
    uint32_t activePixels = 0;
    int w;

    if (bitshapes == NULL || outActivePixels == NULL || ioHash == NULL ||
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

        *ioHash = fnv1aUpdate(*ioHash, columnMask, (uint32_t)pitch);

        for (h = 0; h < height; ++h) {
            if ((columnMask[h / 8] & (1U << (h & 7))) != 0) {
                ++activePixels;
            }
        }
    }

    *outActivePixels = activePixels;
    return 1;
}

int DoomRPG_loadNativeBitShapes(struct Render_s* renderBase) {
    Render_t* render = (Render_t*)renderBase;
    EspAssetPackEntry bitshapes;
    uint64_t legacyShapeWordsTotal = 0;
    uint64_t selectedMaskBytes = 0;
    uint64_t spritePackedBytes = 0;
    uint64_t activePixelsTotal = 0;
    uint32_t uniqueShapes = 0;
    uint32_t maxWidth = 0;
    uint32_t maxHeight = 0;
    uint32_t maxPitch = 0;
    uint32_t sourceHash = 2166136261U;
    uint32_t heapBeforeOpen;
    uint32_t heapAfterOpen;
    uint32_t heapAfterClose;
    int samplePrinted = 0;
    int i;

    printf("\n=== Doom RPG ESP32-native bitshape loader ===\n");

    if (render == NULL || render->mapSpriteTexels == NULL ||
        render->mapSpriteTexelsCount <= 0 || render->mediaBitShapeOffsets == NULL) {
        printf("[BITSHAPE] Selected sprite references are unavailable\n");
        return 0;
    }

    if (!sortSelectedSpritesBySourceOffset(render)) {
        printf("[BITSHAPE] FAILED validating/sorting selected sprite references\n");
        return 0;
    }

    /* The failed first implementation proved the selected legacy expansion is
     * 55,676 bytes for menu.bsp. On ESP32 shapeData is therefore deliberately
     * eliminated rather than reconstructed. The source offset table loaded from
     * mappings.bin stays authoritative and will feed an on-demand renderer.
     */
    SDL_free(render->shapeData);
    render->shapeData = NULL;

    heapBeforeOpen = heap8Free();
    printf("[BITSHAPE] Begin refs=%d heap8=%u largest8=%u shapeData=%p\n",
           render->mapSpriteTexelsCount,
           (unsigned int)heapBeforeOpen,
           (unsigned int)largest8Block(),
           (void*)render->shapeData);

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        printf("[BITSHAPE] FAILED opening %s\n", ESP_ASSET_PACK_DEFAULT_PATH);
        return 0;
    }

    heapAfterOpen = heap8Free();

    if (!EspAssetPack_findEntry("bitshapes.bin", &bitshapes)) {
        printf("[BITSHAPE] FAILED locating bitshapes.bin in native pack\n");
        EspAssetPack_close();
        return 0;
    }

    printf("[BITSHAPE] Pack open heap8=%u largest8=%u cost=%dB entrySize=%u\n",
           (unsigned int)heapAfterOpen,
           (unsigned int)largest8Block(),
           (int)heapBeforeOpen - (int)heapAfterOpen,
           (unsigned int)bitshapes.size);

    /* Walk one unique source shape at a time. We read its 12-byte fixed header
     * and then at most one mask column (<=32 bytes) at a time. Shared shapes are
     * decoded once and their packed sprite payload size is multiplied by the
     * number of selected sprite IDs referencing that source shape.
     */
    i = 0;
    while (i < render->mapSpriteTexelsCount) {
        byte header[BITSHAPE_FIXED_HEADER_BYTES];
        uint32_t legacyWords;
        uint32_t activePixels;
        uint32_t packedBytesPerRef;
        uint32_t maskBytes;
        uint32_t sourceEnd;
        int spriteIndex = render->mapSpriteTexels[i];
        int sourceOffset;
        int width;
        int height;
        int pitch;
        int groupRefs = 1;
        int j;

        if (!sourceOffsetForSprite(render, spriteIndex, &sourceOffset)) {
            printf("[BITSHAPE] FAILED invalid source offset sprite=%d\n", spriteIndex);
            EspAssetPack_close();
            return 0;
        }

        if (!readShapeHeader(&bitshapes, sourceOffset, header) ||
            !shapeGeometry(header, &legacyWords, &width, &height, &pitch)) {
            printf("[BITSHAPE] FAILED reading shape header sprite=%d sourceOffset=%d\n",
                   spriteIndex, sourceOffset);
            EspAssetPack_close();
            return 0;
        }

        maskBytes = (uint32_t)width * (uint32_t)pitch;
        sourceEnd = BITSHAPE_FILE_HEADER_BYTES + (uint32_t)sourceOffset +
                    BITSHAPE_FIXED_HEADER_BYTES + maskBytes;
        if (sourceEnd > bitshapes.size) {
            printf("[BITSHAPE] FAILED shape mask exceeds bitshapes.bin sprite=%d\n",
                   spriteIndex);
            EspAssetPack_close();
            return 0;
        }

        /* Count consecutive selected sprite IDs sharing this exact source shape. */
        for (j = i + 1; j < render->mapSpriteTexelsCount; ++j) {
            int nextOffset;
            if (!sourceOffsetForSprite(render, render->mapSpriteTexels[j], &nextOffset)) {
                printf("[BITSHAPE] FAILED invalid grouped sprite=%d\n",
                       render->mapSpriteTexels[j]);
                EspAssetPack_close();
                return 0;
            }
            if (nextOffset != sourceOffset) {
                break;
            }
            ++groupRefs;
        }

        sourceHash = fnv1aUpdate(sourceHash, header, BITSHAPE_FIXED_HEADER_BYTES);
        if (!countActivePixels(&bitshapes,
                               sourceOffset,
                               width,
                               height,
                               pitch,
                               &activePixels,
                               &sourceHash)) {
            printf("[BITSHAPE] FAILED reading mask sprite=%d sourceOffset=%d\n",
                   spriteIndex, sourceOffset);
            EspAssetPack_close();
            return 0;
        }

        /* This is exactly Render_getSTexelBufferSize()/2 from the old expanded
         * representation: active pixels rounded up to an even nibble count,
         * then packed two 4-bit texels per byte.
         */
        packedBytesPerRef = ((activePixels + 1U) & ~1U) / 2U;

        legacyShapeWordsTotal += legacyWords;
        selectedMaskBytes += maskBytes;
        activePixelsTotal += (uint64_t)activePixels * (uint64_t)groupRefs;
        spritePackedBytes += (uint64_t)packedBytesPerRef * (uint64_t)groupRefs;

        if ((uint32_t)width > maxWidth) {
            maxWidth = (uint32_t)width;
        }
        if ((uint32_t)height > maxHeight) {
            maxHeight = (uint32_t)height;
        }
        if ((uint32_t)pitch > maxPitch) {
            maxPitch = (uint32_t)pitch;
        }

        if (!samplePrinted) {
            printf("[BITSHAPE] Sample sprite=%d sourceOffset=%d texelOffset=%u bounds=%u..%u,%u..%u mask=%uB active=%u packed=%uB refs=%d\n",
                   spriteIndex,
                   sourceOffset,
                   (unsigned int)readLe32(header),
                   (unsigned int)header[8],
                   (unsigned int)header[9],
                   (unsigned int)header[10],
                   (unsigned int)header[11],
                   (unsigned int)maskBytes,
                   (unsigned int)activePixels,
                   (unsigned int)packedBytesPerRef,
                   groupRefs);
            samplePrinted = 1;
        }

        ++uniqueShapes;
        i += groupRefs;
    }

    printf("[BITSHAPE] Selected unique=%u refs=%d max=%ux%u pitch=%uB\n",
           (unsigned int)uniqueShapes,
           render->mapSpriteTexelsCount,
           (unsigned int)maxWidth,
           (unsigned int)maxHeight,
           (unsigned int)maxPitch);
    printf("[BITSHAPE] Legacy expanded shapeData=%lluB (%llu words) -> ESP32 resident=0B\n",
           (unsigned long long)(legacyShapeWordsTotal * sizeof(short)),
           (unsigned long long)legacyShapeWordsTotal);
    printf("[BITSHAPE] Selected source masks=%lluB, decoded one column at a time scratch<=%uB\n",
           (unsigned long long)selectedMaskBytes,
           (unsigned int)MAX_COLUMN_MASK_BYTES);
    printf("[BITSHAPE] Exact selected sprite texels=%lluB packed across %d refs activePixels=%llu\n",
           (unsigned long long)spritePackedBytes,
           render->mapSpriteTexelsCount,
           (unsigned long long)activePixelsTotal);
    printf("[BITSHAPE] Source walk fnv1a=%08x mappings remain source offsets shapeData=%p\n",
           (unsigned int)sourceHash,
           (void*)render->shapeData);

    EspAssetPack_close();
    heapAfterClose = heap8Free();

    printf("[BITSHAPE] Pack closed heap8=%u largest8=%u deltaFromStart=%d\n",
           (unsigned int)heapAfterClose,
           (unsigned int)largest8Block(),
           (int)heapBeforeOpen - (int)heapAfterClose);

    if (heapAfterClose != heapBeforeOpen) {
        printf("[BITSHAPE] FAILED on-demand source walk changed persistent heap\n");
        return 0;
    }

    printf("[BITSHAPE] READY on-demand bitshape source model validated; legacy shapeData eliminated\n");
    printf("[BITSHAPE] Renderer integration will consume source header/mask directly from bounded cache\n");
    printf("[BITSHAPE] Render_loadTexels / monolithic mediaTexels still intentionally NOT executed\n");
    return 1;
}
