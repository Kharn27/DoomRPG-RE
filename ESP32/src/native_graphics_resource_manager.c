#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "native_graphics_resource_manager.h"

#define BITSHAPE_FILE_HEADER_BYTES 4U
#define BITSHAPE_FIXED_HEADER_BYTES 12U
#define TEXEL_FILE_HEADER_BYTES 4U
#define WTEXELS_HEADER_BYTES 4U
#define PACKED_WALL_TEXTURE_BYTES 2048U
#define WALL_WIDTH 64
#define WALL_HEIGHT 64
#define MEDIA_BITSHAPE_OFFSET_INT_COUNT 1300
#define MEDIA_TEXEL_OFFSET_INT_COUNT 592
#define MAX_COLUMN_MASK_BYTES 32U

static EspNativeGraphicsStats graphicsStats;

static uint32_t readLe32(const uint8_t* data) {
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static uint32_t fnv1a32(const uint8_t* data, uint32_t length) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    for (i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

static void recordLoad(uint32_t logicalBytes, int sprite) {
    if (sprite) {
        graphicsStats.spriteLoads++;
    }
    else {
        graphicsStats.wallLoads++;
    }

    graphicsStats.logicalBytesLoaded += logicalBytes;
    if (logicalBytes > graphicsStats.peakFrameBytes) {
        graphicsStats.peakFrameBytes = logicalBytes;
    }
}

static int spriteIndexIsValid(int spriteIndex) {
    return spriteIndex >= 0 &&
           (spriteIndex * 2 + 1) < MEDIA_BITSHAPE_OFFSET_INT_COUNT;
}

static int textureIndexIsValid(int textureIndex) {
    return textureIndex >= 0 &&
           (textureIndex * 2 + 1) < MEDIA_TEXEL_OFFSET_INT_COUNT;
}

static int shapeGeometry(const uint8_t header[BITSHAPE_FIXED_HEADER_BYTES],
                         int* outXMin,
                         int* outXMax,
                         int* outYMin,
                         int* outYMax,
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

    if (header == NULL || outXMin == NULL || outXMax == NULL ||
        outYMin == NULL || outYMax == NULL || outWidth == NULL ||
        outHeight == NULL || outPitch == NULL) {
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

    *outXMin = xMin;
    *outXMax = xMax;
    *outYMin = yMin;
    *outYMax = yMax;
    *outWidth = width;
    *outHeight = height;
    *outPitch = pitch;
    return 1;
}

static int countActivePixels(const EspAssetPackEntry* bitshapes,
                             uint32_t sourceOffset,
                             int width,
                             int height,
                             int pitch,
                             uint32_t* outActivePixels) {
    uint8_t columnMask[MAX_COLUMN_MASK_BYTES];
    uint32_t activePixels = 0;
    int x;

    if (bitshapes == NULL || outActivePixels == NULL ||
        width <= 0 || height <= 0 || pitch <= 0 ||
        pitch > (int)sizeof(columnMask)) {
        return 0;
    }

    for (x = 0; x < width; ++x) {
        uint32_t relativeOffset =
            BITSHAPE_FILE_HEADER_BYTES + sourceOffset +
            BITSHAPE_FIXED_HEADER_BYTES + ((uint32_t)x * (uint32_t)pitch);
        int y;

        if (relativeOffset > bitshapes->size ||
            (uint32_t)pitch > bitshapes->size - relativeOffset ||
            !EspAssetPack_readRange(bitshapes,
                                    relativeOffset,
                                    columnMask,
                                    (size_t)pitch)) {
            return 0;
        }

        for (y = 0; y < height; ++y) {
            if ((columnMask[y / 8] & (1U << (y & 7))) != 0) {
                ++activePixels;
            }
        }
    }

    *outActivePixels = activePixels;
    return 1;
}

void EspNativeGraphics_resetStats(void) {
    memset(&graphicsStats, 0, sizeof(graphicsStats));
}

void EspNativeGraphics_getStats(EspNativeGraphicsStats* outStats) {
    if (outStats != NULL) {
        *outStats = graphicsStats;
    }
}

void EspNativeGraphics_releaseSpriteFrame(EspNativeSpriteFrame* frame) {
    if (frame == NULL) {
        return;
    }

    SDL_free(frame->storage);
    memset(frame, 0, sizeof(*frame));
}

void EspNativeGraphics_releaseWallFrame(EspNativeWallFrame* frame) {
    if (frame == NULL) {
        return;
    }

    SDL_free(frame->texels);
    memset(frame, 0, sizeof(*frame));
}

int EspNativeGraphics_loadSpriteFrame(struct Render_s* renderBase,
                                      int spriteIndex,
                                      EspNativeSpriteFrame* frame) {
    Render_t* render = (Render_t*)renderBase;
    EspAssetPackEntry bitshapes;
    EspAssetPackEntry wtexels;
    EspAssetPackEntry stexels;
    uint8_t shapeHeader[BITSHAPE_FIXED_HEADER_BYTES];
    uint8_t wallHeader[TEXEL_FILE_HEADER_BYTES];
    uint8_t spriteHeader[TEXEL_FILE_HEADER_BYTES];
    uint32_t wallDataSize;
    uint32_t spriteDataSize;
    uint32_t spriteBaseTexelOffset;
    uint32_t relativeTexelOffset;
    uint32_t maskOffset;
    uint32_t maskEnd;
    int sourceOffset;
    int paletteOffset;
    int packOpen = 0;

    if (render == NULL || frame == NULL ||
        render->mediaBitShapeOffsets == NULL || render->mediaPalettes == NULL ||
        !spriteIndexIsValid(spriteIndex)) {
        return 0;
    }

    memset(frame, 0, sizeof(*frame));
    sourceOffset = render->mediaBitShapeOffsets[spriteIndex * 2];
    paletteOffset = render->mediaBitShapeOffsets[spriteIndex * 2 + 1];

    if (sourceOffset < 0 || paletteOffset < 0 ||
        paletteOffset + 15 >= render->mediaPalettesLength) {
        printf("[GFXRM] FAILED sprite mapping id=%d source=%d palette=%d entries=%d\n",
               spriteIndex,
               sourceOffset,
               paletteOffset,
               render->mediaPalettesLength);
        return 0;
    }

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        printf("[GFXRM] FAILED opening %s for sprite=%d\n",
               ESP_ASSET_PACK_DEFAULT_PATH,
               spriteIndex);
        return 0;
    }
    packOpen = 1;
    graphicsStats.packOpenCycles++;

    if (!EspAssetPack_findEntry("bitshapes.bin", &bitshapes) ||
        !EspAssetPack_findEntry("wtexels.bin", &wtexels) ||
        !EspAssetPack_findEntry("stexels.bin", &stexels)) {
        printf("[GFXRM] FAILED locating sprite sources\n");
        goto fail;
    }

    if ((uint32_t)sourceOffset > bitshapes.size ||
        BITSHAPE_FILE_HEADER_BYTES + (uint32_t)sourceOffset > bitshapes.size ||
        BITSHAPE_FIXED_HEADER_BYTES >
            bitshapes.size - (BITSHAPE_FILE_HEADER_BYTES + (uint32_t)sourceOffset) ||
        !EspAssetPack_readRange(&bitshapes,
                                BITSHAPE_FILE_HEADER_BYTES + (uint32_t)sourceOffset,
                                shapeHeader,
                                sizeof(shapeHeader))) {
        printf("[GFXRM] FAILED reading sprite shape header id=%d\n", spriteIndex);
        goto fail;
    }

    frame->spriteIndex = spriteIndex;
    frame->paletteOffset = paletteOffset;
    frame->sourceOffset = (uint32_t)sourceOffset;
    frame->texelOffset = readLe32(shapeHeader);

    if (!shapeGeometry(shapeHeader,
                       &frame->xMin,
                       &frame->xMax,
                       &frame->yMin,
                       &frame->yMax,
                       &frame->width,
                       &frame->height,
                       &frame->pitch)) {
        printf("[GFXRM] FAILED invalid sprite geometry id=%d\n", spriteIndex);
        goto fail;
    }

    frame->maskBytes = (uint32_t)frame->width * (uint32_t)frame->pitch;
    maskOffset = BITSHAPE_FILE_HEADER_BYTES + frame->sourceOffset +
                 BITSHAPE_FIXED_HEADER_BYTES;
    maskEnd = maskOffset + frame->maskBytes;
    if (maskEnd < maskOffset || maskEnd > bitshapes.size) {
        printf("[GFXRM] FAILED sprite mask range id=%d offset=%u bytes=%u\n",
               spriteIndex,
               (unsigned int)maskOffset,
               (unsigned int)frame->maskBytes);
        goto fail;
    }

    if (!countActivePixels(&bitshapes,
                           frame->sourceOffset,
                           frame->width,
                           frame->height,
                           frame->pitch,
                           &frame->activePixels)) {
        printf("[GFXRM] FAILED counting sprite pixels id=%d\n", spriteIndex);
        goto fail;
    }

    frame->packedBytes = ((frame->activePixels + 1U) & ~1U) / 2U;
    if (frame->packedBytes == 0) {
        printf("[GFXRM] FAILED empty sprite payload id=%d\n", spriteIndex);
        goto fail;
    }

    if (!EspAssetPack_readRange(&wtexels, 0, wallHeader, sizeof(wallHeader)) ||
        !EspAssetPack_readRange(&stexels, 0, spriteHeader, sizeof(spriteHeader))) {
        printf("[GFXRM] FAILED reading texel source headers\n");
        goto fail;
    }

    wallDataSize = readLe32(wallHeader);
    spriteDataSize = readLe32(spriteHeader);
    if (wallDataSize > UINT32_MAX / 2U) {
        printf("[GFXRM] FAILED sprite logical base overflow\n");
        goto fail;
    }

    spriteBaseTexelOffset = wallDataSize * 2U;
    if (frame->texelOffset < spriteBaseTexelOffset ||
        ((frame->texelOffset - spriteBaseTexelOffset) & 1U) != 0) {
        printf("[GFXRM] FAILED sprite texel offset id=%d offset=%u base=%u\n",
               spriteIndex,
               (unsigned int)frame->texelOffset,
               (unsigned int)spriteBaseTexelOffset);
        goto fail;
    }

    relativeTexelOffset = frame->texelOffset - spriteBaseTexelOffset;
    frame->stexelsReadOffset = TEXEL_FILE_HEADER_BYTES + relativeTexelOffset / 2U;

    if (spriteDataSize + TEXEL_FILE_HEADER_BYTES != stexels.size ||
        frame->stexelsReadOffset > stexels.size ||
        frame->packedBytes > stexels.size - frame->stexelsReadOffset) {
        printf("[GFXRM] FAILED sprite texel range id=%d offset=%u bytes=%u\n",
               spriteIndex,
               (unsigned int)frame->stexelsReadOffset,
               (unsigned int)frame->packedBytes);
        goto fail;
    }

    if (frame->maskBytes > UINT32_MAX - frame->packedBytes) {
        printf("[GFXRM] FAILED sprite frame storage overflow\n");
        goto fail;
    }

    frame->storageBytes = frame->maskBytes + frame->packedBytes;
    frame->storage = (uint8_t*)SDL_malloc(frame->storageBytes);
    if (frame->storage == NULL) {
        printf("[GFXRM] FAILED allocating sprite frame=%uB\n",
               (unsigned int)frame->storageBytes);
        goto fail;
    }

    frame->mask = frame->storage;
    frame->texels = frame->storage + frame->maskBytes;

    if (!EspAssetPack_readRange(&bitshapes,
                                maskOffset,
                                frame->mask,
                                frame->maskBytes) ||
        !EspAssetPack_readRange(&stexels,
                                frame->stexelsReadOffset,
                                frame->texels,
                                frame->packedBytes)) {
        printf("[GFXRM] FAILED reading bounded sprite frame id=%d\n", spriteIndex);
        goto fail;
    }

    frame->texelHash = fnv1a32(frame->texels, frame->packedBytes);
    EspAssetPack_close();
    packOpen = 0;

    recordLoad(frame->storageBytes, 1);
    printf("[GFXRM] SPRITE id=%d storage=%uB mask=%uB texels=%uB hash=%08x pack=closed\n",
           spriteIndex,
           (unsigned int)frame->storageBytes,
           (unsigned int)frame->maskBytes,
           (unsigned int)frame->packedBytes,
           (unsigned int)frame->texelHash);
    return 1;

fail:
    if (packOpen) {
        EspAssetPack_close();
    }
    EspNativeGraphics_releaseSpriteFrame(frame);
    return 0;
}

int EspNativeGraphics_loadWallFrame(struct Render_s* renderBase,
                                    int textureIndex,
                                    EspNativeWallFrame* frame) {
    Render_t* render = (Render_t*)renderBase;
    EspAssetPackEntry wtexels;
    uint8_t header[WTEXELS_HEADER_BYTES];
    uint32_t sourceDataSize;
    uint32_t sourceByteOffset;
    int sourceTexelOffset;
    int paletteOffset;
    int packOpen = 0;

    if (render == NULL || frame == NULL ||
        render->mediaTexelOffsets == NULL || render->mediaPalettes == NULL ||
        !textureIndexIsValid(textureIndex)) {
        return 0;
    }

    memset(frame, 0, sizeof(*frame));
    sourceTexelOffset = render->mediaTexelOffsets[textureIndex * 2];
    paletteOffset = render->mediaTexelOffsets[textureIndex * 2 + 1];

    if (sourceTexelOffset < 0 || (sourceTexelOffset & 1) != 0 ||
        paletteOffset < 0 || paletteOffset + 15 >= render->mediaPalettesLength) {
        printf("[GFXRM] FAILED wall mapping id=%d texel=%d palette=%d entries=%d\n",
               textureIndex,
               sourceTexelOffset,
               paletteOffset,
               render->mediaPalettesLength);
        return 0;
    }

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        printf("[GFXRM] FAILED opening %s for wall=%d\n",
               ESP_ASSET_PACK_DEFAULT_PATH,
               textureIndex);
        return 0;
    }
    packOpen = 1;
    graphicsStats.packOpenCycles++;

    if (!EspAssetPack_findEntry("wtexels.bin", &wtexels) ||
        !EspAssetPack_readRange(&wtexels, 0, header, sizeof(header))) {
        printf("[GFXRM] FAILED locating/reading wtexels.bin\n");
        goto fail;
    }

    sourceDataSize = readLe32(header);
    if (sourceDataSize + WTEXELS_HEADER_BYTES != wtexels.size) {
        printf("[GFXRM] FAILED wtexels header=%u file=%u\n",
               (unsigned int)sourceDataSize,
               (unsigned int)wtexels.size);
        goto fail;
    }

    frame->textureIndex = textureIndex;
    frame->paletteOffset = paletteOffset;
    frame->width = WALL_WIDTH;
    frame->height = WALL_HEIGHT;
    frame->sourceTexelOffset = (uint32_t)sourceTexelOffset;
    frame->packedBytes = PACKED_WALL_TEXTURE_BYTES;

    sourceByteOffset = frame->sourceTexelOffset / 2U;
    frame->wtexelsReadOffset = WTEXELS_HEADER_BYTES + sourceByteOffset;

    if (frame->wtexelsReadOffset > wtexels.size ||
        frame->packedBytes > wtexels.size - frame->wtexelsReadOffset) {
        printf("[GFXRM] FAILED wall range id=%d offset=%u bytes=%u file=%u\n",
               textureIndex,
               (unsigned int)frame->wtexelsReadOffset,
               (unsigned int)frame->packedBytes,
               (unsigned int)wtexels.size);
        goto fail;
    }

    frame->texels = (uint8_t*)SDL_malloc(frame->packedBytes);
    if (frame->texels == NULL) {
        printf("[GFXRM] FAILED allocating wall frame=%uB\n",
               (unsigned int)frame->packedBytes);
        goto fail;
    }

    if (!EspAssetPack_readRange(&wtexels,
                                frame->wtexelsReadOffset,
                                frame->texels,
                                frame->packedBytes)) {
        printf("[GFXRM] FAILED reading bounded wall frame id=%d\n", textureIndex);
        goto fail;
    }

    frame->texelHash = fnv1a32(frame->texels, frame->packedBytes);
    EspAssetPack_close();
    packOpen = 0;

    recordLoad(frame->packedBytes, 0);
    printf("[GFXRM] WALL id=%d storage=%uB hash=%08x pack=closed\n",
           textureIndex,
           (unsigned int)frame->packedBytes,
           (unsigned int)frame->texelHash);
    return 1;

fail:
    if (packOpen) {
        EspAssetPack_close();
    }
    EspNativeGraphics_releaseWallFrame(frame);
    return 0;
}
