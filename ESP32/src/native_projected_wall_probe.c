#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "native_graphics_resource_manager.h"
#include "native_projected_wall_bridge.h"
#include "native_projected_wall_probe.h"
#include "platform_video_config.h"

/* Keep ESP-IDF headers after DoomRPG.h: stdbool false/true macros collide
 * with the engine's legacy boolean enum. */
#include <esp_heap_caps.h>

#define TEST_TEXTURE_INDEX 112
#define TEST_TEXTURE_EXPECTED_FNV1A 0x92d40704U
#define TEST_WORLD_DEPTH 128
#define TEST_WORLD_HALF_WIDTH 32
#define TEST_WORLD_TEXTURE_LENGTH 64
#define EXPECTED_VIEWPORT_WIDTH 160
#define EXPECTED_VIEWPORT_HEIGHT 80
#define EXPECTED_PROJECTED_COLUMNS 40U
#define EXPECTED_PROJECTED_PIXELS 1600U

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

int DoomRPG_probeProjectedWallGfxrm(struct Render_s* renderBase) {
    Render_t* render = (Render_t*)renderBase;
    Line_t line;
    EspNativeProjectedWallStats projectedStats;
    EspNativeGraphicsStats gfxStats;
    uint32_t heapBefore;
    uint32_t largestBefore;
    uint32_t heapAfter;
    uint32_t largestAfter;
    uint32_t framebufferHash;
    int originalLineFlags;
    int projectedStart;
    int projectedEnd;

    int oldViewX;
    int oldViewY;
    int oldViewZ;
    int oldViewCos_;
    int oldViewSin_;
    int oldViewTransX;
    int oldViewSin;
    int oldViewCos;
    int oldViewTransY;
    int oldScreenLeft;
    int oldScreenTop;
    int oldScreenRight;
    int oldScreenBottom;
    int oldLineRasterCount;
    int oldNumLines;
    byte oldSpanMode;
    boolean oldDamageBlend;
    boolean oldSkipStretch;
    short* oldPixels;
    span_t oldSpanFunction;
    unsigned short* oldSpanPalettes;

    printf("\n=== Doom RPG ESP32 projected wall via GFXRM ===\n");

    if (render == NULL || render->framebuffer == NULL ||
        render->columnScale == NULL || render->lines == NULL ||
        render->linesLength <= 0 || render->mediaPalettes == NULL) {
        printf("[PROJWALL] FAILED renderer runtime contract unavailable\n");
        return 0;
    }

    if (render->shapeData != NULL || render->mediaTexels != NULL) {
        printf("[PROJWALL] FAILED legacy graphics pools unexpectedly resident shapeData=%p mediaTexels=%p\n",
               (void*)render->shapeData,
               (void*)render->mediaTexels);
        return 0;
    }

    if (render->screenWidth != EXPECTED_VIEWPORT_WIDTH ||
        render->screenHeight != EXPECTED_VIEWPORT_HEIGHT ||
        render->pitch != DOOMRPG_LOGICAL_WIDTH * (int)sizeof(uint16_t)) {
        printf("[PROJWALL] FAILED viewport=%dx%d pitch=%d expected=%dx%d pitch=%u\n",
               render->screenWidth,
               render->screenHeight,
               render->pitch,
               EXPECTED_VIEWPORT_WIDTH,
               EXPECTED_VIEWPORT_HEIGHT,
               (unsigned int)(DOOMRPG_LOGICAL_WIDTH * sizeof(uint16_t)));
        return 0;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();
    printf("[PROJWALL] Begin heap8=%u largest8=%u viewport=%dx%d@%d,%d texture=%d shapeData=%p mediaTexels=%p\n",
           (unsigned int)heapBefore,
           (unsigned int)largestBefore,
           render->screenWidth,
           render->screenHeight,
           render->screenX,
           render->screenY,
           TEST_TEXTURE_INDEX,
           (void*)render->shapeData,
           (void*)render->mediaTexels);

    oldViewX = render->viewX;
    oldViewY = render->viewY;
    oldViewZ = render->viewZ;
    oldViewCos_ = render->viewCos_;
    oldViewSin_ = render->viewSin_;
    oldViewTransX = render->viewTransX;
    oldViewSin = render->viewSin;
    oldViewCos = render->viewCos;
    oldViewTransY = render->viewTransY;
    oldScreenLeft = render->screenLeft;
    oldScreenTop = render->screenTop;
    oldScreenRight = render->screenRight;
    oldScreenBottom = render->screenBottom;
    oldLineRasterCount = render->lineRasterCount;
    oldNumLines = render->numLines;
    oldSpanMode = render->spanMode;
    oldDamageBlend = render->damageBlend;
    oldSkipStretch = render->skipStretch;
    oldPixels = render->pixels;
    oldSpanFunction = render->spanFunction;
    oldSpanPalettes = render->spanPalettes;
    originalLineFlags = render->lines[0].flags;

    memset(render->framebuffer, 0,
           (size_t)render->pitch * DOOMRPG_LOGICAL_HEIGHT);

    render->viewX = 0;
    render->viewY = 0;
    render->viewZ = 32;
    render->viewCos_ = 65536;
    render->viewSin_ = 0;
    render->viewTransX = 0;
    render->viewSin = 0;
    render->viewCos = 65536;
    render->viewTransY = 0;

    render->screenLeft = 0;
    render->screenTop = 0;
    render->screenRight = render->screenWidth;
    render->screenBottom = render->screenHeight;
    render->pixels = (short*)&render->framebuffer[
        (render->pitch * render->screenY) +
        (render->screenX * (int)sizeof(short))];
    render->spanMode = 0;
    render->damageBlend = false;
    render->skipStretch = false;
    render->lineRasterCount = 0;
    render->numLines = 0;
    Render_initColumnScale(render);

    memset(&line, 0, sizeof(line));
    line.vert1.x = TEST_WORLD_DEPTH;
    line.vert1.y = -TEST_WORLD_HALF_WIDTH;
    line.vert1.z = 0;
    line.vert2.x = TEST_WORLD_DEPTH;
    line.vert2.y = TEST_WORLD_HALF_WIDTH;
    line.vert2.z = TEST_WORLD_TEXTURE_LENGTH;
    line.texture = TEST_TEXTURE_INDEX;
    line.flags = 0;

    EspNativeGraphics_resetStats();
    EspNativeProjectedWall_resetStats();

    printf("[PROJWALL] WORLD v1=(%d,%d,z%d) v2=(%d,%d,z%d) camera=(0,0,z32) spanMode=0\n",
           line.vert1.x, line.vert1.y, line.vert1.z,
           line.vert2.x, line.vert2.y, line.vert2.z);
    printf("[PROJWALL] -> Render_drawLines -> transform -> clip -> project -> Render_drawWallSpans\n");

    Render_drawLines(render, &line);

    projectedStart = (line.vert1.x + 65535) >> 16;
    projectedEnd = (line.vert2.x + 65535) >> 16;
    EspNativeProjectedWall_getStats(&projectedStats);
    EspNativeGraphics_getStats(&gfxStats);

    framebufferHash = fnv1a32(render->framebuffer,
                              (uint32_t)render->pitch * DOOMRPG_LOGICAL_HEIGHT);

    printf("[PROJWALL] PROJECTED columns=%d..%d count=%d scale=%d/%d z=%d/%d lineRasterCount=%d\n",
           projectedStart,
           projectedEnd,
           projectedEnd - projectedStart,
           line.vert1.y,
           line.vert2.y,
           line.vert1.z,
           line.vert2.z,
           render->lineRasterCount);
    printf("[PROJWALL] Span stats begin=%u end=%u calls=%u pixels=%u rangeErrors=%u texture=%d texelHash=%08x\n",
           (unsigned int)projectedStats.beginCalls,
           (unsigned int)projectedStats.endCalls,
           (unsigned int)projectedStats.spanCalls,
           (unsigned int)projectedStats.pixelsDrawn,
           (unsigned int)projectedStats.outOfRangeReads,
           projectedStats.lastTextureIndex,
           (unsigned int)projectedStats.lastTexelHash);
    printf("[PROJWALL] GFXRM stats spriteLoads=%u wallLoads=%u packOpenCycles=%u logicalBytes=%u peakFrame=%u\n",
           (unsigned int)gfxStats.spriteLoads,
           (unsigned int)gfxStats.wallLoads,
           (unsigned int)gfxStats.packOpenCycles,
           (unsigned int)gfxStats.logicalBytesLoaded,
           (unsigned int)gfxStats.peakFrameBytes);
    printf("[PROJWALL] framebufferFNV=%08x\n",
           (unsigned int)framebufferHash);

    SDL_RenderPresent(NULL);
    printf("[PROJWALL] Presented projected wall through original Render_drawWallSpans geometry\n");

    render->lines[0].flags = originalLineFlags;
    render->viewX = oldViewX;
    render->viewY = oldViewY;
    render->viewZ = oldViewZ;
    render->viewCos_ = oldViewCos_;
    render->viewSin_ = oldViewSin_;
    render->viewTransX = oldViewTransX;
    render->viewSin = oldViewSin;
    render->viewCos = oldViewCos;
    render->viewTransY = oldViewTransY;
    render->screenLeft = oldScreenLeft;
    render->screenTop = oldScreenTop;
    render->screenRight = oldScreenRight;
    render->screenBottom = oldScreenBottom;
    render->lineRasterCount = oldLineRasterCount;
    render->numLines = oldNumLines;
    render->spanMode = oldSpanMode;
    render->damageBlend = oldDamageBlend;
    render->skipStretch = oldSkipStretch;
    render->pixels = oldPixels;
    render->spanFunction = oldSpanFunction;
    render->spanPalettes = oldSpanPalettes;

    heapAfter = heap8Free();
    largestAfter = largest8Block();
    printf("[PROJWALL] End heap8=%u largest8=%u deltaFromStart=%d\n",
           (unsigned int)heapAfter,
           (unsigned int)largestAfter,
           (int)heapBefore - (int)heapAfter);

    if (projectedStart != 60 || projectedEnd != 100 ||
        projectedStats.beginCalls != 1U || projectedStats.endCalls != 1U ||
        projectedStats.spanCalls != EXPECTED_PROJECTED_COLUMNS ||
        projectedStats.pixelsDrawn != EXPECTED_PROJECTED_PIXELS ||
        projectedStats.outOfRangeReads != 0U ||
        projectedStats.lastTextureIndex != TEST_TEXTURE_INDEX ||
        projectedStats.lastTexelHash != TEST_TEXTURE_EXPECTED_FNV1A ||
        gfxStats.spriteLoads != 0U || gfxStats.wallLoads != 1U ||
        gfxStats.packOpenCycles != 1U || gfxStats.logicalBytesLoaded != 2048U ||
        gfxStats.peakFrameBytes != 2048U ||
        heapAfter != heapBefore || largestAfter != largestBefore) {
        printf("[PROJWALL] FAILED projected wall contract changed\n");
        return 0;
    }

    printf("[PROJWALL] READY original projection + wall span geometry now consumes GFXRM texture data\n");
    printf("[PROJWALL] READY mode0 projected wall rendered with mediaTexels still NULL\n");
    return 1;
}
