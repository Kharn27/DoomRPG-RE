#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Render.h"

#include "native_graphics_resource_manager.h"
#include "native_menu_sprite_frame_probe.h"
#include "native_projected_sprite_renderer.h"
#include "native_projected_wall_bridge.h"
#include "platform_video_config.h"

/* Keep ESP-IDF headers after DoomRPG.h: stdbool false/true macros collide
 * with the engine's legacy boolean enum. */
#include <esp_heap_caps.h>

#define EXPECTED_WALLS_ONLY_FNV 0xa6d87c4aU

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

int DoomRPG_probeNativeMenuSpriteFrame(struct Render_s* renderBase) {
    Render_t* render = (Render_t*)renderBase;
    Sprite_t* sprite;
    EspNativeGraphicsStats gfxStats;
    EspNativeProjectedSpriteStats spriteStats;
    EspNativeProjectedWallStats wallBackedStats;
    uint32_t heapBefore;
    uint32_t largestBefore;
    uint32_t heapAfter;
    uint32_t largestAfter;
    uint32_t wallFramebufferHash;
    uint32_t finalFramebufferHash;
    uint32_t visibleListHash = 2166136261U;
    uint32_t renderStart;
    uint32_t renderElapsed;
    uint32_t visibleObjects = 0;
    uint32_t outOfRangeObjects = 0;

    printf("\n=== Doom RPG ESP32 real menu.bsp walls + native sprites ===\n");

    if (render == NULL || render->framebuffer == NULL ||
        render->mapSprites == NULL || render->columnScale == NULL ||
        render->mediaSpriteIds == NULL || render->mediaBitShapeOffsets == NULL ||
        render->mediaPalettes == NULL || render->viewSprites == NULL) {
        printf("[MENUSPRITE] FAILED real menu sprite prerequisites unavailable\n");
        return 0;
    }

    if (render->shapeData != NULL || render->mediaTexels != NULL) {
        printf("[MENUSPRITE] FAILED legacy graphics pool resident shapeData=%p mediaTexels=%p\n",
               (void*)render->shapeData,
               (void*)render->mediaTexels);
        return 0;
    }

    wallFramebufferHash = fnv1a32(
        (const uint8_t*)render->framebuffer,
        (uint32_t)render->pitch * DOOMRPG_LOGICAL_HEIGHT);

    printf("[MENUSPRITE] Begin wallsFNV=%08x expected=%08x shapeData=%p mediaTexels=%p\n",
           (unsigned int)wallFramebufferHash,
           (unsigned int)EXPECTED_WALLS_ONLY_FNV,
           (void*)render->shapeData,
           (void*)render->mediaTexels);

    if (wallFramebufferHash != EXPECTED_WALLS_ONLY_FNV) {
        printf("[MENUSPRITE] FAILED cached walls regression changed before sprite pass\n");
        return 0;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();
    printf("[MENUSPRITE] Baseline heap8=%u largest8=%u numMapSprites=%d runtimeSlots=%d\n",
           (unsigned int)heapBefore,
           (unsigned int)largestBefore,
           render->numMapSprites,
           render->numSprites);

    for (sprite = render->viewSprites;
         sprite != NULL;
         sprite = sprite->viewNext) {
        int objectIndex = -1;

        if (sprite >= render->mapSprites &&
            sprite < render->mapSprites + render->numSprites) {
            objectIndex = (int)(sprite - render->mapSprites);
        }
        else {
            outOfRangeObjects++;
        }

        visibleListHash = fnvMixU32(visibleListHash, (uint32_t)objectIndex);
        visibleListHash = fnvMixU32(visibleListHash, (uint32_t)sprite->sortZ);
        visibleListHash = fnvMixU32(visibleListHash,
                                    (uint32_t)(sprite->info & 511));
        visibleObjects++;
    }

    printf("[MENUSPRITE] View list objects=%u outOfRange=%u listFNV=%08x ordering=original-BSP-sortZ\n",
           (unsigned int)visibleObjects,
           (unsigned int)outOfRangeObjects,
           (unsigned int)visibleListHash);

    if (visibleObjects == 0U || outOfRangeObjects != 0U) {
        printf("[MENUSPRITE] FAILED BSP sprite list contract invalid\n");
        return 0;
    }

    EspNativeGraphics_resetStats();
    EspNativeProjectedSprite_resetStats();
    EspNativeProjectedWall_resetStats();

    renderStart = (uint32_t)DoomRPG_GetTimeMS();
    for (sprite = render->viewSprites;
         sprite != NULL;
         sprite = sprite->viewNext) {
        int objectIndex = (int)(sprite - render->mapSprites);

        if (!EspNativeProjectedSprite_drawObject(render,
                                                 sprite,
                                                 objectIndex,
                                                 0)) {
            printf("[MENUSPRITE] FAILED object render index=%d\n", objectIndex);
            if (EspNativeProjectedWall_isActive()) {
                EspNativeProjectedWall_end();
            }
            return 0;
        }
    }
    renderElapsed = (uint32_t)DoomRPG_GetTimeMS() - renderStart;

    EspNativeGraphics_getStats(&gfxStats);
    EspNativeProjectedSprite_getStats(&spriteStats);
    EspNativeProjectedWall_getStats(&wallBackedStats);

    finalFramebufferHash = fnv1a32(
        (const uint8_t*)render->framebuffer,
        (uint32_t)render->pitch * DOOMRPG_LOGICAL_HEIGHT);

    heapAfter = heap8Free();
    largestAfter = largest8Block();

    printf("[MENUSPRITE] Objects total=%u hidden=%u lightsSkipped=%u entityUnsupported=%u resolvedDraws=%u\n",
           (unsigned int)spriteStats.objectCalls,
           (unsigned int)spriteStats.hiddenObjects,
           (unsigned int)spriteStats.lightObjectsSkipped,
           (unsigned int)spriteStats.entityObjectsUnsupported,
           (unsigned int)spriteStats.resolvedDrawCalls);
    printf("[MENUSPRITE] Requests spriteFrames=%u unique=%u repeats=%u requestFNV=%08x wallBacked=%u maxFrame=%uB\n",
           (unsigned int)spriteStats.spriteFrameRequests,
           (unsigned int)spriteStats.uniqueSpriteFrames,
           (unsigned int)spriteStats.repeatedSpriteFrames,
           (unsigned int)spriteStats.requestHash,
           (unsigned int)spriteStats.wallBackedRequests,
           (unsigned int)spriteStats.maxFrameBytes);
    printf("[MENUSPRITE] Cull near=%u backface=%u clipped=%u spans=%u pixels=%u\n",
           (unsigned int)spriteStats.nearCulled,
           (unsigned int)spriteStats.backfaceCulled,
           (unsigned int)spriteStats.clipCulled,
           (unsigned int)spriteStats.spanCalls,
           (unsigned int)spriteStats.pixelsDrawn);
    printf("[MENUSPRITE] Invariants rangeErrors=%u legacyPtrViolations=%u shapeDataViolations=%u mappingViolations=%u unsupportedFlags=%u unsupportedModes=%u\n",
           (unsigned int)spriteStats.rangeErrors,
           (unsigned int)spriteStats.legacyPointerViolations,
           (unsigned int)spriteStats.legacyShapeViolations,
           (unsigned int)spriteStats.mappingViolations,
           (unsigned int)spriteStats.unsupportedFlagPaths,
           (unsigned int)spriteStats.unsupportedRenderModes);
    printf("[MENUSPRITE] GFXRM spriteLoads=%u wallLoads=%u packOpenCycles=%u logicalBytes=%u peakFrame=%u\n",
           (unsigned int)gfxStats.spriteLoads,
           (unsigned int)gfxStats.wallLoads,
           (unsigned int)gfxStats.packOpenCycles,
           (unsigned int)gfxStats.logicalBytesLoaded,
           (unsigned int)gfxStats.peakFrameBytes);
    printf("[MENUSPRITE] Wall-backed projected begin=%u end=%u spans=%u pixels=%u errors=%u/%u/%u\n",
           (unsigned int)wallBackedStats.beginCalls,
           (unsigned int)wallBackedStats.endCalls,
           (unsigned int)wallBackedStats.spanCalls,
           (unsigned int)wallBackedStats.pixelsDrawn,
           (unsigned int)wallBackedStats.rangeErrors,
           (unsigned int)wallBackedStats.legacyPointerViolations,
           (unsigned int)wallBackedStats.mappingOffsetViolations);
    printf("[MENUSPRITE] framebufferFNV=%08x wallsFNV=%08x changed=%s renderMs=%u shapeData=%p mediaTexels=%p\n",
           (unsigned int)finalFramebufferHash,
           (unsigned int)wallFramebufferHash,
           finalFramebufferHash != wallFramebufferHash ? "yes" : "NO",
           (unsigned int)renderElapsed,
           (void*)render->shapeData,
           (void*)render->mediaTexels);
    printf("[MENUSPRITE] End heap8=%u largest8=%u deltaFromStart=%d\n",
           (unsigned int)heapAfter,
           (unsigned int)largestAfter,
           (int)heapBefore - (int)heapAfter);

    if (spriteStats.objectCalls != visibleObjects ||
        spriteStats.entityObjectsUnsupported != 0U ||
        spriteStats.spriteFrameRequests == 0U ||
        gfxStats.spriteLoads != spriteStats.spriteFrameRequests ||
        gfxStats.packOpenCycles != gfxStats.spriteLoads + gfxStats.wallLoads ||
        spriteStats.spanCalls == 0U || spriteStats.pixelsDrawn == 0U ||
        spriteStats.rangeErrors != 0U ||
        spriteStats.legacyPointerViolations != 0U ||
        spriteStats.legacyShapeViolations != 0U ||
        spriteStats.mappingViolations != 0U ||
        spriteStats.unsupportedFlagPaths != 0U ||
        spriteStats.unsupportedRenderModes != 0U ||
        wallBackedStats.rangeErrors != 0U ||
        wallBackedStats.legacyPointerViolations != 0U ||
        wallBackedStats.mappingOffsetViolations != 0U ||
        render->shapeData != NULL || render->mediaTexels != NULL ||
        finalFramebufferHash == wallFramebufferHash ||
        heapAfter != heapBefore || largestAfter != largestBefore) {
        printf("[MENUSPRITE] FAILED real menu sprite contract changed\n");
        return 0;
    }

    SDL_RenderPresent(NULL);
    printf("[MENUSPRITE] Presented real BSP-sorted sprites over cached native menu walls\n");
    printf("[MENUSPRITE] READY real menu sprites rendered from bounded uncached GFXRM frames\n");
    printf("[MENUSPRITE] READY request sequence measured before any sprite cache policy is chosen\n");
    return 1;
}
