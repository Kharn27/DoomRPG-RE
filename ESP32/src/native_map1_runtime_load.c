#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Game.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Render.h"

#include "esp_bsp_reader.h"
#include "esp_map_runtime.h"
#include "native_intro_clock.h"
#include "native_intro_dispose.h"
#include "native_intro_input.h"
#include "native_map1_bsp_pass1.h"
#include "native_map1_runtime_load.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#include <esp_heap_caps.h>

#define EXPECTED_INTRO_BSP_BYTES 21823U
#define EXPECTED_INTRO_CRC32 0x623f34e4U
#define EXPECTED_COMPACT_PLAN_BYTES 14095U

typedef struct Esp32Map1RuntimeLoadState_s {
    int armed;
    int attempted;
    int done;
} Esp32Map1RuntimeLoadState;

static Esp32Map1RuntimeLoadState loadState;

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

static uint32_t framebufferHash(void) {
    const uint8_t* framebuffer =
        (const uint8_t*)Esp32PlatformVideo_framebuffer();
    const size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();

    if (framebuffer == NULL ||
        bytes != (size_t)DOOMRPG_LOGICAL_WIDTH *
                     (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t)) {
        return 0U;
    }

    return fnv1a32(framebuffer, (uint32_t)bytes);
}

static int introResourcesAreReleased(const DoomCanvas_t* canvas) {
    return canvas != NULL &&
           canvas->imgSpaceBG.imgBitmap == NULL &&
           canvas->imgLinesLayer.imgBitmap == NULL &&
           canvas->imgPlanetLayer.imgBitmap == NULL &&
           canvas->imgSpaceship.imgBitmap == NULL &&
           canvas->storyText1[0] == NULL &&
           canvas->storyText1[1] == NULL &&
           canvas->storyText2 == NULL;
}

static int legacyRuntimeIsClear(const Render_t* render) {
    return render != NULL &&
           render->nodes == NULL &&
           render->lines == NULL &&
           render->mapSprites == NULL &&
           render->tileEvents == NULL &&
           render->mapByteCode == NULL &&
           render->mapStringsIDs == NULL &&
           render->mediaTexelOffsets == NULL &&
           render->mediaBitShapeOffsets == NULL &&
           render->mediaTexturesIds == NULL &&
           render->mediaSpriteIds == NULL &&
           render->mapTextureTexels == NULL &&
           render->mapSpriteTexels == NULL &&
           render->shapeData == NULL &&
           render->mediaTexels == NULL &&
           render->ioBuffer == NULL &&
           !EspNativeWallCache_isActive() &&
           !EspNativeSpriteCache_isActive();
}

static int logicalBoundaryIsSafe(const DoomRPG_t* doomRpg) {
    const DoomCanvas_t* canvas;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->game == NULL ||
        doomRpg->menuSystem == NULL) {
        return 0;
    }

    canvas = doomRpg->doomCanvas;
    return Esp32IntroDispose_isDone() &&
           Esp32Map1BspPass1_isDone() &&
           !Esp32IntroClock_isActive() &&
           !Esp32IntroInput_isActive() &&
           doomRpg->menuSystem->menu == MENU_NONE &&
           canvas->state == ST_INTRO &&
           canvas->storyPage == 3 &&
           canvas->storyTextPage == 0 &&
           canvas->startupMap == MAP_INTRO &&
           introResourcesAreReleased(canvas) &&
           legacyRuntimeIsClear(doomRpg->render) &&
           doomRpg->game->numEntities == 0 &&
           doomRpg->game->numMonsters == 0;
}

static int inventoryIsUsable(const EspBspInventory* inventory) {
    return inventory != NULL &&
           inventory->sourceBytes == EXPECTED_INTRO_BSP_BYTES &&
           inventory->consumedBytes == EXPECTED_INTRO_BSP_BYTES &&
           inventory->crc32 == EXPECTED_INTRO_CRC32 &&
           inventory->crc32 == inventory->expectedCrc32 &&
           inventory->trailingBytes == 0U &&
           inventory->plan.persistentBytes == EXPECTED_COMPACT_PLAN_BYTES &&
           inventory->lineTextureIdsAbove255 == 0U &&
           inventory->textureResourceIdsAbove255 == 0U &&
           inventory->spriteResourceIdsAbove255 == 0U;
}

void Esp32Map1RuntimeLoad_reset(void) {
    SDL_memset(&loadState, 0, sizeof(loadState));
    EspMapRuntime_reset();
}

void Esp32Map1RuntimeLoad_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    DoomCanvas_t* canvas;
    Render_t* render;
    EspBspInventory inventory;
    const EspMapRuntimeView* view;
    const char* mapFile;
    uint32_t inventoryStartedMs;
    uint32_t inventoryElapsedMs;
    uint32_t populateStartedMs;
    uint32_t populateElapsedMs;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint32_t frameBefore;
    uint32_t frameAfter;
    uint32_t heapUsed;
    int32_t allocatorOverhead;

    if (loadState.done || loadState.attempted || doomRpg == NULL) {
        return;
    }

    if (!Esp32Map1BspPass1_isDone()) {
        return;
    }

    if (!loadState.armed) {
        loadState.armed = 1;
        printf("[NATIVEMAP] ARMED pass1 complete; compact arena allocation+population starts on next loop service\n");
        return;
    }

    loadState.attempted = 1;
    canvas = doomRpg->doomCanvas;
    render = doomRpg->render;

    printf("\n=== Doom RPG ESP32-native MAP_INTRO resident structural base ===\n");

    if (!logicalBoundaryIsSafe(doomRpg) || EspMapRuntime_isLoaded()) {
        printf("[NATIVEMAP] FAILED precondition state=%d page=%d heap8=%u largest8=%u legacyClear=%d nativeLoaded=%d entities=%d monsters=%d\n",
               canvas != NULL ? canvas->state : -1,
               canvas != NULL ? canvas->storyPage : -1,
               (unsigned int)heap8Free(),
               (unsigned int)largest8Block(),
               legacyRuntimeIsClear(render),
               EspMapRuntime_isLoaded(),
               doomRpg->game != NULL ? doomRpg->game->numEntities : -1,
               doomRpg->game != NULL ? doomRpg->game->numMonsters : -1);
        return;
    }

    mapFile = doomRpg->game->mapFiles[canvas->startupMap - 1];
    if (mapFile == NULL || SDL_strcasecmp(mapFile, "/intro.bsp") != 0) {
        printf("[NATIVEMAP] FAILED startupMap=%d resolves to '%s'\n",
               canvas->startupMap,
               mapFile != NULL ? mapFile : "<null>");
        return;
    }

    printf("[NATIVEMAP] CONTRACT pass1 regression -> one %uB immutable arena -> direct .pak section reads; strings stay on SD; entities/gameplay/render forbidden\n",
           (unsigned int)EXPECTED_COMPACT_PLAN_BYTES);

    inventoryStartedMs = DoomRPG_GetUpTimeMS();
    if (!EspBspReader_inventoryPackEntry(mapFile, &inventory)) {
        printf("[NATIVEMAP] FAILED inventory refresh\n");
        return;
    }
    inventoryElapsedMs = DoomRPG_GetUpTimeMS() - inventoryStartedMs;

    if (!inventoryIsUsable(&inventory)) {
        printf("[NATIVEMAP] FAILED inventory regression bytes=%u crc=%08x plan=%u overflow=%u/%u/%u\n",
               (unsigned int)inventory.sourceBytes,
               (unsigned int)inventory.crc32,
               (unsigned int)inventory.plan.persistentBytes,
               (unsigned int)inventory.lineTextureIdsAbove255,
               (unsigned int)inventory.textureResourceIdsAbove255,
               (unsigned int)inventory.spriteResourceIdsAbove255);
        return;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();
    frameBefore = framebufferHash();
    populateStartedMs = DoomRPG_GetUpTimeMS();

    printf("[NATIVEMAP] BEGIN file=%s inventory=%ums heap8=%u largest8=%u frameFNV=%08x plan=%u\n",
           mapFile,
           (unsigned int)inventoryElapsedMs,
           (unsigned int)heapBefore,
           (unsigned int)largestBefore,
           (unsigned int)frameBefore,
           (unsigned int)inventory.plan.persistentBytes);

    if (!EspMapRuntime_loadPackEntry(mapFile, &inventory)) {
        printf("[NATIVEMAP] FAILED native runtime population heap8=%u largest8=%u\n",
               (unsigned int)heap8Free(),
               (unsigned int)largest8Block());
        return;
    }

    populateElapsedMs = DoomRPG_GetUpTimeMS() - populateStartedMs;
    heapAfter = heap8Free();
    largestAfter = largest8Block();
    frameAfter = framebufferHash();
    view = EspMapRuntime_view();

    heapUsed = heapBefore >= heapAfter ? heapBefore - heapAfter : 0U;
    allocatorOverhead = (int32_t)heapUsed - (int32_t)EXPECTED_COMPACT_PLAN_BYTES;

    if (view == NULL ||
        view->arenaBytes != EXPECTED_COMPACT_PLAN_BYTES ||
        heapAfter >= heapBefore ||
        heapUsed < EXPECTED_COMPACT_PLAN_BYTES ||
        frameAfter != frameBefore ||
        !logicalBoundaryIsSafe(doomRpg) ||
        !legacyRuntimeIsClear(render) ||
        doomRpg->game->numEntities != 0 ||
        doomRpg->game->numMonsters != 0) {
        printf("[NATIVEMAP] FAILED postcondition arena=%u heap8=%u->%u used=%u largest8=%u->%u frame=%08x->%08x legacyClear=%d\n",
               view != NULL ? (unsigned int)view->arenaBytes : 0U,
               (unsigned int)heapBefore,
               (unsigned int)heapAfter,
               (unsigned int)heapUsed,
               (unsigned int)largestBefore,
               (unsigned int)largestAfter,
               (unsigned int)frameBefore,
               (unsigned int)frameAfter,
               legacyRuntimeIsClear(render));
        EspMapRuntime_reset();
        printf("[NATIVEMAP] CLEANUP heap8=%u largest8=%u\n",
               (unsigned int)heap8Free(),
               (unsigned int)largest8Block());
        return;
    }

    loadState.done = 1;
    printf("[NATIVEMAP] READY arenaBytes=%u arenaFNV=%08x populateReadCalls=%u populateElapsed=%ums\n",
           (unsigned int)view->arenaBytes,
           (unsigned int)view->arenaFNV1a,
           (unsigned int)view->populateReadCalls,
           (unsigned int)populateElapsedMs);
    printf("[NATIVEMAP] RESIDENT nodes=%u/%uB lines=%u/%uB sprites=%u/%uB events=%u/%uB byteCodes=%u/%uB strings=%u/%uB blockMap=%uB planes=%uB resources=%uB\n",
           (unsigned int)view->nodeCount,
           (unsigned int)view->nodeBytes,
           (unsigned int)view->lineCount,
           (unsigned int)view->lineBytes,
           (unsigned int)view->mapSpriteCount,
           (unsigned int)view->mapSpriteBytes,
           (unsigned int)view->eventCount,
           (unsigned int)view->eventBytes,
           (unsigned int)view->byteCodeCount,
           (unsigned int)view->byteCodeBytes,
           (unsigned int)view->stringCount,
           (unsigned int)view->stringOffsetsBytes,
           (unsigned int)view->blockMapBytes,
           (unsigned int)view->planeMapBytes,
           (unsigned int)(3U * ESP_MAP_RUNTIME_RESOURCE_SET_BYTES));
    printf("[NATIVEMAP] RAM heap8=%u->%u used=%u payload=%u allocatorOverhead=%d largest8=%u->%u frameFNV=%08x->%08x\n",
           (unsigned int)heapBefore,
           (unsigned int)heapAfter,
           (unsigned int)heapUsed,
           (unsigned int)view->arenaBytes,
           (int)allocatorOverhead,
           (unsigned int)largestBefore,
           (unsigned int)largestAfter,
           (unsigned int)frameBefore,
           (unsigned int)frameAfter);
    printf("[NATIVEMAP] PARK state=%d page=%d startupMap=%d nativeArena=yes legacyRuntime=NULL shapeData=%p mediaTexels=%p entities=%d monsters=%d noGameplay=yes\n",
           canvas->state,
           canvas->storyPage,
           canvas->startupMap,
           (void*)render->shapeData,
           (void*)render->mediaTexels,
           doomRpg->game->numEntities,
           doomRpg->game->numMonsters);
}

int Esp32Map1RuntimeLoad_isDone(void) {
    return loadState.done;
}
