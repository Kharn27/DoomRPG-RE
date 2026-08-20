#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Game.h"
#include "MenuSystem.h"
#include "Render.h"

#include "esp_bsp_reader.h"
#include "native_intro_clock.h"
#include "native_intro_dispose.h"
#include "native_intro_input.h"
#include "native_map1_bsp_pass1.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

/* Keep ESP-IDF stdbool macros after DoomRPG's legacy boolean enum. */
#include <esp_heap_caps.h>

#define EXPECTED_INTRO_BSP_BYTES 21823U
#define EXPECTED_INTRO_NODES 223U
#define EXPECTED_INTRO_LINES 480U
#define EXPECTED_INTRO_MAP_SPRITES 344U
#define EXPECTED_INTRO_EVENTS 93U
#define EXPECTED_INTRO_BYTECODES 265U
#define EXPECTED_INTRO_STRINGS 94U
#define EXPECTED_INTRO_STRING_DATA_BYTES 7779U
#define EXPECTED_INTRO_LEGACY_STRING_ALLOC_BYTES 7873U

typedef struct Esp32Map1BspPass1State_s {
    int armed;
    int attempted;
    int done;
} Esp32Map1BspPass1State;

static Esp32Map1BspPass1State pass1State;

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

static int runtimeIsClear(const Render_t* render) {
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

static int preBoundaryIsSafe(const DoomRPG_t* doomRpg) {
    const DoomCanvas_t* canvas;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->game == NULL ||
        doomRpg->menuSystem == NULL) {
        return 0;
    }

    canvas = doomRpg->doomCanvas;
    return Esp32IntroDispose_isDone() &&
           !Esp32IntroClock_isActive() &&
           !Esp32IntroInput_isActive() &&
           doomRpg->menuSystem->menu == MENU_NONE &&
           canvas->state == ST_INTRO &&
           canvas->storyPage == 3 &&
           canvas->storyTextPage == 0 &&
           canvas->startupMap == MAP_INTRO &&
           introResourcesAreReleased(canvas) &&
           runtimeIsClear(doomRpg->render) &&
           doomRpg->game->numEntities == 0 &&
           doomRpg->game->numMonsters == 0;
}

static int inventoryMatchesMeasuredIntro(const EspBspInventory* inventory) {
    return inventory != NULL &&
           inventory->sourceBytes == EXPECTED_INTRO_BSP_BYTES &&
           inventory->consumedBytes == EXPECTED_INTRO_BSP_BYTES &&
           inventory->structuralEndOffset == EXPECTED_INTRO_BSP_BYTES &&
           inventory->trailingBytes == 0U &&
           inventory->nodes == EXPECTED_INTRO_NODES &&
           inventory->lines == EXPECTED_INTRO_LINES &&
           inventory->mapSprites == EXPECTED_INTRO_MAP_SPRITES &&
           inventory->events == EXPECTED_INTRO_EVENTS &&
           inventory->byteCodes == EXPECTED_INTRO_BYTECODES &&
           inventory->strings == EXPECTED_INTRO_STRINGS &&
           inventory->stringDataBytes == EXPECTED_INTRO_STRING_DATA_BYTES &&
           inventory->legacyStringAllocationBytes ==
               EXPECTED_INTRO_LEGACY_STRING_ALLOC_BYTES &&
           inventory->crc32 == inventory->expectedCrc32;
}

void Esp32Map1BspPass1_reset(void) {
    SDL_memset(&pass1State, 0, sizeof(pass1State));
}

void Esp32Map1BspPass1_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    DoomCanvas_t* canvas;
    Render_t* render;
    EspBspInventory inventory;
    const char* mapFile;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint32_t frameBefore;
    uint32_t frameAfter;

    if (pass1State.done || pass1State.attempted || doomRpg == NULL) {
        return;
    }

    if (!Esp32IntroDispose_isDone()) {
        return;
    }

    if (!pass1State.armed) {
        pass1State.armed = 1;
        printf("[NATIVEBSP1] ARMED post-intro boundary; native .pak inventory starts on next loop service\n");
        return;
    }

    pass1State.attempted = 1;
    canvas = doomRpg->doomCanvas;
    render = doomRpg->render;

    printf("\n=== Doom RPG ESP32-native BSP reader pass 1 ===\n");

    if (!preBoundaryIsSafe(doomRpg)) {
        printf("[NATIVEBSP1] FAILED precondition state=%d page=%d startupMap=%d menu=%d heap8=%u largest8=%u shapeData=%p mediaTexels=%p entities=%d monsters=%d\n",
               canvas != NULL ? canvas->state : -1,
               canvas != NULL ? canvas->storyPage : -1,
               canvas != NULL ? canvas->startupMap : -1,
               doomRpg->menuSystem != NULL ? doomRpg->menuSystem->menu : -1,
               (unsigned int)heap8Free(),
               (unsigned int)largest8Block(),
               render != NULL ? (void*)render->shapeData : NULL,
               render != NULL ? (void*)render->mediaTexels : NULL,
               doomRpg->game != NULL ? doomRpg->game->numEntities : -1,
               doomRpg->game != NULL ? doomRpg->game->numMonsters : -1);
        return;
    }

    mapFile = doomRpg->game->mapFiles[canvas->startupMap - 1];
    if (mapFile == NULL || SDL_strcasecmp(mapFile, "/intro.bsp") != 0) {
        printf("[NATIVEBSP1] FAILED startupMap=%d resolves to '%s', expected /intro.bsp\n",
               canvas->startupMap,
               mapFile != NULL ? mapFile : "<null>");
        return;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();
    frameBefore = framebufferHash();

    printf("[NATIVEBSP1] BEGIN state=%d page=%d mapId=%d file=%s heap8=%u largest8=%u frameFNV=%08x\n",
           canvas->state,
           canvas->storyPage,
           canvas->startupMap,
           mapFile,
           (unsigned int)heapBefore,
           (unsigned int)largestBefore,
           (unsigned int)frameBefore);
    printf("[NATIVEBSP1] CONTRACT .pak -> 256B reader window -> scalar inventory only; no ZIP inflate, mappings, map runtime, bitshapes, texels or entities\n");

    if (!EspBspReader_inventoryPackEntry(mapFile, &inventory)) {
        printf("[NATIVEBSP1] FAILED native BSP reader\n");
        return;
    }

    heapAfter = heap8Free();
    largestAfter = largest8Block();
    frameAfter = framebufferHash();

    if (!inventoryMatchesMeasuredIntro(&inventory)) {
        printf("[NATIVEBSP1] FAILED inventory regression bytes=%u nodes=%u lines=%u sprites=%u events=%u byteCodes=%u strings=%u stringData=%u legacyStringAlloc=%u trailing=%u\n",
               (unsigned int)inventory.sourceBytes,
               (unsigned int)inventory.nodes,
               (unsigned int)inventory.lines,
               (unsigned int)inventory.mapSprites,
               (unsigned int)inventory.events,
               (unsigned int)inventory.byteCodes,
               (unsigned int)inventory.strings,
               (unsigned int)inventory.stringDataBytes,
               (unsigned int)inventory.legacyStringAllocationBytes,
               (unsigned int)inventory.trailingBytes);
        return;
    }

    if (!preBoundaryIsSafe(doomRpg) ||
        heapAfter != heapBefore ||
        largestAfter != largestBefore ||
        frameAfter != frameBefore) {
        printf("[NATIVEBSP1] FAILED postcondition heap8=%u->%u largest8=%u->%u frameFNV=%08x->%08x runtimeClear=%d\n",
               (unsigned int)heapBefore,
               (unsigned int)heapAfter,
               (unsigned int)largestBefore,
               (unsigned int)largestAfter,
               (unsigned int)frameBefore,
               (unsigned int)frameAfter,
               runtimeIsClear(render));
        return;
    }

    pass1State.done = 1;
    printf("[NATIVEBSP1] READY name='%.16s' bytes=%u nodes=%u lines=%u mapSprites=%u events=%u byteCodes=%u strings=%u stringData=%u maxString=%u\n",
           inventory.mapName,
           (unsigned int)inventory.sourceBytes,
           (unsigned int)inventory.nodes,
           (unsigned int)inventory.lines,
           (unsigned int)inventory.mapSprites,
           (unsigned int)inventory.events,
           (unsigned int)inventory.byteCodes,
           (unsigned int)inventory.strings,
           (unsigned int)inventory.stringDataBytes,
           (unsigned int)inventory.maxStringBytes);
    printf("[NATIVEBSP1] STREAM window=%uB readCalls=%u fnv1a=%08x crc32=%08x verified=yes\n",
           (unsigned int)ESP_BSP_READER_BUFFER_BYTES,
           (unsigned int)inventory.readCalls,
           (unsigned int)inventory.fnv1a32,
           (unsigned int)inventory.crc32);
    printf("[NATIVEBSP1] RAM heap8=%u->%u delta=%d largest8=%u->%u delta=%d frameFNV=%08x->%08x\n",
           (unsigned int)heapBefore,
           (unsigned int)heapAfter,
           (int)heapAfter - (int)heapBefore,
           (unsigned int)largestBefore,
           (unsigned int)largestAfter,
           (int)largestAfter - (int)largestBefore,
           (unsigned int)frameBefore,
           (unsigned int)frameAfter);
    printf("[NATIVEBSP1] PARK state=%d page=%d startupMap=%d mappings=NULL runtime=NULL shapeData=%p mediaTexels=%p entities=%d monsters=%d noLegacyMapLoader=yes\n",
           canvas->state,
           canvas->storyPage,
           canvas->startupMap,
           (void*)render->shapeData,
           (void*)render->mediaTexels,
           doomRpg->game->numEntities,
           doomRpg->game->numMonsters);
}

int Esp32Map1BspPass1_isDone(void) {
    return pass1State.done;
}
