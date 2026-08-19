#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Render.h"

#include "native_graphics_resource_manager.h"
#include "native_menu_wall_frame_probe.h"
#include "native_projected_wall_bridge.h"
#include "native_wall_lru_cache.h"
#include "platform_video_config.h"

/* Keep ESP-IDF headers after DoomRPG.h: stdbool false/true macros collide
 * with the engine's legacy boolean enum. */
#include <esp_heap_caps.h>

#define MENU_CAMERA_VIEW_Z 36
#define MENU_TEXTURE_TRACK_LIMIT 1024
#define MENU_TEXTURE_TRACK_WORDS (MENU_TEXTURE_TRACK_LIMIT / 32)
#define PACKED_WALL_BYTES 2048U

#define EXPECTED_MENU_NODE_COUNT 28U
#define EXPECTED_MENU_NODE_RASTER_COUNT 10U
#define EXPECTED_MENU_VISIBLE_LEAVES 10U
#define EXPECTED_MENU_SOURCE_LINES 46U
#define EXPECTED_MENU_WALL_REQUESTS 25U
#define EXPECTED_MENU_BACKFACE_CULLED 15U
#define EXPECTED_MENU_CLIP_CULLED 6U
#define EXPECTED_MENU_UNIQUE_TEXTURES 8U
#define EXPECTED_MENU_REPEATED_REQUESTS 17U
#define EXPECTED_MENU_REQUEST_FNV 0x4db9da28U
#define EXPECTED_MENU_FRAMEBUFFER_FNV 0xa6d87c4aU
#define EXPECTED_MENU_NATIVE_SPANS 224U
#define EXPECTED_MENU_NATIVE_PIXELS 5589U

#define EXPECTED_CACHE_HITS 14U
#define EXPECTED_CACHE_MISSES 11U
#define EXPECTED_CACHE_EVICTIONS 8U
#define EXPECTED_CACHE_RESIDENT_SLOTS 3U
#define EXPECTED_CACHE_RESIDENT_BYTES \
    (EXPECTED_CACHE_RESIDENT_SLOTS * PACKED_WALL_BYTES)
#define EXPECTED_GFXRM_LOGICAL_BYTES \
    (EXPECTED_CACHE_MISSES * PACKED_WALL_BYTES)

typedef struct MenuWallFrameStats_s {
    uint32_t leafNodes;
    uint32_t lineCandidates;
    uint32_t backfaceCulled;
    uint32_t clipCulled;
    uint32_t spriteSpanSkipped;
    uint32_t occluderOnlySkipped;
    uint32_t wallRequests;
    uint32_t uniqueTextures;
    uint32_t repeatedTextureRequests;
    uint32_t textureTrackingErrors;
    uint32_t textureRequestHash;
} MenuWallFrameStats;

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

static uint32_t fnvMixU32(uint32_t hash, uint32_t value) {
    int shift;
    for (shift = 0; shift < 32; shift += 8) {
        hash ^= (value >> shift) & 0xffU;
        hash *= 16777619U;
    }
    return hash;
}

static void prepareSolidMenuBackground(Render_t* render) {
    int i;

    memset(render->framebuffer, 0,
           (size_t)render->pitch * DOOMRPG_LOGICAL_HEIGHT);

    /* DoomCanvas_LoadMenuMap() grays the menu palette and then replicates the
     * first floor/ceiling colors across the viewport width after Render_setup().
     * The runtime structures probe already has the correct viewport allocation,
     * so only the color replication is needed here.
     */
    Render_setGrayPalettes(render);
    for (i = 1; i < render->screenWidth; ++i) {
        render->floorColor[i] = render->floorColor[0];
        render->ceilingColor[i] = render->ceilingColor[0];
    }
}

static int resolveDeterministicWallTexture(Render_t* render,
                                           const Line_t* line,
                                           int* outTextureIndex) {
    uint32_t phase;
    int logicalTexture;
    int baseTexture;

    if (render == NULL || line == NULL || outTextureIndex == NULL ||
        render->mediaTexturesIds == NULL) {
        return 0;
    }

    logicalTexture = line->texture;
    if (logicalTexture < 0 || logicalTexture >= render->textureCnt) {
        printf("[MENUWALL] FAILED logical texture=%d outside textureCnt=%d\n",
               logicalTexture, render->textureCnt);
        return 0;
    }

    /* Render_drawNodeLines() adds a four-frame phase derived from
     * animFrameTime + logicalTexture*3. Freeze animFrameTime at zero for this
     * hardware regression so the resulting frame/hash is deterministic.
     */
    phase = (((uint32_t)(logicalTexture * 3) * 0x400000U) >> 30);
    baseTexture = render->mediaTexturesIds[logicalTexture];
    *outTextureIndex = baseTexture + (int)(uint16_t)phase;
    return *outTextureIndex >= 0;
}

static void noteTextureRequest(MenuWallFrameStats* stats,
                               uint32_t* seenTextures,
                               int textureIndex,
                               int lineIndex) {
    uint32_t word;
    uint32_t bit;

    stats->textureRequestHash =
        fnvMixU32(stats->textureRequestHash, (uint32_t)lineIndex);
    stats->textureRequestHash =
        fnvMixU32(stats->textureRequestHash, (uint32_t)textureIndex);

    if (textureIndex < 0 || textureIndex >= MENU_TEXTURE_TRACK_LIMIT) {
        stats->textureTrackingErrors++;
        return;
    }

    word = (uint32_t)textureIndex >> 5;
    bit = 1U << ((uint32_t)textureIndex & 31U);
    if (seenTextures[word] & bit) {
        stats->repeatedTextureRequests++;
    }
    else {
        seenTextures[word] |= bit;
        stats->uniqueTextures++;
    }
}

/* Return values:
 *   0 = fatal error
 *   1 = line valid but intentionally/not visibly rasterized
 *   2 = projected wall is ready for native texture sampling
 */
static int projectMenuWall(Render_t* render,
                           const Line_t* sourceLine,
                           int lineIndex,
                           Line_t* projected,
                           MenuWallFrameStats* stats) {
    Vertex_t swapVertex;

    *projected = *sourceLine;
    stats->lineCandidates++;

    if (((projected->vert1.x - render->viewX) *
         (projected->vert2.y - projected->vert1.y)) +
        ((projected->vert1.y - render->viewY) *
         (-(projected->vert2.x - projected->vert1.x))) <= 0) {
        if (!(projected->flags & 1)) {
            stats->backfaceCulled++;
            return 1;
        }

        swapVertex = projected->vert1;
        projected->vert1 = projected->vert2;
        projected->vert2 = swapVertex;
    }

    Render_transform2DVerts(render, &projected->vert1);
    Render_transform2DVerts(render, &projected->vert2);

    if (!Render_clipLine(render, projected)) {
        stats->clipCulled++;
        return 1;
    }

    Render_projectVertex(render, &projected->vert1);
    Render_projectVertex(render, &projected->vert2);

    if ((projected->flags & 0x20000000) != 0) {
        /* This flag is injected by Render_walkNode() for its visibility-only
         * occlusion pass. It should not normally exist on the source map line,
         * but keep the original semantic if one ever does.
         */
        Render_occludeClippedLine(render, projected);
        stats->occluderOnlySkipped++;
        return 1;
    }

    if ((projected->flags & 2) != 0) {
        /* Original Render_drawLine() routes these through Render_drawSpriteSpan.
         * That source path is deliberately outside this walls-only increment.
         */
        stats->spriteSpanSkipped++;
        return 1;
    }

    if (lineIndex < 0 || lineIndex >= render->linesLength) {
        printf("[MENUWALL] FAILED line index=%d outside linesLength=%d\n",
               lineIndex, render->linesLength);
        return 0;
    }

    if ((render->lines[lineIndex].flags & 160) == 0) {
        render->lines[lineIndex].flags |= 128;
    }

    return 2;
}

static int drawNativeMenuWall(Render_t* render,
                              const Line_t* sourceLine,
                              int lineIndex,
                              MenuWallFrameStats* stats,
                              uint32_t* seenTextures) {
    const EspNativeWallFrame* cachedFrame;
    Line_t projected;
    int projectionResult;
    int textureIndex;

    projectionResult = projectMenuWall(render, sourceLine, lineIndex,
                                       &projected, stats);
    if (projectionResult != 2) {
        return projectionResult != 0;
    }

    if (!resolveDeterministicWallTexture(render, sourceLine, &textureIndex)) {
        return 0;
    }

    projected.texture = (short)textureIndex;
    render->spanMode = 0;
    render->numLines = lineIndex;

    noteTextureRequest(stats, seenTextures, textureIndex, lineIndex);
    stats->wallRequests++;

    cachedFrame = NULL;
    if (!EspNativeWallCache_acquire(render, textureIndex, &cachedFrame) ||
        cachedFrame == NULL) {
        printf("[MENUWALL] FAILED wall-cache acquire line=%d texture=%d\n",
               lineIndex, textureIndex);
        return 0;
    }

    if (!EspNativeProjectedWall_beginBorrowed(render, cachedFrame)) {
        printf("[MENUWALL] FAILED borrowed projected frame line=%d texture=%d\n",
               lineIndex, textureIndex);
        return 0;
    }

    if (!EspNativeProjectedWall_drawWallSpans(render, &projected)) {
        printf("[MENUWALL] FAILED native spans line=%d texture=%d\n",
               lineIndex, textureIndex);
        EspNativeProjectedWall_end();
        return 0;
    }

    EspNativeProjectedWall_end();
    return 1;
}

int DoomRPG_probeNativeMenuWallFrame(struct Render_s* renderBase) {
    Render_t* render = (Render_t*)renderBase;
    DoomCanvas_t* doomCanvas;
    Node_t* viewNode;
    Node_t* viewNodes;
    EspNativeGraphicsStats gfxStats;
    EspNativeProjectedWallStats projectedStats;
    EspNativeWallCacheStats cacheStats;
    MenuWallFrameStats stats;
    uint32_t seenTextures[MENU_TEXTURE_TRACK_WORDS];
    uint32_t heapBefore;
    uint32_t largestBefore;
    uint32_t heapCacheResident;
    uint32_t largestCacheResident;
    uint32_t heapAfter;
    uint32_t largestAfter;
    uint32_t framebufferHash;
    uint32_t renderStart;
    uint32_t renderElapsed;
    int menuViewX;
    int menuViewY;
    int menuViewAngle;
    int oldSkipLines;
    int oldSkipSprites;
    int oldSkipViewNudge;
    int oldRenderFloorCeilingTextures;

    printf("\n=== Doom RPG ESP32 real menu.bsp walls-only frame + 3-slot LRU ===\n");

    if (render == NULL || render->doomRpg == NULL ||
        render->doomRpg->doomCanvas == NULL ||
        render->framebuffer == NULL || render->nodes == NULL ||
        render->lines == NULL || render->columnScale == NULL ||
        render->floorColor == NULL || render->ceilingColor == NULL ||
        render->mediaPalettes == NULL || render->mediaTexturesIds == NULL ||
        render->mediaTexelOffsets == NULL) {
        printf("[MENUWALL] FAILED renderer/menu runtime contract unavailable\n");
        return 0;
    }

    if (render->shapeData != NULL || render->mediaTexels != NULL) {
        printf("[MENUWALL] FAILED legacy graphics pools resident shapeData=%p mediaTexels=%p\n",
               (void*)render->shapeData, (void*)render->mediaTexels);
        return 0;
    }

    if (render->nodesLength != 53 || render->linesLength != 120 ||
        render->screenWidth != 160 || render->screenHeight != 80 ||
        render->screenY != 20) {
        printf("[MENUWALL] FAILED unexpected menu geometry nodes=%d lines=%d viewport=%dx%d@%d,%d\n",
               render->nodesLength, render->linesLength,
               render->screenWidth, render->screenHeight,
               render->screenX, render->screenY);
        return 0;
    }

    if (render->mapSpawnIndex < 0 || render->mapSpawnIndex >= 1024) {
        printf("[MENUWALL] FAILED invalid menu spawn index=%d\n",
               render->mapSpawnIndex);
        return 0;
    }

    doomCanvas = render->doomRpg->doomCanvas;
    menuViewX = ((render->mapSpawnIndex % 32) << 6) + 32;
    menuViewY = ((render->mapSpawnIndex / 32) << 6) + 32;
    menuViewAngle = render->mapSpawnDir & 255;

    heapBefore = heap8Free();
    largestBefore = largest8Block();

    printf("[MENUWALL] Begin heap8=%u largest8=%u shapeData=%p mediaTexels=%p\n",
           (unsigned int)heapBefore,
           (unsigned int)largestBefore,
           (void*)render->shapeData,
           (void*)render->mediaTexels);
    printf("[MENUWALL] BSP header spawnIndex=%d tile=%d,%d world=%d,%d dir=%d cameraZ=%d\n",
           render->mapSpawnIndex,
           render->mapSpawnIndex % 32,
           render->mapSpawnIndex / 32,
           menuViewX, menuViewY,
           menuViewAngle,
           MENU_CAMERA_VIEW_Z);
    printf("[MENUWALL] cameraZ=36 follows the engine's normal eye-height convention; menu X/Y/angle come directly from menu.bsp\n");

    memset(&stats, 0, sizeof(stats));
    memset(seenTextures, 0, sizeof(seenTextures));
    stats.textureRequestHash = 2166136261U;

    oldSkipLines = render->skipLines;
    oldSkipSprites = render->skipSprites;
    oldSkipViewNudge = render->skipViewNudge;
    oldRenderFloorCeilingTextures = doomCanvas->renderFloorCeilingTextures;

    prepareSolidMenuBackground(render);

    /* Reuse Render_render() for the exact camera transform, viewport setup,
     * solid background and BSP visibility walk. Lines/sprites are suppressed
     * only for this pass; Render_walkNode() still builds the original ordered
     * view-node list and performs its occlusion pass.
     */
    render->skipLines = 1;
    render->skipSprites = 1;
    render->skipViewNudge = 1;
    doomCanvas->renderFloorCeilingTextures = false;

    renderStart = (uint32_t)DoomRPG_GetTimeMS();
    Render_render(render, menuViewX, menuViewY, MENU_CAMERA_VIEW_Z,
                  (unsigned int)menuViewAngle);

    render->skipLines = oldSkipLines;
    render->skipSprites = oldSkipSprites;
    render->skipViewNudge = oldSkipViewNudge;
    doomCanvas->renderFloorCeilingTextures = oldRenderFloorCeilingTextures;

    /* Freeze animation exactly as in the uncached reference frame. */
    render->animFrameTime = 0;

    EspNativeGraphics_resetStats();
    EspNativeProjectedWall_resetStats();
    if (!EspNativeWallCache_begin(render)) {
        printf("[MENUWALL] FAILED starting cold three-slot wall cache\n");
        return 0;
    }

    viewNodes = &render->viewNodes;
    for (viewNode = viewNodes->next;
         viewNode != NULL && viewNode != viewNodes;
         viewNode = viewNode->next) {
        int lineCount = (viewNode->args2 >> 16) & 0xffff;
        int firstLine = viewNode->args2 & 0xffff;
        int i;

        stats.leafNodes++;
        if (firstLine < 0 || lineCount < 0 ||
            firstLine + lineCount > render->linesLength) {
            printf("[MENUWALL] FAILED leaf line range first=%d count=%d lines=%d\n",
                   firstLine, lineCount, render->linesLength);
            EspNativeWallCache_end();
            return 0;
        }

        for (i = 0; i < lineCount; ++i) {
            int lineIndex = firstLine + i;
            if (!drawNativeMenuWall(render, &render->lines[lineIndex],
                                    lineIndex, &stats, seenTextures)) {
                if (EspNativeProjectedWall_isActive()) {
                    EspNativeProjectedWall_end();
                }
                EspNativeWallCache_end();
                return 0;
            }
        }
    }

    renderElapsed = (uint32_t)DoomRPG_GetTimeMS() - renderStart;
    EspNativeProjectedWall_getStats(&projectedStats);
    EspNativeGraphics_getStats(&gfxStats);
    EspNativeWallCache_getStats(&cacheStats);

    framebufferHash = fnv1a32(
        render->framebuffer,
        (uint32_t)render->pitch * DOOMRPG_LOGICAL_HEIGHT);

    heapCacheResident = heap8Free();
    largestCacheResident = largest8Block();

    printf("[MENUWALL] BSP visibility nodeCount=%d nodeRasterCount=%d visibleLeaves=%u sourceLines=%u\n",
           render->nodeCount,
           render->nodeRasterCount,
           (unsigned int)stats.leafNodes,
           (unsigned int)stats.lineCandidates);
    printf("[MENUWALL] Line result walls=%u backface=%u clipped=%u spriteSpanSkipped=%u occluderOnly=%u\n",
           (unsigned int)stats.wallRequests,
           (unsigned int)stats.backfaceCulled,
           (unsigned int)stats.clipCulled,
           (unsigned int)stats.spriteSpanSkipped,
           (unsigned int)stats.occluderOnlySkipped);
    printf("[MENUWALL] Texture requests total=%u unique=%u repeats=%u trackingErrors=%u requestFNV=%08x animFrameTime=0\n",
           (unsigned int)stats.wallRequests,
           (unsigned int)stats.uniqueTextures,
           (unsigned int)stats.repeatedTextureRequests,
           (unsigned int)stats.textureTrackingErrors,
           (unsigned int)stats.textureRequestHash);
    printf("[MENUWALL] LRU slots=%u requests=%u hits=%u misses=%u evictions=%u resident=%u peak=%u residentBytes=%u peakBytes=%u\n",
           (unsigned int)ESP_NATIVE_WALL_CACHE_SLOTS,
           (unsigned int)cacheStats.requests,
           (unsigned int)cacheStats.hits,
           (unsigned int)cacheStats.misses,
           (unsigned int)cacheStats.evictions,
           (unsigned int)cacheStats.residentSlots,
           (unsigned int)cacheStats.peakResidentSlots,
           (unsigned int)cacheStats.residentBytes,
           (unsigned int)cacheStats.peakResidentBytes);
    printf("[MENUWALL] LRU resident heap8=%u largest8=%u aggregateCost=%dB logicalPayload=%uB\n",
           (unsigned int)heapCacheResident,
           (unsigned int)largestCacheResident,
           (int)heapBefore - (int)heapCacheResident,
           (unsigned int)cacheStats.residentBytes);
    printf("[MENUWALL] GFXRM wallLoads=%u packOpenCycles=%u logicalBytes=%u expected=%u peakFrame=%u\n",
           (unsigned int)gfxStats.wallLoads,
           (unsigned int)gfxStats.packOpenCycles,
           (unsigned int)gfxStats.logicalBytesLoaded,
           (unsigned int)EXPECTED_GFXRM_LOGICAL_BYTES,
           (unsigned int)gfxStats.peakFrameBytes);
    printf("[MENUWALL] Native spans begin=%u end=%u spanCalls=%u pixels=%u rangeErrors=%u legacyPtrViolations=%u mappingViolations=%u\n",
           (unsigned int)projectedStats.beginCalls,
           (unsigned int)projectedStats.endCalls,
           (unsigned int)projectedStats.spanCalls,
           (unsigned int)projectedStats.pixelsDrawn,
           (unsigned int)projectedStats.rangeErrors,
           (unsigned int)projectedStats.legacyPointerViolations,
           (unsigned int)projectedStats.mappingOffsetViolations);
    printf("[MENUWALL] framebufferFNV=%08x expected=%08x renderMs=%u floor=%04x ceiling=%04x mediaTexels=%p\n",
           (unsigned int)framebufferHash,
           (unsigned int)EXPECTED_MENU_FRAMEBUFFER_FNV,
           (unsigned int)renderElapsed,
           (unsigned int)(uint16_t)render->floorColor[0],
           (unsigned int)(uint16_t)render->ceilingColor[0],
           (void*)render->mediaTexels);

    if ((uint32_t)render->nodeCount != EXPECTED_MENU_NODE_COUNT ||
        (uint32_t)render->nodeRasterCount != EXPECTED_MENU_NODE_RASTER_COUNT ||
        stats.leafNodes != EXPECTED_MENU_VISIBLE_LEAVES ||
        stats.lineCandidates != EXPECTED_MENU_SOURCE_LINES ||
        stats.wallRequests != EXPECTED_MENU_WALL_REQUESTS ||
        stats.backfaceCulled != EXPECTED_MENU_BACKFACE_CULLED ||
        stats.clipCulled != EXPECTED_MENU_CLIP_CULLED ||
        stats.spriteSpanSkipped != 0U || stats.occluderOnlySkipped != 0U ||
        stats.uniqueTextures != EXPECTED_MENU_UNIQUE_TEXTURES ||
        stats.repeatedTextureRequests != EXPECTED_MENU_REPEATED_REQUESTS ||
        stats.textureTrackingErrors != 0U ||
        stats.textureRequestHash != EXPECTED_MENU_REQUEST_FNV ||
        cacheStats.requests != EXPECTED_MENU_WALL_REQUESTS ||
        cacheStats.hits != EXPECTED_CACHE_HITS ||
        cacheStats.misses != EXPECTED_CACHE_MISSES ||
        cacheStats.evictions != EXPECTED_CACHE_EVICTIONS ||
        cacheStats.residentSlots != EXPECTED_CACHE_RESIDENT_SLOTS ||
        cacheStats.peakResidentSlots != EXPECTED_CACHE_RESIDENT_SLOTS ||
        cacheStats.residentBytes != EXPECTED_CACHE_RESIDENT_BYTES ||
        cacheStats.peakResidentBytes != EXPECTED_CACHE_RESIDENT_BYTES ||
        gfxStats.spriteLoads != 0U ||
        gfxStats.wallLoads != EXPECTED_CACHE_MISSES ||
        gfxStats.packOpenCycles != EXPECTED_CACHE_MISSES ||
        gfxStats.logicalBytesLoaded != EXPECTED_GFXRM_LOGICAL_BYTES ||
        gfxStats.peakFrameBytes != PACKED_WALL_BYTES ||
        projectedStats.beginCalls != EXPECTED_MENU_WALL_REQUESTS ||
        projectedStats.endCalls != EXPECTED_MENU_WALL_REQUESTS ||
        projectedStats.spanCalls != EXPECTED_MENU_NATIVE_SPANS ||
        projectedStats.pixelsDrawn != EXPECTED_MENU_NATIVE_PIXELS ||
        projectedStats.rangeErrors != 0U ||
        projectedStats.legacyPointerViolations != 0U ||
        projectedStats.mappingOffsetViolations != 0U ||
        framebufferHash != EXPECTED_MENU_FRAMEBUFFER_FNV ||
        render->mediaTexels != NULL) {
        printf("[MENUWALL] FAILED cached real menu regression contract changed\n");
        EspNativeWallCache_end();
        return 0;
    }

    EspNativeWallCache_end();
    heapAfter = heap8Free();
    largestAfter = largest8Block();

    printf("[MENUWALL] End heap8=%u largest8=%u deltaFromStart=%d cacheReleased=yes\n",
           (unsigned int)heapAfter,
           (unsigned int)largestAfter,
           (int)heapBefore - (int)heapAfter);

    if (EspNativeWallCache_isActive() ||
        heapAfter != heapBefore || largestAfter != largestBefore) {
        printf("[MENUWALL] FAILED cache teardown did not restore allocator state\n");
        return 0;
    }

    SDL_RenderPresent(NULL);
    printf("[MENUWALL] Presented cached real menu.bsp walls-only frame on CYD\n");
    printf("[MENUWALL] READY framebuffer stayed bit-identical while LRU reduced 25 requests to 11 physical wall loads\n");
    printf("[MENUWALL] READY measured three-slot cache = 14 hits / 11 misses / 8 evictions / 6144B peak payload\n");
    return 1;
}
