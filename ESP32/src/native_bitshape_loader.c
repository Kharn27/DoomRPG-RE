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

    /* Preserve the exact ordering contract expected by the old loader, but do
     * it without touching the source offset table yet.
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
                         uint32_t* outWords,
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
    uint32_t words;

    if (header == NULL || outWords == NULL || outWidth == NULL ||
        outHeight == NULL || outPitch == NULL) {
        return 0;
    }

    words = (uint32_t)readLe16(header + 6);
    xMin = header[8];
    xMax = header[9];
    yMin = header[10];
    yMax = header[11];

    if (words == 0 || xMax < xMin || yMax < yMin) {
        return 0;
    }

    width = (xMax - xMin) + 1;
    height = (yMax - yMin) + 1;
    pitch = (height + 7) / 8;

    if (width <= 0 || height <= 0 || pitch <= 0 ||
        pitch > (int)MAX_COLUMN_MASK_BYTES) {
        return 0;
    }

    *outWords = words;
    *outWidth = width;
    *outHeight = height;
    *outPitch = pitch;
    return 1;
}

int DoomRPG_loadNativeBitShapes(struct Render_s* renderBase) {
    Render_t* render = (Render_t*)renderBase;
    EspAssetPackEntry bitshapes;
    uint64_t shapeWordsTotal = 0;
    uint64_t spritePackedBytes = 0;
    uint32_t uniqueShapes = 0;
    uint32_t maxWidth = 0;
    uint32_t maxHeight = 0;
    uint32_t maxPitch = 0;
    uint32_t heapBeforeOpen;
    uint32_t heapAfterOpen;
    uint32_t heapAfterShapeAlloc;
    uint32_t heapAfterClose;
    uint32_t shapeDataBytes;
    uint32_t shapeHash;
    int previousSourceOffset = -1;
    int previousShapeOffset = 0;
    int shapeOffset = 0;
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

    heapBeforeOpen = heap8Free();
    printf("[BITSHAPE] Begin refs=%d heap8=%u largest8=%u\n",
           render->mapSpriteTexelsCount,
           (unsigned int)heapBeforeOpen,
           (unsigned int)largest8Block());

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

    /* Pass 1: only 12 bytes are read for each unique selected shape. This
     * derives the exact resident shapeData size before allocating anything.
     */
    for (i = 0; i < render->mapSpriteTexelsCount; ++i) {
        byte header[BITSHAPE_FIXED_HEADER_BYTES];
        uint32_t shapeWords;
        int width;
        int height;
        int pitch;
        int sourceOffset;
        uint32_t maskBytes;
        uint32_t sourceEnd;

        if (!sourceOffsetForSprite(render, render->mapSpriteTexels[i], &sourceOffset)) {
            printf("[BITSHAPE] FAILED invalid source offset for sprite=%d\n",
                   render->mapSpriteTexels[i]);
            EspAssetPack_close();
            return 0;
        }

        if (sourceOffset == previousSourceOffset) {
            continue;
        }

        if (!readShapeHeader(&bitshapes, sourceOffset, header) ||
            !shapeGeometry(header, &shapeWords, &width, &height, &pitch)) {
            printf("[BITSHAPE] FAILED reading shape header sprite=%d sourceOffset=%d\n",
                   render->mapSpriteTexels[i], sourceOffset);
            EspAssetPack_close();
            return 0;
        }

        maskBytes = (uint32_t)width * (uint32_t)pitch;
        sourceEnd = BITSHAPE_FILE_HEADER_BYTES + (uint32_t)sourceOffset +
                    BITSHAPE_FIXED_HEADER_BYTES + maskBytes;
        if (sourceEnd > bitshapes.size) {
            printf("[BITSHAPE] FAILED shape mask exceeds bitshapes.bin sprite=%d\n",
                   render->mapSpriteTexels[i]);
            EspAssetPack_close();
            return 0;
        }

        shapeWordsTotal += shapeWords;
        if (shapeWordsTotal > (uint64_t)UINT32_MAX / sizeof(short)) {
            printf("[BITSHAPE] FAILED selected shapeData size overflow\n");
            EspAssetPack_close();
            return 0;
        }

        if ((uint32_t)width > maxWidth) {
            maxWidth = (uint32_t)width;
        }
        if ((uint32_t)height > maxHeight) {
            maxHeight = (uint32_t)height;
        }
        if ((uint32_t)pitch > maxPitch) {
            maxPitch = (uint32_t)pitch;
        }

        ++uniqueShapes;
        previousSourceOffset = sourceOffset;
    }

    shapeDataBytes = (uint32_t)(shapeWordsTotal * sizeof(short));
    printf("[BITSHAPE] Preflight unique=%u shapeWords=%llu shapeData=%uB max=%ux%u pitch=%uB\n",
           (unsigned int)uniqueShapes,
           (unsigned long long)shapeWordsTotal,
           (unsigned int)shapeDataBytes,
           (unsigned int)maxWidth,
           (unsigned int)maxHeight,
           (unsigned int)maxPitch);
    printf("[BITSHAPE] Allocation preflight heap8=%u largest8=%u fitAggregate=%s fitContiguous=%s\n",
           (unsigned int)heap8Free(),
           (unsigned int)largest8Block(),
           shapeDataBytes <= heap8Free() ? "yes" : "NO",
           shapeDataBytes <= largest8Block() ? "yes" : "NO");

    if (shapeDataBytes == 0 || shapeDataBytes > heap8Free() ||
        shapeDataBytes > largest8Block()) {
        printf("[BITSHAPE] REFUSED exact selected shapeData does not fit while pack is open\n");
        EspAssetPack_close();
        return 0;
    }

    SDL_free(render->shapeData);
    render->shapeData = (short*)SDL_malloc(shapeDataBytes);
    if (render->shapeData == NULL) {
        printf("[BITSHAPE] FAILED allocating exact shapeData=%uB\n",
               (unsigned int)shapeDataBytes);
        EspAssetPack_close();
        return 0;
    }

    heapAfterShapeAlloc = heap8Free();
    printf("[BITSHAPE] shapeData resident ptr=%p heap8=%u largest8=%u used=%uB\n",
           (void*)render->shapeData,
           (unsigned int)heapAfterShapeAlloc,
           (unsigned int)largest8Block(),
           (unsigned int)(heapAfterOpen >= heapAfterShapeAlloc
                              ? heapAfterOpen - heapAfterShapeAlloc
                              : 0));

    /* Pass 2: rebuild the exact structure consumed by the existing renderer.
     * Only one column mask (maximum 32 bytes) is ever read at a time.
     */
    previousSourceOffset = -1;
    previousShapeOffset = 0;
    shapeOffset = 0;

    for (i = 0; i < render->mapSpriteTexelsCount; ++i) {
        byte header[BITSHAPE_FIXED_HEADER_BYTES];
        byte columnMask[MAX_COLUMN_MASK_BYTES];
        uint32_t expectedShapeWords;
        int width;
        int height;
        int pitch;
        int spriteIndex = render->mapSpriteTexels[i];
        int sourceOffset;
        int shapeStart;
        int xMin;
        int xMax;
        int yMin;
        int w;

        if (!sourceOffsetForSprite(render, spriteIndex, &sourceOffset)) {
            printf("[BITSHAPE] FAILED source offset disappeared sprite=%d\n", spriteIndex);
            EspAssetPack_close();
            return 0;
        }

        if (sourceOffset == previousSourceOffset) {
            render->mediaBitShapeOffsets[spriteIndex * 2] = previousShapeOffset;
            continue;
        }

        if (!readShapeHeader(&bitshapes, sourceOffset, header) ||
            !shapeGeometry(header, &expectedShapeWords, &width, &height, &pitch)) {
            printf("[BITSHAPE] FAILED second-pass header sprite=%d sourceOffset=%d\n",
                   spriteIndex, sourceOffset);
            EspAssetPack_close();
            return 0;
        }

        shapeStart = shapeOffset;
        previousShapeOffset = shapeStart;
        previousSourceOffset = sourceOffset;
        xMin = header[8];
        xMax = header[9];
        yMin = header[10];

        if ((uint32_t)(shapeOffset + 4 + width) > shapeWordsTotal) {
            printf("[BITSHAPE] FAILED shapeData header/index overflow sprite=%d\n", spriteIndex);
            EspAssetPack_close();
            return 0;
        }

        render->shapeData[shapeOffset++] = (short)readLe16(header + 0);
        render->shapeData[shapeOffset++] = (short)readLe16(header + 2);
        render->shapeData[shapeOffset++] = (short)xMin;
        render->shapeData[shapeOffset++] = (short)xMax;

        shapeOffset += width;

        {
            int activePixelOffset = 0;

            for (w = 0; w < width; ++w) {
                uint32_t maskRelativeOffset =
                    BITSHAPE_FILE_HEADER_BYTES + (uint32_t)sourceOffset +
                    BITSHAPE_FIXED_HEADER_BYTES + ((uint32_t)w * (uint32_t)pitch);
                int h = 0;

                render->shapeData[shapeStart + w + 4] =
                    (short)(shapeOffset - shapeStart - 2);

                if (!EspAssetPack_readRange(&bitshapes,
                                            maskRelativeOffset,
                                            columnMask,
                                            (size_t)pitch)) {
                    printf("[BITSHAPE] FAILED reading column mask sprite=%d column=%d\n",
                           spriteIndex, w);
                    EspAssetPack_close();
                    return 0;
                }

                while (h < height) {
                    int runStart;
                    int runPixelOffset;

                    while (h < height &&
                           (columnMask[h / 8] & (1U << (h & 7))) == 0) {
                        ++h;
                    }
                    if (h == height) {
                        break;
                    }

                    runStart = h;
                    runPixelOffset = activePixelOffset;
                    while (h < height &&
                           (columnMask[h / 8] & (1U << (h & 7))) != 0) {
                        ++activePixelOffset;
                        ++h;
                    }

                    if ((uint32_t)(shapeOffset + 2) > shapeWordsTotal) {
                        printf("[BITSHAPE] FAILED shapeData run overflow sprite=%d\n",
                               spriteIndex);
                        EspAssetPack_close();
                        return 0;
                    }

                    render->shapeData[shapeOffset++] =
                        (short)(((runStart + yMin) & 0xFF) |
                                (((h - runStart) & 0xFF) << 8));
                    render->shapeData[shapeOffset++] = (short)runPixelOffset;
                }

                if ((uint32_t)(shapeOffset + 1) > shapeWordsTotal) {
                    printf("[BITSHAPE] FAILED shapeData terminator overflow sprite=%d\n",
                           spriteIndex);
                    EspAssetPack_close();
                    return 0;
                }
                render->shapeData[shapeOffset++] = 127;
            }
        }

        if ((uint32_t)(shapeOffset - shapeStart) != expectedShapeWords) {
            printf("[BITSHAPE] FAILED generated words sprite=%d expected=%u actual=%u\n",
                   spriteIndex,
                   (unsigned int)expectedShapeWords,
                   (unsigned int)(shapeOffset - shapeStart));
            EspAssetPack_close();
            return 0;
        }

        render->mediaBitShapeOffsets[spriteIndex * 2] = shapeStart;
    }

    if ((uint64_t)shapeOffset != shapeWordsTotal) {
        printf("[BITSHAPE] FAILED final shape words expected=%llu actual=%d\n",
               (unsigned long long)shapeWordsTotal, shapeOffset);
        EspAssetPack_close();
        return 0;
    }

    EspAssetPack_close();
    heapAfterClose = heap8Free();
    render->bitShapeMemory =
        (int)(heapBeforeOpen >= heapAfterClose ? heapBeforeOpen - heapAfterClose : 0);

    shapeHash = fnv1a32((const byte*)render->shapeData, shapeDataBytes);

    for (i = 0; i < render->mapSpriteTexelsCount; ++i) {
        int bytes = Render_getSTexelBufferSize(render, render->mapSpriteTexels[i]);
        if (bytes < 0) {
            printf("[BITSHAPE] FAILED negative sprite texel size sprite=%d\n",
                   render->mapSpriteTexels[i]);
            return 0;
        }
        spritePackedBytes += (uint64_t)bytes / 2U;
    }

    printf("[BITSHAPE] Built exact shapeData words=%d bytes=%u fnv1a=%08x\n",
           shapeOffset,
           (unsigned int)shapeDataBytes,
           (unsigned int)shapeHash);
    printf("[BITSHAPE] Exact selected sprite packed texels=%lluB across %d refs\n",
           (unsigned long long)spritePackedBytes,
           render->mapSpriteTexelsCount);
    printf("[BITSHAPE] Pack closed heap8=%u largest8=%u persistentCost=%dB\n",
           (unsigned int)heapAfterClose,
           (unsigned int)largest8Block(),
           render->bitShapeMemory);
    printf("[BITSHAPE] READY native selected bitshapes resident; bitshapes.bin never materialized\n");
    printf("[BITSHAPE] Render_getSTexelBufferSize validated against rebuilt shapeData\n");
    printf("[BITSHAPE] Render_loadTexels / mediaTexels still intentionally NOT executed\n");

    return 1;
}
