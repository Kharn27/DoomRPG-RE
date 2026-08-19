#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "native_sprite_render_consumer.h"
#include "platform_video_config.h"

/* Keep ESP-IDF headers after DoomRPG.h: stdbool false/true macros collide
 * with the engine's legacy boolean enum. */
#include <esp_heap_caps.h>

#define BITSHAPE_FILE_HEADER_BYTES 4U
#define BITSHAPE_FIXED_HEADER_BYTES 12U
#define TEXEL_FILE_HEADER_BYTES 4U
#define MEDIA_BITSHAPE_OFFSET_INT_COUNT 1300
#define MAX_COLUMN_MASK_BYTES 32U

/* The previous hardware probe proved this is the largest packed payload among
 * the 284 sprite references selected by menu.bsp. It is deliberately used for
 * the first visual consumer because it exercises the measured worst case.
 */
#define TEST_SPRITE_INDEX 172
#define TEST_SPRITE_EXPECTED_PACKED_BYTES 1600U
#define TEST_SPRITE_EXPECTED_TEXEL_FNV1A 0x0c0a7acdU

typedef struct EspNativeSpriteFrame_s {
    int spriteIndex;
    int paletteOffset;
    int xMin;
    int xMax;
    int yMin;
    int yMax;
    int width;
    int height;
    int pitch;
    uint32_t sourceOffset;
    uint32_t texelOffset;
    uint32_t stexelsReadOffset;
    uint32_t maskBytes;
    uint32_t activePixels;
    uint32_t packedBytes;
    uint32_t storageBytes;
    byte* storage;
    byte* mask;
    byte* texels;
} EspNativeSpriteFrame;

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

static int spriteIndexIsValid(int spriteIndex) {
    return spriteIndex >= 0 &&
           (spriteIndex * 2 + 1) < MEDIA_BITSHAPE_OFFSET_INT_COUNT;
}

static int selectedSpriteContains(Render_t* render, int spriteIndex) {
    int i;

    if (render == NULL || render->mapSpriteTexels == NULL) {
        return 0;
    }

    for (i = 0; i < render->mapSpriteTexelsCount; ++i) {
        if (render->mapSpriteTexels[i] == spriteIndex) {
            return 1;
        }
    }
    return 0;
}

static int shapeGeometry(const byte header[BITSHAPE_FIXED_HEADER_BYTES],
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
    byte columnMask[MAX_COLUMN_MASK_BYTES];
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

static void freeFrame(EspNativeSpriteFrame* frame) {
    if (frame == NULL) {
        return;
    }

    SDL_free(frame->storage);
    memset(frame, 0, sizeof(*frame));
}

static int loadFrame(Render_t* render,
                     int spriteIndex,
                     EspNativeSpriteFrame* frame) {
    EspAssetPackEntry bitshapes;
    EspAssetPackEntry wtexels;
    EspAssetPackEntry stexels;
    byte shapeHeader[BITSHAPE_FIXED_HEADER_BYTES];
    byte wallHeader[TEXEL_FILE_HEADER_BYTES];
    byte spriteHeader[TEXEL_FILE_HEADER_BYTES];
    uint32_t wallDataSize;
    uint32_t spriteDataSize;
    uint32_t spriteBaseTexelOffset;
    uint32_t relativeTexelOffset;
    uint32_t maskOffset;
    uint32_t maskEnd;
    uint32_t texelHash;
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
        printf("[SPRITERENDER] FAILED invalid mapping sprite=%d sourceOffset=%d paletteOffset=%d paletteEntries=%d\n",
               spriteIndex,
               sourceOffset,
               paletteOffset,
               render->mediaPalettesLength);
        return 0;
    }

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        printf("[SPRITERENDER] FAILED opening %s\n", ESP_ASSET_PACK_DEFAULT_PATH);
        return 0;
    }
    packOpen = 1;

    if (!EspAssetPack_findEntry("bitshapes.bin", &bitshapes) ||
        !EspAssetPack_findEntry("wtexels.bin", &wtexels) ||
        !EspAssetPack_findEntry("stexels.bin", &stexels)) {
        printf("[SPRITERENDER] FAILED locating native sprite resources\n");
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
        printf("[SPRITERENDER] FAILED reading shape header sprite=%d sourceOffset=%d\n",
               spriteIndex, sourceOffset);
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
        printf("[SPRITERENDER] FAILED invalid shape geometry sprite=%d\n", spriteIndex);
        goto fail;
    }

    frame->maskBytes = (uint32_t)frame->width * (uint32_t)frame->pitch;
    maskOffset = BITSHAPE_FILE_HEADER_BYTES + frame->sourceOffset +
                 BITSHAPE_FIXED_HEADER_BYTES;
    maskEnd = maskOffset + frame->maskBytes;
    if (maskEnd < maskOffset || maskEnd > bitshapes.size) {
        printf("[SPRITERENDER] FAILED shape mask range sprite=%d offset=%u bytes=%u\n",
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
        printf("[SPRITERENDER] FAILED counting active pixels sprite=%d\n", spriteIndex);
        goto fail;
    }

    frame->packedBytes = ((frame->activePixels + 1U) & ~1U) / 2U;
    if (frame->packedBytes == 0) {
        printf("[SPRITERENDER] FAILED empty packed payload sprite=%d\n", spriteIndex);
        goto fail;
    }

    if (!EspAssetPack_readRange(&wtexels, 0, wallHeader, sizeof(wallHeader)) ||
        !EspAssetPack_readRange(&stexels, 0, spriteHeader, sizeof(spriteHeader))) {
        printf("[SPRITERENDER] FAILED reading texel source headers\n");
        goto fail;
    }

    wallDataSize = readLe32(wallHeader);
    spriteDataSize = readLe32(spriteHeader);
    if (wallDataSize > UINT32_MAX / 2U) {
        printf("[SPRITERENDER] FAILED wall texel logical base overflow\n");
        goto fail;
    }

    spriteBaseTexelOffset = wallDataSize * 2U;
    if (frame->texelOffset < spriteBaseTexelOffset ||
        ((frame->texelOffset - spriteBaseTexelOffset) & 1U) != 0) {
        printf("[SPRITERENDER] FAILED invalid sprite texel offset=%u base=%u\n",
               (unsigned int)frame->texelOffset,
               (unsigned int)spriteBaseTexelOffset);
        goto fail;
    }

    relativeTexelOffset = frame->texelOffset - spriteBaseTexelOffset;
    frame->stexelsReadOffset = TEXEL_FILE_HEADER_BYTES + relativeTexelOffset / 2U;

    if (spriteDataSize + TEXEL_FILE_HEADER_BYTES != stexels.size ||
        frame->stexelsReadOffset > stexels.size ||
        frame->packedBytes > stexels.size - frame->stexelsReadOffset) {
        printf("[SPRITERENDER] FAILED stexels range sprite=%d offset=%u bytes=%u data=%u file=%u\n",
               spriteIndex,
               (unsigned int)frame->stexelsReadOffset,
               (unsigned int)frame->packedBytes,
               (unsigned int)spriteDataSize,
               (unsigned int)stexels.size);
        goto fail;
    }

    if (frame->maskBytes > UINT32_MAX - frame->packedBytes) {
        printf("[SPRITERENDER] FAILED frame storage overflow\n");
        goto fail;
    }

    frame->storageBytes = frame->maskBytes + frame->packedBytes;
    frame->storage = (byte*)SDL_malloc(frame->storageBytes);
    if (frame->storage == NULL) {
        printf("[SPRITERENDER] FAILED allocating frame storage=%uB\n",
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
        printf("[SPRITERENDER] FAILED loading bounded sprite frame\n");
        goto fail;
    }

    texelHash = fnv1a32(frame->texels, frame->packedBytes);
    printf("[SPRITERENDER] Loaded sprite=%d sourceOffset=%u paletteOffset=%d bounds=%d..%d,%d..%d size=%dx%d\n",
           frame->spriteIndex,
           (unsigned int)frame->sourceOffset,
           frame->paletteOffset,
           frame->xMin,
           frame->xMax,
           frame->yMin,
           frame->yMax,
           frame->width,
           frame->height);
    printf("[SPRITERENDER] Source mask=%uB active=%u texelOffset=%u stexelsByteOffset=%u packed=%uB storage=%uB\n",
           (unsigned int)frame->maskBytes,
           (unsigned int)frame->activePixels,
           (unsigned int)frame->texelOffset,
           (unsigned int)frame->stexelsReadOffset,
           (unsigned int)frame->packedBytes,
           (unsigned int)frame->storageBytes);
    printf("[SPRITERENDER] Texel fnv1a=%08x expected=%08x\n",
           (unsigned int)texelHash,
           (unsigned int)TEST_SPRITE_EXPECTED_TEXEL_FNV1A);

    if (spriteIndex == TEST_SPRITE_INDEX &&
        (frame->packedBytes != TEST_SPRITE_EXPECTED_PACKED_BYTES ||
         texelHash != TEST_SPRITE_EXPECTED_TEXEL_FNV1A)) {
        printf("[SPRITERENDER] FAILED validated sprite-172 payload changed\n");
        goto fail;
    }

    EspAssetPack_close();
    return 1;

fail:
    if (packOpen) {
        EspAssetPack_close();
    }
    freeFrame(frame);
    return 0;
}

static uint16_t spriteTexelColor(Render_t* render,
                                 const EspNativeSpriteFrame* frame,
                                 uint32_t activePixelIndex) {
    byte packed;
    int paletteIndex;

    packed = frame->texels[activePixelIndex >> 1];
    paletteIndex = (activePixelIndex & 1U) != 0 ? (packed >> 4) : (packed & 0x0F);
    return (uint16_t)render->mediaPalettes[frame->paletteOffset + paletteIndex];
}

static void putPixel(Render_t* render, int x, int y, uint16_t color) {
    uint16_t* framebuffer;
    int pitchPixels;

    if (render == NULL || render->framebuffer == NULL ||
        x < 0 || y < 0 || x >= DOOMRPG_LOGICAL_WIDTH || y >= DOOMRPG_LOGICAL_HEIGHT) {
        return;
    }

    pitchPixels = render->pitch >> 1;
    framebuffer = (uint16_t*)render->framebuffer;
    framebuffer[y * pitchPixels + x] = color;
}

static void drawBackground(Render_t* render,
                           int originX,
                           int originY,
                           int width,
                           int height) {
    uint16_t* framebuffer;
    int pitchPixels;
    int x;
    int y;

    framebuffer = (uint16_t*)render->framebuffer;
    pitchPixels = render->pitch >> 1;

    for (y = 0; y < DOOMRPG_LOGICAL_HEIGHT; ++y) {
        for (x = 0; x < DOOMRPG_LOGICAL_WIDTH; ++x) {
            framebuffer[y * pitchPixels + x] = 0x0000;
        }
    }

    /* A small checkerboard under the sprite makes transparent mask holes
     * visible on hardware without introducing another asset dependency.
     */
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            int screenX = originX + x;
            int screenY = originY + y;
            uint16_t color = (((x >> 3) + (y >> 3)) & 1) ? 0x2104 : 0x4208;
            putPixel(render, screenX, screenY, color);
        }
    }

    for (x = -1; x <= width; ++x) {
        putPixel(render, originX + x, originY - 1, 0xFFFF);
        putPixel(render, originX + x, originY + height, 0xFFFF);
    }
    for (y = -1; y <= height; ++y) {
        putPixel(render, originX - 1, originY + y, 0xFFFF);
        putPixel(render, originX + width, originY + y, 0xFFFF);
    }
}

static int rasterizeFrame(Render_t* render,
                          const EspNativeSpriteFrame* frame,
                          int originX,
                          int originY,
                          uint32_t* outDrawnPixels) {
    uint32_t activePixelIndex = 0;
    int x;
    int y;

    if (render == NULL || frame == NULL || outDrawnPixels == NULL ||
        render->framebuffer == NULL || render->mediaPalettes == NULL ||
        frame->mask == NULL || frame->texels == NULL ||
        frame->paletteOffset < 0 ||
        frame->paletteOffset + 15 >= render->mediaPalettesLength) {
        return 0;
    }

    for (x = 0; x < frame->width; ++x) {
        const byte* columnMask = frame->mask + ((uint32_t)x * (uint32_t)frame->pitch);

        for (y = 0; y < frame->height; ++y) {
            if ((columnMask[y / 8] & (1U << (y & 7))) != 0) {
                if (activePixelIndex >= frame->activePixels) {
                    return 0;
                }

                putPixel(render,
                         originX + x,
                         originY + y,
                         spriteTexelColor(render, frame, activePixelIndex));
                ++activePixelIndex;
            }
        }
    }

    if (activePixelIndex != frame->activePixels) {
        return 0;
    }

    *outDrawnPixels = activePixelIndex;
    return 1;
}

int DoomRPG_probeNativeSpriteRenderConsumer(struct Render_s* renderBase) {
    Render_t* render = (Render_t*)renderBase;
    EspNativeSpriteFrame frame;
    uint32_t heapBefore;
    uint32_t largestBefore;
    uint32_t heapWithFrame;
    uint32_t largestWithFrame;
    uint32_t heapAfter;
    uint32_t largestAfter;
    uint32_t drawnPixels = 0;
    uint32_t framebufferHash;
    int originX;
    int originY;

    printf("\n=== Doom RPG ESP32-native sprite render consumer ===\n");

    if (render == NULL || render->framebuffer == NULL ||
        render->pitch != DOOMRPG_LOGICAL_WIDTH * (int)sizeof(uint16_t)) {
        printf("[SPRITERENDER] FAILED shared 160x120 framebuffer contract unavailable\n");
        return 0;
    }

    if (render->shapeData != NULL || render->mediaTexels != NULL) {
        printf("[SPRITERENDER] FAILED legacy graphics pools unexpectedly resident shapeData=%p mediaTexels=%p\n",
               (void*)render->shapeData,
               (void*)render->mediaTexels);
        return 0;
    }

    if (!selectedSpriteContains(render, TEST_SPRITE_INDEX)) {
        printf("[SPRITERENDER] FAILED validated test sprite=%d is no longer selected by menu.bsp\n",
               TEST_SPRITE_INDEX);
        return 0;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();
    printf("[SPRITERENDER] Begin sprite=%d heap8=%u largest8=%u shapeData=%p mediaTexels=%p\n",
           TEST_SPRITE_INDEX,
           (unsigned int)heapBefore,
           (unsigned int)largestBefore,
           (void*)render->shapeData,
           (void*)render->mediaTexels);

    memset(&frame, 0, sizeof(frame));
    if (!loadFrame(render, TEST_SPRITE_INDEX, &frame)) {
        printf("[SPRITERENDER] FAILED native frame load\n");
        return 0;
    }

    heapWithFrame = heap8Free();
    largestWithFrame = largest8Block();
    printf("[SPRITERENDER] Frame resident after pack close heap8=%u largest8=%u used=%uB logicalStorage=%uB\n",
           (unsigned int)heapWithFrame,
           (unsigned int)largestWithFrame,
           (unsigned int)(heapBefore >= heapWithFrame ? heapBefore - heapWithFrame : 0),
           (unsigned int)frame.storageBytes);

    originX = (DOOMRPG_LOGICAL_WIDTH - frame.width) / 2;
    originY = (DOOMRPG_LOGICAL_HEIGHT - frame.height) / 2;
    drawBackground(render, originX, originY, frame.width, frame.height);

    if (!rasterizeFrame(render, &frame, originX, originY, &drawnPixels)) {
        printf("[SPRITERENDER] FAILED direct native rasterization\n");
        freeFrame(&frame);
        return 0;
    }

    framebufferHash = fnv1a32(render->framebuffer,
                              (uint32_t)render->pitch * DOOMRPG_LOGICAL_HEIGHT);
    printf("[SPRITERENDER] DRAW sprite=%d origin=%d,%d drawn=%u paletteOffset=%d framebufferFNV=%08x\n",
           TEST_SPRITE_INDEX,
           originX,
           originY,
           (unsigned int)drawnPixels,
           frame.paletteOffset,
           (unsigned int)framebufferHash);

    if (drawnPixels != frame.activePixels) {
        printf("[SPRITERENDER] FAILED rasterized pixel count mismatch expected=%u actual=%u\n",
               (unsigned int)frame.activePixels,
               (unsigned int)drawnPixels);
        freeFrame(&frame);
        return 0;
    }

    SDL_RenderPresent(NULL);
    printf("[SPRITERENDER] Presented native sprite on shared 160x120 framebuffer -> CYD 320x240\n");

    freeFrame(&frame);
    heapAfter = heap8Free();
    largestAfter = largest8Block();
    printf("[SPRITERENDER] Released frame heap8=%u largest8=%u deltaFromStart=%d\n",
           (unsigned int)heapAfter,
           (unsigned int)largestAfter,
           (int)heapBefore - (int)heapAfter);

    if (heapAfter != heapBefore || largestAfter != largestBefore) {
        printf("[SPRITERENDER] FAILED render consumer did not restore starting heap layout\n");
        return 0;
    }

    printf("[SPRITERENDER] READY real sprite rendered without shapeData or mediaTexels\n");
    printf("[SPRITERENDER] READY native frame contract = source mask + packed texels + existing 16-color palette\n");
    printf("[SPRITERENDER] Runtime cache/eviction policy still intentionally NOT introduced\n");
    return 1;
}
