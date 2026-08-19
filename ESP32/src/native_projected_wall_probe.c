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
#define TEST_TEXTURE_EXPECTED_SOURCE_OFFSET 65536
#define TEST_TEXTURE_EXPECTED_PALETTE_OFFSET 480
#define TEST_WORLD_DEPTH 128
#define TEST_WORLD_HALF_WIDTH 32
#define TEST_WORLD_TEXTURE_LENGTH 64
#define EXPECTED_VIEWPORT_WIDTH 160
#define EXPECTED_VIEWPORT_HEIGHT 80
#define EXPECTED_PROJECTED_COLUMNS 40U
#define EXPECTED_PROJECTED_PIXELS 1600U
#define EXPECTED_FRAME_ALLOCATOR_COST 2064U

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

static uint16_t chooseSentinel(Render_t* render, int paletteOffset) {
    uint32_t candidate;

    for (candidate = 0xA55AU; candidate <= 0xFFFFU; ++candidate) {
        int i;
        int conflict = 0;
        for (i = 0; i < 16; ++i) {
            if ((uint16_t)render->mediaPalettes[paletteOffset + i] ==
                (uint16_t)candidate) {
                conflict = 1;
                break;
            }
        }
        if (!conflict) {
            return (uint16_t)candidate;
        }
    }

    return 0x1357U;
}

static void fillFramebuffer(Render_t* render, uint16_t color) {
    uint16_t* pixels = (uint16_t*)render->framebuffer;
    uint32_t count =
        ((uint32_t)render->pitch * DOOMRPG_LOGICAL_HEIGHT) / sizeof(uint16_t);
    uint32_t i;

    for (i = 0; i < count; ++i) {
        pixels[i] = color;
    }
}

static uint32_t countChangedPixels(Render_t* render, uint16_t sentinel) {
    const uint16_t* pixels = (const uint16_t*)render->framebuffer;
    uint32_t count =
        ((uint32_t)render->pitch * DOOMRPG_LOGICAL_HEIGHT) / sizeof(uint16_t);
    uint32_t changed = 0;
    uint32_t i;

    for (i = 0; i < count; ++i) {
        if (pixels[i] != sentinel) {
            ++changed;
        }
    }
    return changed;
}

int DoomRPG_probeProjectedWallGfxrm(struct Render_s* renderBase) {
    Render_t* render = (Render_t*)renderBase;
    Line_t line;
    EspNativeProjectedWallStats projectedStats;
    EspNativeGraphicsStats gfxStats;
    uint32_t heapBefore;
    uint32_t largestBefore;
    uint32_t heapBound;
    uint32_t largestBound;
    uint32_t heapAfter;
    uint32_t largestAfter;
    uint32_t framebufferHash;
    uint32_t changedPixels;
    uint16_t sentinel;
    int originalLineFlags;
    int projectedStart;
    int projectedEnd;
    int bridgeBound = 0;

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
        render->linesLength <= 0 || render->mediaPalettes == NULL ||
        render->mediaTexelOffsets == NULL) {
        printf("[PROJWALL] FAILED renderer runtime contract unavailable\n");
        return 0;
    }

    if (render->shapeData != NULL || render->mediaTexels != NULL) {
        printf("[PROJWALL] FAILED starting legacy graphics pools unexpectedly resident shapeData=%p mediaTexels=%p\n",
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

    EspNativeGraphics_resetStats();
    EspNativeProjectedWall_resetStats();

    if (!EspNativeProjectedWall_begin(render, TEST_TEXTURE_INDEX)) {
        printf("[PROJWALL] FAILED bounded compatibility bind\n");
        return 0;
    }
    bridgeBound = 1;

    EspNativeProjectedWall_getStats(&projectedStats);
    heapBound = heap8Free();
    largestBound = largest8Block();
    printf("[PROJWALL] Bound frame heap8=%u largest8=%u used=%uB mediaTexels=%p logicalBound=%uB\n",
           (unsigned int)heapBound,
           (unsigned int)largestBound,
           (unsigned int)(heapBefore >= heapBound ? heapBefore - heapBound : 0),
           (void*)render->mediaTexels,
           (unsigned int)projectedStats.boundBytes);

    sentinel = chooseSentinel(render, projectedStats.lastPaletteOffset);
    fillFramebuffer(render, sentinel);

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

    printf("[PROJWALL] WORLD v1=(%d,%d,z%d) v2=(%d,%d,z%d) camera=(0,0,z32) spanMode=0 sentinel=%04x\n",
           line.vert1.x, line.vert1.y, line.vert1.z,
           line.vert2.x, line.vert2.y, line.vert2.z,
           (unsigned int)sentinel);
    printf("[PROJWALL] -> unchanged Render_drawLines -> transform -> clip -> project -> Render_drawWallSpans -> Render_SpanMode0\n");

    Render_drawLines(render, &line);

    projectedStart = (line.vert1.x + 65535) >> 16;
    projectedEnd = (line.vert2.x + 65535) >> 16;
    changedPixels = countChangedPixels(render, sentinel);
    framebufferHash = fnv1a32(render->framebuffer,
                              (uint32_t)render->pitch * DOOMRPG_LOGICAL_HEIGHT);

    EspNativeProjectedWall_end();
    bridgeBound = 0;
    EspNativeProjectedWall_getStats(&projectedStats);
    EspNativeGraphics_getStats(&gfxStats);

    printf("[PROJWALL] PROJECTED columns=%d..%d count=%d scale=%d/%d z=%d/%d lineRasterCount=%d changedPixels=%u\n",
           projectedStart,
           projectedEnd,
           projectedEnd - projectedStart,
           line.vert1.y,
           line.vert2.y,
           line.vert1.z,
           line.vert2.z,
           render->lineRasterCount,
           (unsigned int)changedPixels);
    printf("[PROJWALL] Bridge stats begin=%u end=%u bound=%uB texture=%d palette=%d originalOffset=%d texelHash=%08x\n",
           (unsigned int)projectedStats.beginCalls,
           (unsigned int)projectedStats.endCalls,
           (unsigned int)projectedStats.boundBytes,
           projectedStats.lastTextureIndex,
           projectedStats.lastPaletteOffset,
           projectedStats.originalTexelOffset,
           (unsigned int)projectedStats.lastTexelHash);
    printf("[PROJWALL] GFXRM stats spriteLoads=%u wallLoads=%u packOpenCycles=%u logicalBytes=%u peakFrame=%u\n",
           (unsigned int)gfxStats.spriteLoads,
           (unsigned int)gfxStats.wallLoads,
           (unsigned int)gfxStats.packOpenCycles,
           (unsigned int)gfxStats.logicalBytesLoaded,
           (unsigned int)gfxStats.peakFrameBytes);
    printf("[PROJWALL] framebufferFNV=%08x mediaTexelsRestored=%p mappingOffsetRestored=%d\n",
           (unsigned int)framebufferHash,
           (void*)render->mediaTexels,
           render->mediaTexelOffsets[TEST_TEXTURE_INDEX * 2]);

    SDL_RenderPresent(NULL);
    printf("[PROJWALL] Presented wall projected by unchanged original wall geometry + SpanMode0\n");

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
    printf("[PROJWALL] Resident largest-block delta=%dB is allocator-placement dependent; final restoration is the contract\n",
           (int)largestBefore - (int)largestBound);

    if (bridgeBound || projectedStart != 60 || projectedEnd != 100 ||
        changedPixels != EXPECTED_PROJECTED_PIXELS ||
        render->mediaTexels != NULL ||
        render->mediaTexelOffsets[TEST_TEXTURE_INDEX * 2] !=
            TEST_TEXTURE_EXPECTED_SOURCE_OFFSET ||
        projectedStats.beginCalls != 1U || projectedStats.endCalls != 1U ||
        projectedStats.boundBytes != 2048U ||
        projectedStats.lastTextureIndex != TEST_TEXTURE_INDEX ||
        projectedStats.lastPaletteOffset != TEST_TEXTURE_EXPECTED_PALETTE_OFFSET ||
        projectedStats.originalTexelOffset != TEST_TEXTURE_EXPECTED_SOURCE_OFFSET ||
        projectedStats.lastTexelHash != TEST_TEXTURE_EXPECTED_FNV1A ||
        gfxStats.spriteLoads != 0U || gfxStats.wallLoads != 1U ||
        gfxStats.packOpenCycles != 1U || gfxStats.logicalBytesLoaded != 2048U ||
        gfxStats.peakFrameBytes != 2048U ||
        heapBefore - heapBound != EXPECTED_FRAME_ALLOCATOR_COST ||
        heapAfter != heapBefore || largestAfter != largestBefore) {
        printf("[PROJWALL] FAILED projected wall contract changed\n");
        return 0;
    }

    printf("[PROJWALL] READY unchanged projection + Render_drawWallSpans + Render_SpanMode0 consumed one bounded GFXRM frame\n");
    printf("[PROJWALL] READY legacy mediaTexels alias existed only during draw and is restored to NULL\n");
    return 1;
}
