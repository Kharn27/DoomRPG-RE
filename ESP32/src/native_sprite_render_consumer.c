#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "native_graphics_resource_manager.h"
#include "native_sprite_render_consumer.h"
#include "platform_video_config.h"

/* Keep ESP-IDF headers after DoomRPG.h: stdbool false/true macros collide
 * with the engine's legacy boolean enum. */
#include <esp_heap_caps.h>

#define TEST_SPRITE_INDEX 172
#define TEST_SPRITE_EXPECTED_PACKED_BYTES 1600U
#define TEST_SPRITE_EXPECTED_TEXEL_FNV1A 0x0c0a7acdU

static uint32_t heap8Free(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8Block(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
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

static uint16_t spriteTexelColor(Render_t* render,
                                 const EspNativeSpriteFrame* frame,
                                 uint32_t activePixelIndex) {
    uint8_t packed = frame->texels[activePixelIndex >> 1];
    int paletteIndex = (activePixelIndex & 1U) != 0
                           ? (packed >> 4)
                           : (packed & 0x0F);
    return (uint16_t)render->mediaPalettes[frame->paletteOffset + paletteIndex];
}

static void putPixel(Render_t* render, int x, int y, uint16_t color) {
    uint16_t* framebuffer;
    int pitchPixels;

    if (render == NULL || render->framebuffer == NULL ||
        x < 0 || y < 0 ||
        x >= DOOMRPG_LOGICAL_WIDTH || y >= DOOMRPG_LOGICAL_HEIGHT) {
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
        const uint8_t* columnMask =
            frame->mask + ((uint32_t)x * (uint32_t)frame->pitch);

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
    printf("[SPRITERENDER] Begin sprite=%d heap8=%u largest8=%u shapeData=%p mediaTexels=%p backend=GFXRM\n",
           TEST_SPRITE_INDEX,
           (unsigned int)heapBefore,
           (unsigned int)largestBefore,
           (void*)render->shapeData,
           (void*)render->mediaTexels);

    memset(&frame, 0, sizeof(frame));
    if (!EspNativeGraphics_loadSpriteFrame(render, TEST_SPRITE_INDEX, &frame)) {
        printf("[SPRITERENDER] FAILED GFXRM sprite frame load\n");
        return 0;
    }

    printf("[SPRITERENDER] Loaded sprite=%d sourceOffset=%u paletteOffset=%d bounds=%d..%d,%d..%d size=%dx%d\n",
           frame.spriteIndex,
           (unsigned int)frame.sourceOffset,
           frame.paletteOffset,
           frame.xMin,
           frame.xMax,
           frame.yMin,
           frame.yMax,
           frame.width,
           frame.height);
    printf("[SPRITERENDER] Source mask=%uB active=%u texelOffset=%u stexelsByteOffset=%u packed=%uB storage=%uB\n",
           (unsigned int)frame.maskBytes,
           (unsigned int)frame.activePixels,
           (unsigned int)frame.texelOffset,
           (unsigned int)frame.stexelsReadOffset,
           (unsigned int)frame.packedBytes,
           (unsigned int)frame.storageBytes);
    printf("[SPRITERENDER] Texel fnv1a=%08x expected=%08x\n",
           (unsigned int)frame.texelHash,
           (unsigned int)TEST_SPRITE_EXPECTED_TEXEL_FNV1A);

    if (frame.packedBytes != TEST_SPRITE_EXPECTED_PACKED_BYTES ||
        frame.texelHash != TEST_SPRITE_EXPECTED_TEXEL_FNV1A) {
        printf("[SPRITERENDER] FAILED validated sprite-172 payload changed\n");
        EspNativeGraphics_releaseSpriteFrame(&frame);
        return 0;
    }

    heapWithFrame = heap8Free();
    largestWithFrame = largest8Block();
    printf("[SPRITERENDER] Frame resident after manager load heap8=%u largest8=%u used=%uB logicalStorage=%uB\n",
           (unsigned int)heapWithFrame,
           (unsigned int)largestWithFrame,
           (unsigned int)(heapBefore >= heapWithFrame ? heapBefore - heapWithFrame : 0),
           (unsigned int)frame.storageBytes);

    originX = (DOOMRPG_LOGICAL_WIDTH - frame.width) / 2;
    originY = (DOOMRPG_LOGICAL_HEIGHT - frame.height) / 2;
    drawBackground(render, originX, originY, frame.width, frame.height);

    if (!rasterizeFrame(render, &frame, originX, originY, &drawnPixels)) {
        printf("[SPRITERENDER] FAILED direct native rasterization\n");
        EspNativeGraphics_releaseSpriteFrame(&frame);
        return 0;
    }

    framebufferHash = fnv1a32((const uint8_t*)render->framebuffer,
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
        EspNativeGraphics_releaseSpriteFrame(&frame);
        return 0;
    }

    SDL_RenderPresent(NULL);
    printf("[SPRITERENDER] Presented native sprite on shared 160x120 framebuffer -> CYD 320x240\n");

    EspNativeGraphics_releaseSpriteFrame(&frame);
    heapAfter = heap8Free();
    largestAfter = largest8Block();
    printf("[SPRITERENDER] Released manager frame heap8=%u largest8=%u deltaFromStart=%d\n",
           (unsigned int)heapAfter,
           (unsigned int)largestAfter,
           (int)heapBefore - (int)heapAfter);

    if (heapAfter != heapBefore || largestAfter != largestBefore) {
        printf("[SPRITERENDER] FAILED manager-backed render did not restore starting heap layout\n");
        return 0;
    }

    printf("[SPRITERENDER] READY real sprite rendered through shared GFXRM backend\n");
    printf("[SPRITERENDER] Rasterizer no longer owns SD/pack/bitshape/stexels loading logic\n");
    return 1;
}
