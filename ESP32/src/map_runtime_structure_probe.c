#include <SDL.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Game.h"
#include "Render.h"
#include "map_runtime_structure_probe.h"
#include "native_asset_pack_probe.h"
#include "native_bitshape_loader.h"
#include "native_palette.h"
#include "native_sprite_render_consumer.h"
#include "native_sprite_texel_probe.h"
#include "native_wall_render_consumer.h"
#include "resource_memory_plan_probe.h"

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
static int probeActive = 0;
static int loadingBarCalls = 0;
static int boundaryReached = 0;
static uint32_t boundaryHeap8 = 0;
static uint32_t boundaryLargest8 = 0;
static jmp_buf structureBoundaryJump;

static uint32_t heap8Free(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8Block(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static int realStructuresAreComplete(Render_t* render) {
    if (render == NULL) {
        return 0;
    }

    return render->nodesLength == EXPECTED_MENU_NODES && render->nodes != NULL &&
           render->linesLength == EXPECTED_MENU_LINES && render->lines != NULL &&
           render->numMapSprites == EXPECTED_MENU_MAP_SPRITES &&
           render->numSprites == EXPECTED_MENU_RUNTIME_SPRITES &&
           render->mapSprites != NULL &&
           render->numTileEvents == EXPECTED_MENU_EVENTS && render->tileEvents != NULL &&
           render->mapByteCode != NULL &&
           render->mapStringCount == 0 && render->mapStringsIDs == NULL &&
           render->mapTextureTexels != NULL && render->mapSpriteTexels != NULL &&
           render->mapTextureTexelsCount > 0 &&
           render->mapSpriteTexelsCount > 0 &&
           render->planeTexturesCnt > 0 && render->planeTexturesCnt <= 24;
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

    printf("[MAPSTRUCT] Boundary reached before bitshapes/texels loadingBarCalls=%d heap8=%u largest8=%u\n",
           loadingBarCalls,
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

/*
 * DoomCanvas_updateLoadingBar() is defined in DoomCanvas.c, a different object
 * from Render.c, so GNU ld --wrap is reliable here. During the real
 * Render_beginLoadMapData() path the first six calls occur while parsing
 * nodes/lines/sprites/events/strings/blockmap. The next call occurs after the
 * BSP ioBuffer has been freed and immediately before Render_loadBitShapes().
 *
 * The wrapper is transparent during every other startup stage. While this
 * probe is armed, it only jumps out once the actual runtime structures and
 * resource reference lists prove that the complete structural phase ran.
 */
void __real_DoomCanvas_updateLoadingBar(DoomCanvas_t* doomCanvas);

void __wrap_DoomCanvas_updateLoadingBar(DoomCanvas_t* doomCanvas) {
    Render_t* render;

    if (!probeActive) {
        __real_DoomCanvas_updateLoadingBar(doomCanvas);
        return;
    }

    loadingBarCalls++;
    render = (doomRpg != NULL) ? doomRpg->render : NULL;

    if (loadingBarCalls >= 7 && realStructuresAreComplete(render)) {
        /* Render_beginLoadMapData() has already SDL_free()'d ioBuffer here. */
        render->ioBuffer = NULL;
        DoomRPG_markMapRuntimeStructureBoundary(render);
        probeActive = 0;
        longjmp(structureBoundaryJump, 1);
    }

    __real_DoomCanvas_updateLoadingBar(doomCanvas);
}

int DoomRPG_probeMenuMapRuntimeStructures(int menuBspReady) {
    Render_t* render;
    boolean beginResult;
    int jumpResult;
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
    loadingBarCalls = 0;
    probeActive = 0;

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

    printf("[MAPSTRUCT] -> real Render_beginLoadMapData(), armed stop before bitshapes\n");
    probeActive = 1;
    jumpResult = setjmp(structureBoundaryJump);

    if (jumpResult == 0) {
        boolean unexpectedResult = Render_beginLoadMapData(render);
        probeActive = 0;
        printf("[MAPSTRUCT] FAILED Render_beginLoadMapData returned normally result=%d calls=%d\n",
               (int)unexpectedResult, loadingBarCalls);
        return 0;
    }

    probeActive = 0;
    heapAfter = heap8Free();
    largestAfter = largest8Block();

    printf("[MAPSTRUCT] Probe-stop returned from real loader boundary=%s calls=%d heap8=%u largest8=%u\n",
           boundaryReached ? "reached" : "NOT reached",
           loadingBarCalls,
           (unsigned int)heapAfter,
           (unsigned int)largestAfter);

    if (!boundaryReached) {
        printf("[MAPSTRUCT] FAILED map loader did not reach the intended bitshape boundary\n");
        return 0;
    }

    if (loadingBarCalls != 7) {
        printf("[MAPSTRUCT] FAILED unexpected loading-bar boundary count=%d expected=7\n",
               loadingBarCalls);
        return 0;
    }

    if (render->ioBuffer != NULL) {
        printf("[MAPSTRUCT] FAILED freed BSP ioBuffer was not cleared by probe boundary\n");
        return 0;
    }

    if (!realStructuresAreComplete(render)) {
        printf("[MAPSTRUCT] FAILED runtime structures do not match the validated menu.bsp plan\n");
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
    printf("[MAPSTRUCT] Original Render_loadBitShapes / Render_loadTexels intentionally NOT executed\n");

    if (!DoomRPG_probeMenuResourceMemoryPlan(1)) {
        printf("[MAPSTRUCT] FAILED graphics resource memory plan\n");
        return 0;
    }

    printf("[MAPSTRUCT] Resource memory plan complete; original heavy graphics loaders remain blocked\n");

    if (!DoomRPG_probeNativeAssetPack(1)) {
        printf("[MAPSTRUCT] FAILED ESP32-native asset pack probe\n");
        return 0;
    }

    printf("[MAPSTRUCT] Native asset pack random-access probe complete\n");

    if (!DoomRPG_loadNativeBitShapes(render)) {
        printf("[MAPSTRUCT] FAILED ESP32-native selected bitshape loader\n");
        return 0;
    }

    printf("[MAPSTRUCT] Native on-demand bitshape model validated; probing sprite texels\n");

    if (!DoomRPG_probeNativeSpriteTexels(render)) {
        printf("[MAPSTRUCT] FAILED ESP32-native sprite texel probe\n");
        return 0;
    }

    printf("[MAPSTRUCT] Native sprite texel random-access probe complete; normalizing native palette\n");

    if (!DoomRPG_prepareNativePalette(render)) {
        printf("[MAPSTRUCT] FAILED ESP32-native palette normalization\n");
        return 0;
    }

    printf("[MAPSTRUCT] Native RGB565 palette ready; rendering one native sprite\n");

    if (!DoomRPG_probeNativeSpriteRenderConsumer(render)) {
        printf("[MAPSTRUCT] FAILED ESP32-native sprite render consumer\n");
        return 0;
    }

    printf("[MAPSTRUCT] Native sprite render consumer complete; rendering one native wall texture\n");

    if (!DoomRPG_probeNativeWallRenderConsumer(render)) {
        printf("[MAPSTRUCT] FAILED ESP32-native wall render consumer\n");
        return 0;
    }

    printf("[MAPSTRUCT] Native wall render consumer complete; full map texel loading remains blocked\n");
    probeReady = 1;
    return 1;
}
