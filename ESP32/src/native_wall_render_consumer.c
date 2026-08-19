#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "native_graphics_resource_manager.h"
#include "native_wall_render_consumer.h"
#include "platform_video_config.h"

/* Keep ESP-IDF headers after DoomRPG.h: stdbool false/true macros collide
 * with the engine's legacy boolean enum. */
#include <esp_heap_caps.h>

#define WALL_WIDTH 64
#define WALL_HEIGHT 64
#define TEST_TEXTURE_INDEX 112
#define TEST_TEXTURE_EXPECTED_SOURCE_TEXEL_OFFSET 65536U
#define TEST_TEXTURE_EXPECTED_FNV1A 0x92d40704U

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

static int selectedTextureContains(Render_t* render, int textureIndex) {
    int i;

    if (render == NULL || render->mapTextureTexels == NULL) {
        return 0;
    }

    for (i = 0; i < render->mapTextureTexelsCount; ++i) {
        if (render->mapTextureTexels[i] == textureIndex) {
            return 1;
        }
    }
    return 0;
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

static void drawBackground(Render_t* render, int originX, int originY) {
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

    for (x = -1; x <= WALL_WIDTH; ++x) {
        putPixel(render, originX + x, originY - 1, 0xFFFF);
        putPixel(render, originX + x, originY + WALL_HEIGHT, 0xFFFF);
    }
    for (y = -1; y <= WALL_HEIGHT; ++y) {
        putPixel(render, originX - 1, originY + y, 0xFFFF);
        putPixel(render, originX + WALL_WIDTH, originY + y, 0xFFFF);
    }
}

static uint16_t wallTexelColor(Render_t* render,
                               const EspNativeWallFrame* frame,
                               int x,
                               int y) {
    uint32_t texelIndex = ((uint32_t)x << 6) + (uint32_t)y;
    uint8_t packed = frame->texels[texelIndex >> 1];
    int paletteIndex = (texelIndex & 1U) != 0
                           ? (packed >> 4)
                           : (packed & 0x0F);
    return (uint16_t)render->mediaPalettes[frame->paletteOffset + paletteIndex];
}

static int rasterizeFrame(Render_t* render,
                          const EspNativeWallFrame* frame,
                          int originX,
                          int originY,
                          uint32_t* outDrawnPixels) {
    uint32_t drawn = 0;
    int x;
    int y;

    if (render == NULL || frame == NULL || outDrawnPixels == NULL ||
        render->framebuffer == NULL || render->mediaPalettes == NULL ||
        frame->texels == NULL || frame->paletteOffset < 0 ||
        frame->paletteOffset + 15 >= render->mediaPalettesLength ||
        frame->width != WALL_WIDTH || frame->height != WALL_HEIGHT) {
        return 0;
    }

    for (x = 0; x < WALL_WIDTH; ++x) {
        for (y = 0; y < WALL_HEIGHT; ++y) {
            putPixel(render,
                     originX + x,
                     originY + y,
                     wallTexelColor(render, frame, x, y));
            ++drawn;
        }
    }

    *outDrawnPixels = drawn;
    return drawn == (uint32_t)(WALL_WIDTH * WALL_HEIGHT);
}

int DoomRPG_probeNativeWallRenderConsumer(struct Render_s* renderBase) {
    Render_t* render = (Render_t*)renderBase;
    EspNativeWallFrame frame;
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

    printf("\n=== Doom RPG ESP32-native wall render consumer ===\n");

    if (render == NULL || render->framebuffer == NULL ||
        render->pitch != DOOMRPG_LOGICAL_WIDTH * (int)sizeof(uint16_t)) {
        printf("[WALLRENDER] FAILED shared 160x120 framebuffer contract unavailable\n");
        return 0;
    }

    if (render->shapeData != NULL || render->mediaTexels != NULL) {
        printf("[WALLRENDER] FAILED legacy graphics pools unexpectedly resident shapeData=%p mediaTexels=%p\n",
               (void*)render->shapeData,
               (void*)render->mediaTexels);
        return 0;
    }

    if (!selectedTextureContains(render, TEST_TEXTURE_INDEX)) {
        printf("[WALLRENDER] FAILED validated test texture=%d is no longer selected by menu.bsp\n",
               TEST_TEXTURE_INDEX);
        return 0;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();
    printf("[WALLRENDER] Begin texture=%d heap8=%u largest8=%u shapeData=%p mediaTexels=%p backend=GFXRM\n",
           TEST_TEXTURE_INDEX,
           (unsigned int)heapBefore,
           (unsigned int)largestBefore,
           (void*)render->shapeData,
           (void*)render->mediaTexels);

    memset(&frame, 0, sizeof(frame));
    if (!EspNativeGraphics_loadWallFrame(render, TEST_TEXTURE_INDEX, &frame)) {
        printf("[WALLRENDER] FAILED GFXRM wall frame load\n");
        return 0;
    }

    printf("[WALLRENDER] Loaded texture=%d texelOffset=%u byteOffset=%u paletteOffset=%d packed=%uB\n",
           frame.textureIndex,
           (unsigned int)frame.sourceTexelOffset,
           (unsigned int)(frame.wtexelsReadOffset - 4U),
           frame.paletteOffset,
           (unsigned int)frame.packedBytes);
    printf("[WALLRENDER] Texel fnv1a=%08x expected=%08x first=%02x%02x%02x%02x last=%02x%02x%02x%02x\n",
           (unsigned int)frame.texelHash,
           (unsigned int)TEST_TEXTURE_EXPECTED_FNV1A,
           frame.texels[0], frame.texels[1], frame.texels[2], frame.texels[3],
           frame.texels[frame.packedBytes - 4U],
           frame.texels[frame.packedBytes - 3U],
           frame.texels[frame.packedBytes - 2U],
           frame.texels[frame.packedBytes - 1U]);

    if (frame.sourceTexelOffset != TEST_TEXTURE_EXPECTED_SOURCE_TEXEL_OFFSET ||
        frame.texelHash != TEST_TEXTURE_EXPECTED_FNV1A) {
        printf("[WALLRENDER] FAILED validated texture-112 source changed\n");
        EspNativeGraphics_releaseWallFrame(&frame);
        return 0;
    }

    heapWithFrame = heap8Free();
    largestWithFrame = largest8Block();
    printf("[WALLRENDER] Frame resident after manager load heap8=%u largest8=%u used=%uB logicalStorage=%uB\n",
           (unsigned int)heapWithFrame,
           (unsigned int)largestWithFrame,
           (unsigned int)(heapBefore >= heapWithFrame ? heapBefore - heapWithFrame : 0),
           (unsigned int)frame.packedBytes);

    originX = (DOOMRPG_LOGICAL_WIDTH - WALL_WIDTH) / 2;
    originY = (DOOMRPG_LOGICAL_HEIGHT - WALL_HEIGHT) / 2;
    drawBackground(render, originX, originY);

    if (!rasterizeFrame(render, &frame, originX, originY, &drawnPixels)) {
        printf("[WALLRENDER] FAILED direct native wall rasterization\n");
        EspNativeGraphics_releaseWallFrame(&frame);
        return 0;
    }

    framebufferHash = fnv1a32((const uint8_t*)render->framebuffer,
                              (uint32_t)render->pitch * DOOMRPG_LOGICAL_HEIGHT);
    printf("[WALLRENDER] DRAW texture=%d origin=%d,%d drawn=%u paletteOffset=%d framebufferFNV=%08x\n",
           TEST_TEXTURE_INDEX,
           originX,
           originY,
           (unsigned int)drawnPixels,
           frame.paletteOffset,
           (unsigned int)framebufferHash);

    SDL_RenderPresent(NULL);
    printf("[WALLRENDER] Presented native wall texture on shared 160x120 framebuffer -> CYD 320x240\n");

    EspNativeGraphics_releaseWallFrame(&frame);
    heapAfter = heap8Free();
    largestAfter = largest8Block();
    printf("[WALLRENDER] Released manager frame heap8=%u largest8=%u deltaFromStart=%d\n",
           (unsigned int)heapAfter,
           (unsigned int)largestAfter,
           (int)heapBefore - (int)heapAfter);

    if (heapAfter != heapBefore || largestAfter != largestBefore) {
        printf("[WALLRENDER] FAILED manager-backed render did not restore starting heap layout\n");
        return 0;
    }

    printf("[WALLRENDER] READY real wall texture rendered through shared GFXRM backend\n");
    printf("[WALLRENDER] Rasterizer no longer owns SD/pack/wtexels loading logic\n");
    return 1;
}
