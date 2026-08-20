#include <SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Render.h"

#include "native_intro_clock.h"
#include "native_intro_dispose.h"
#include "native_intro_input.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#include <esp_heap_caps.h>

typedef struct Esp32IntroDisposeState_s {
    int attempted;
    int done;
} Esp32IntroDisposeState;

static Esp32IntroDisposeState disposeState;

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

static int runtimePoolsAreReleased(const Render_t* render) {
    return render != NULL &&
           render->nodes == NULL &&
           render->lines == NULL &&
           render->mapSprites == NULL &&
           render->mediaTexelOffsets == NULL &&
           render->mediaBitShapeOffsets == NULL &&
           render->mapTextureTexels == NULL &&
           render->mapSpriteTexels == NULL &&
           render->shapeData == NULL &&
           render->mediaTexels == NULL &&
           !EspNativeWallCache_isActive() &&
           !EspNativeSpriteCache_isActive();
}

static int preDisposeBoundaryIsSafe(const DoomRPG_t* doomRpg) {
    const DoomCanvas_t* canvas;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->menuSystem == NULL) {
        return 0;
    }

    canvas = doomRpg->doomCanvas;

    return !Esp32IntroClock_isActive() &&
           !Esp32IntroInput_isActive() &&
           doomRpg->menuSystem->menu == MENU_NONE &&
           canvas->state == ST_INTRO &&
           canvas->storyPage == 2 &&
           canvas->storyTextPage == 0 &&
           canvas->showTextDone &&
           canvas->storyText1[0] != NULL &&
           canvas->storyText1[1] != NULL &&
           canvas->storyText2 != NULL &&
           canvas->imgSpaceBG.imgBitmap != NULL &&
           canvas->imgLinesLayer.imgBitmap != NULL &&
           canvas->imgPlanetLayer.imgBitmap != NULL &&
           canvas->imgSpaceship.imgBitmap != NULL &&
           runtimePoolsAreReleased(doomRpg->render);
}

static int postDisposeBoundaryIsSafe(const DoomRPG_t* doomRpg) {
    const DoomCanvas_t* canvas;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->menuSystem == NULL) {
        return 0;
    }

    canvas = doomRpg->doomCanvas;

    return !Esp32IntroClock_isActive() &&
           !Esp32IntroInput_isActive() &&
           doomRpg->menuSystem->menu == MENU_NONE &&
           canvas->state == ST_INTRO &&
           canvas->storyPage == 3 &&
           canvas->storyTextPage == 0 &&
           canvas->storyText1[0] == NULL &&
           canvas->storyText1[1] == NULL &&
           canvas->storyText2 == NULL &&
           canvas->imgSpaceBG.imgBitmap == NULL &&
           canvas->imgLinesLayer.imgBitmap == NULL &&
           canvas->imgPlanetLayer.imgBitmap == NULL &&
           canvas->imgSpaceship.imgBitmap == NULL &&
           !doomRpg->graphSetCliping &&
           runtimePoolsAreReleased(doomRpg->render);
}

static void freeImageMeasured(DoomRPG_t* doomRpg,
                              Image_t* image,
                              const char* label) {
    const uint32_t heapBefore = heap8Free();
    const uint32_t largestBefore = largest8Block();

    DoomRPG_freeImage(doomRpg, image);

    printf("[INTRODISP] FREE image=%s heap8=%u->%u gain=%d largest8=%u->%u ptr=%p\n",
           label,
           (unsigned int)heapBefore,
           (unsigned int)heap8Free(),
           (int)heap8Free() - (int)heapBefore,
           (unsigned int)largestBefore,
           (unsigned int)largest8Block(),
           (void*)image->imgBitmap);
}

static void freeTextMeasured(char** text, const char* label) {
    const uint32_t heapBefore = heap8Free();
    const uint32_t largestBefore = largest8Block();
    const size_t bytes = *text != NULL ? SDL_strlen(*text) + 1U : 0U;

    SDL_free(*text);
    *text = NULL;

    printf("[INTRODISP] FREE text=%s bytes=%u heap8=%u->%u gain=%d largest8=%u->%u ptr=%p\n",
           label,
           (unsigned int)bytes,
           (unsigned int)heapBefore,
           (unsigned int)heap8Free(),
           (int)heap8Free() - (int)heapBefore,
           (unsigned int)largestBefore,
           (unsigned int)largest8Block(),
           (void*)*text);
}

void Esp32IntroDispose_reset(void) {
    disposeState.attempted = 0;
    disposeState.done = 0;
}

void Esp32IntroDispose_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    DoomCanvas_t* canvas;
    Render_t* render;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint32_t frameBefore;
    uint32_t frameAfter;

    if (disposeState.done || disposeState.attempted || doomRpg == NULL) {
        return;
    }

    if (!preDisposeBoundaryIsSafe(doomRpg)) {
        canvas = doomRpg->doomCanvas;
        render = doomRpg->render;
        disposeState.attempted = 1;
        printf("[INTRODISP] FAILED precondition clock=%d input=%d menu=%d state=%d page=%d textPage=%d textDone=%d heap8=%u largest8=%u shapeData=%p mediaTexels=%p\n",
               Esp32IntroClock_isActive(),
               Esp32IntroInput_isActive(),
               doomRpg->menuSystem != NULL ? doomRpg->menuSystem->menu : -1,
               canvas != NULL ? canvas->state : -1,
               canvas != NULL ? canvas->storyPage : -1,
               canvas != NULL ? canvas->storyTextPage : -1,
               canvas != NULL && canvas->showTextDone ? 1 : 0,
               (unsigned int)heap8Free(),
               (unsigned int)largest8Block(),
               render != NULL ? (void*)render->shapeData : NULL,
               render != NULL ? (void*)render->mediaTexels : NULL);
        return;
    }

    disposeState.attempted = 1;
    canvas = doomRpg->doomCanvas;
    heapBefore = heap8Free();
    largestBefore = largest8Block();
    frameBefore = framebufferHash();

    printf("\n=== Doom RPG ESP32 bounded intro disposal ===\n");
    printf("[INTRODISP] BEGIN state=%d page=%d textPage=%d startupMap=%d frameFNV=%08x heap8=%u largest8=%u clip=%d\n",
           canvas->state,
           canvas->storyPage,
           canvas->storyTextPage,
           canvas->startupMap,
           (unsigned int)frameBefore,
           (unsigned int)heapBefore,
           (unsigned int)largestBefore,
           doomRpg->graphSetCliping ? 1 : 0);
    printf("[INTRODISP] CONTRACT mirror DoomCanvas_disposeIntro resources only; DoomCanvas_loadMap is forbidden\n");

    /* DoomCanvas_changeStoryPage() increments to 3 before calling the original
     * disposer. Preserve that state transition while keeping map loading out.
     */
    canvas->storyPage = 3;

    freeImageMeasured(doomRpg, &canvas->imgSpaceBG, "c.bmp/imgSpaceBG");
    freeImageMeasured(doomRpg, &canvas->imgLinesLayer, "d.bmp/imgLinesLayer");
    freeImageMeasured(doomRpg, &canvas->imgPlanetLayer, "e.bmp/imgPlanetLayer");
    freeImageMeasured(doomRpg, &canvas->imgSpaceship, "f.bmp/imgSpaceship");

    freeTextMeasured(&canvas->storyText1[0], "storyText1[0]");
    freeTextMeasured(&canvas->storyText1[1], "storyText1[1]");
    freeTextMeasured(&canvas->storyText2, "storyText2");

    DoomRPG_setClipFalse(doomRpg);

    heapAfter = heap8Free();
    largestAfter = largest8Block();
    frameAfter = framebufferHash();

    if (!postDisposeBoundaryIsSafe(doomRpg) ||
        heapAfter <= heapBefore ||
        largestAfter < largestBefore ||
        frameAfter != frameBefore) {
        printf("[INTRODISP] FAILED postcondition state=%d page=%d textPage=%d frameFNV=%08x->%08x heap8=%u->%u largest8=%u->%u shapeData=%p mediaTexels=%p\n",
               canvas->state,
               canvas->storyPage,
               canvas->storyTextPage,
               (unsigned int)frameBefore,
               (unsigned int)frameAfter,
               (unsigned int)heapBefore,
               (unsigned int)heapAfter,
               (unsigned int)largestBefore,
               (unsigned int)largestAfter,
               (void*)doomRpg->render->shapeData,
               (void*)doomRpg->render->mediaTexels);
        return;
    }

    disposeState.done = 1;
    printf("[INTRODISP] READY state=%d page=%d textPage=%d frameFNV=%08x->%08x heap8=%u->%u recovered=%d largest8=%u->%u assets=NULL texts=NULL clip=off noMapLoad=yes\n",
           canvas->state,
           canvas->storyPage,
           canvas->storyTextPage,
           (unsigned int)frameBefore,
           (unsigned int)frameAfter,
           (unsigned int)heapBefore,
           (unsigned int)heapAfter,
           (int)heapAfter - (int)heapBefore,
           (unsigned int)largestBefore,
           (unsigned int)largestAfter);
    printf("[INTRODISP] PARK state=%d startupMap=%d shapeData=%p mediaTexels=%p nodes=%p lines=%p mapSprites=%p; next milestone owns map loading\n",
           canvas->state,
           canvas->startupMap,
           (void*)doomRpg->render->shapeData,
           (void*)doomRpg->render->mediaTexels,
           (void*)doomRpg->render->nodes,
           (void*)doomRpg->render->lines,
           (void*)doomRpg->render->mapSprites);
}

int Esp32IntroDispose_isDone(void) {
    return disposeState.done;
}
