#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Game.h"
#include "Render.h"
#include "map_runtime_structure_probe.h"

/* Keep ESP-IDF's stdbool macros after DoomRPG's legacy boolean enum. */
#include <esp_heap_caps.h>

extern DoomRPG_t* doomRpg;

#define EXPECTED_MENU_NODES 53
#define EXPECTED_MENU_LINES 120
#define EXPECTED_MENU_MAP_SPRITES 44
#define EXPECTED_MENU_RUNTIME_SPRITES 68
#define EXPECTED_MENU_EVENTS 15
#define EXPECTED_STRUCTURAL_PAYLOAD 13980U
#define EXPECTED_LARGEST_STRUCTURAL_ALLOC 4096U

static int probeAttempted = 0;
static int probeReady = 0;
static int boundaryReached = 0;
static uint32_t boundaryHeap8 = 0;
static uint32_t boundaryLargest8 = 0;

static uint32_t heap8Free(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8Block(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

void DoomRPG_markMapRuntimeStructureBoundary(struct Render_s* renderBase) {
    Render_t* render = (Render_t*)renderBase;

    boundaryReached = 1;
    boundaryHeap8 = heap8Free();
    boundaryLargest8 = largest8Block();

    if (render == NULL) {
        printf("[MAPSTRUCT] Boundary reached with NULL Render pointer\n");
        return;
    }

    printf("[MAPSTRUCT] Boundary reached before bitshapes/texels heap8=%u largest8=%u\n",
           (unsigned int)boundaryHeap8,
           (unsigned int)boundaryLargest8);
    printf("[MAPSTRUCT] Real runtime counts nodes=%d lines=%d mapSprites=%d runtimeSprites=%d events=%d\n",
           render->nodesLength,
           render->linesLength,
           render->numMapSprites,
           render->numSprites,
           render->numTileEvents);
    printf("[MAPSTRUCT] Resource refs mapTextures=%d mapSprites=%d planeTextures=%d\n",
           render->mapTextureTexelsCount,
           render->mapSpriteTexelsCount,
           render->planeTexturesCnt);
}

int DoomRPG_probeMenuMapRuntimeStructures(int menuBspReady) {
    Render_t* render;
    boolean beginResult;
    boolean dataResult;
    uint32_t heapBefore;
    uint32_t largestBefore;
    uint32_t heapAfterBegin;
    uint32_t largestAfterBegin;
    uint32_t heapAfter;
    uint32_t largestAfter;
    uint32_t measuredUsed;

    if (probeAttempted) {
        return probeReady;
    }
    probeAttempted = 1;

    printf("\n=== Doom RPG real menu map runtime structure probe ===\n");

    if (!menuBspReady) {
        printf("[MAPSTRUCT] Complete menu BSP plan is not ready; probe skipped safely\n");
        return 0;
    }

    if (doomRpg == NULL || doomRpg->render == NULL) {
        printf("[MAPSTRUCT] Core Render object unavailable; probe refused\n");
        return 0;
    }

    render = doomRpg->render;
    heapBefore = heap8Free();
    largestBefore = largest8Block();

    printf("[MAPSTRUCT] Begin heap8=%u largest8=%u plannedPayload=%u largestAlloc=%u\n",
           (unsigned int)heapBefore,
           (unsigned int)largestBefore,
           (unsigned int)EXPECTED_STRUCTURAL_PAYLOAD,
           (unsigned int)EXPECTED_LARGEST_STRUCTURAL_ALLOC);

    if (heapBefore <= EXPECTED_STRUCTURAL_PAYLOAD ||
        largestBefore < EXPECTED_LARGEST_STRUCTURAL_ALLOC) {
        printf("[MAPSTRUCT] REFUSED current heap no longer satisfies validated structure plan\n");
        return 0;
    }

    boundaryReached = 0;
    boundaryHeap8 = 0;
    boundaryLargest8 = 0;

    printf("[MAPSTRUCT] -> Render_beginLoadMap(MAP_MENU)\n");
    beginResult = Render_beginLoadMap(render, MAP_MENU);
    heapAfterBegin = heap8Free();
    largestAfterBegin = largest8Block();

    printf("[MAPSTRUCT] Render_beginLoadMap result=%d heap8=%u largest8=%u ioBuffer=%p pos=%d\n",
           (int)beginResult,
           (unsigned int)heapAfterBegin,
           (unsigned int)largestAfterBegin,
           (void*)render->ioBuffer,
           render->ioBufferPos);

    if (!beginResult || render->ioBuffer == NULL || render->ioBufferPos != 33) {
        printf("[MAPSTRUCT] FAILED real Render_beginLoadMap did not reach the validated header boundary\n");
        return 0;
    }

    printf("[MAPSTRUCT] -> Render_beginLoadMapData() (expected probe-stop false)\n");
    dataResult = Render_beginLoadMapData(render);
    heapAfter = heap8Free();
    largestAfter = largest8Block();

    printf("[MAPSTRUCT] Render_beginLoadMapData result=%d boundary=%s heap8=%u largest8=%u\n",
           (int)dataResult,
           boundaryReached ? "reached" : "NOT reached",
           (unsigned int)heapAfter,
           (unsigned int)largestAfter);

    if (!boundaryReached) {
        printf("[MAPSTRUCT] FAILED map loader stopped before the intended bitshape boundary\n");
        return 0;
    }

    if (dataResult != false) {
        printf("[MAPSTRUCT] FAILED probe guard was bypassed; resource loading may have executed\n");
        return 0;
    }

    if (render->ioBuffer != NULL) {
        printf("[MAPSTRUCT] FAILED generated Render probe did not clear freed ioBuffer\n");
        return 0;
    }

    if (render->nodesLength != EXPECTED_MENU_NODES || render->nodes == NULL ||
        render->linesLength != EXPECTED_MENU_LINES || render->lines == NULL ||
        render->numMapSprites != EXPECTED_MENU_MAP_SPRITES ||
        render->numSprites != EXPECTED_MENU_RUNTIME_SPRITES ||
        render->mapSprites == NULL ||
        render->numTileEvents != EXPECTED_MENU_EVENTS || render->tileEvents == NULL ||
        render->mapByteCode == NULL ||
        render->mapTextureTexels == NULL || render->mapSpriteTexels == NULL) {
        printf("[MAPSTRUCT] FAILED runtime structures do not match the validated menu.bsp plan\n");
        return 0;
    }

    if (render->mapStringCount != 0 || render->mapStringsIDs != NULL) {
        printf("[MAPSTRUCT] FAILED menu BSP unexpectedly retained strings\n");
        return 0;
    }

    if (render->mapTextureTexelsCount <= 0 ||
        render->mapSpriteTexelsCount <= 0 ||
        render->planeTexturesCnt <= 0 || render->planeTexturesCnt > 24) {
        printf("[MAPSTRUCT] FAILED generated texture/sprite reference lists are invalid\n");
        return 0;
    }

    measuredUsed = heapBefore >= heapAfter ? heapBefore - heapAfter : 0;

    printf("[MAPSTRUCT] Persistent real structures used=%uB planPayload=%uB overhead=%dB\n",
           (unsigned int)measuredUsed,
           (unsigned int)EXPECTED_STRUCTURAL_PAYLOAD,
           (int)measuredUsed - (int)EXPECTED_STRUCTURAL_PAYLOAD);
    printf("[MAPSTRUCT] Persistent pointers nodes=%p lines=%p sprites=%p events=%p byteCode=%p\n",
           (void*)render->nodes,
           (void*)render->lines,
           (void*)render->mapSprites,
           (void*)render->tileEvents,
           (void*)render->mapByteCode);
    printf("[MAPSTRUCT] Scratch refs textures=%p sprites=%p counts=%d/%d planes=%d\n",
           (void*)render->mapTextureTexels,
           (void*)render->mapSpriteTexels,
           render->mapTextureTexelsCount,
           render->mapSpriteTexelsCount,
           render->planeTexturesCnt);
    printf("[MAPSTRUCT] READY real menu structures resident heap8=%u largest8=%u\n",
           (unsigned int)heapAfter,
           (unsigned int)largestAfter);
    printf("[MAPSTRUCT] Render_loadBitShapes / Render_loadTexels intentionally NOT executed\n");

    probeReady = 1;
    return 1;
}
