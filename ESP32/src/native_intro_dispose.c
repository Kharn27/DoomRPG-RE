#include <SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Render.h"

#include "esp_native_intro_state.h"
#include "native_intro_clock.h"
#include "native_intro_dispose.h"
#include "native_intro_input.h"
#include "native_story_fit.h"
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
    const EspNativeIntroState_t* introState;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->menuSystem == NULL) {
        return 0;
    }

    canvas = doomRpg->doomCanvas;
    introState = EspNativeIntroState_view(doomRpg);

    return !Esp32IntroClock_isActive() &&
           !Esp32IntroInput_isActive() &&
           doomRpg->menuSystem->menu == MENU_NONE &&
           canvas->state == ST_INTRO &&
           introState != NULL &&
           introState->storyPage == 2 &&
           introState->storyTextPage == 0 &&
           introState->showTextDone &&
           introState->storyText1[0] != NULL &&
           introState->storyText1[1] != NULL &&
           introState->storyText2 != NULL &&
           introState->imgSpaceBG.imgBitmap != NULL &&
           introState->imgLinesLayer.imgBitmap != NULL &&
           introState->imgPlanetLayer.imgBitmap != NULL &&
           introState->imgSpaceship.imgBitmap != NULL &&
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
           EspNativeIntroState_view(doomRpg) == NULL &&
           !Esp32StoryFit_hasHand() &&
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
    EspNativeIntroState_t* introState;
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
        introState = EspNativeIntroState_get(doomRpg);
        render = doomRpg->render;
        disposeState.attempted = 1;
        printf("[INTRODISP] FAILED precondition clock=%d input=%d menu=%d state=%d page=%d textPage=%d textDone=%d heap8=%u largest8=%u shapeData=%p mediaTexels=%p\n",
               Esp32IntroClock_isActive(),
               Esp32IntroInput_isActive(),
               doomRpg->menuSystem != NULL ? doomRpg->menuSystem->menu : -1,
               canvas != NULL ? canvas->state : -1,
               introState != NULL ? introState->storyPage : -1,
               introState != NULL ? introState->storyTextPage : -1,
               introState != NULL && introState->showTextDone ? 1 : 0,
               (unsigned int)heap8Free(),
               (unsigned int)largest8Block(),
               render != NULL ? (void*)render->shapeData : NULL,
               render != NULL ? (void*)render->mediaTexels : NULL);
        return;
    }

    disposeState.attempted = 1;
    canvas = doomRpg->doomCanvas;
    introState = EspNativeIntroState_get(doomRpg);
    if (introState == NULL) {
        disposeState.attempted = 1;
        printf("[INTRODISP] FAILED native transient intro owner missing\n");
        return;
    }
    heapBefore = heap8Free();
    largestBefore = largest8Block();
    frameBefore = framebufferHash();

    printf("\n=== Doom RPG ESP32 bounded intro disposal ===\n");
    printf("[INTRODISP] BEGIN state=%d page=%d textPage=%d startupMap=%d frameFNV=%08x heap8=%u largest8=%u clip=%d owner=native-transient\n",
           canvas->state,
           introState->storyPage,
           introState->storyTextPage,
           canvas->startupMap,
           (unsigned int)frameBefore,
           (unsigned int)heapBefore,
           (unsigned int)largestBefore,
           doomRpg->graphSetCliping ? 1 : 0);
    printf("[INTRODISP] CONTRACT mirror DoomCanvas_disposeIntro resources only; DoomCanvas_loadMap is forbidden\n");

    /* DoomCanvas_changeStoryPage() increments to 3 before calling the original
     * disposer. Preserve that state transition while keeping map loading out.
     */
    introState->storyPage = 3;

    {
        const uint32_t before = heap8Free();
        Esp32StoryFit_release(canvas);
        printf("[INTRODISP] FREE image=p.bmp/storyHand heap8=%u->%u gain=%d owner=native-story\n",
               (unsigned int)before,
               (unsigned int)heap8Free(),
               (int)heap8Free() - (int)before);
    }

    {
        const uint32_t before = heap8Free();
        EspNativeIntroState_release(doomRpg);
        printf("[INTRODISP] FREE owner=native-intro-state stateBytes=96 heap8=%u->%u gain=%d state=NULL\n",
               (unsigned int)before,
               (unsigned int)heap8Free(),
               (int)heap8Free() - (int)before);
    }
    introState = NULL;

    DoomRPG_setClipFalse(doomRpg);

    heapAfter = heap8Free();
    largestAfter = largest8Block();
    frameAfter = framebufferHash();

    if (!postDisposeBoundaryIsSafe(doomRpg) ||
        heapAfter <= heapBefore ||
        largestAfter < largestBefore ||
        frameAfter != frameBefore) {
        printf("[INTRODISP] FAILED postcondition state=%d page=3 textPage=0 frameFNV=%08x->%08x heap8=%u->%u largest8=%u->%u shapeData=%p mediaTexels=%p\n",
               canvas->state,
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
    printf("[INTRODISP] READY state=%d page=3 textPage=0 frameFNV=%08x->%08x heap8=%u->%u recovered=%d largest8=%u->%u owner=NULL assets=NULL texts=NULL clip=off noMapLoad=yes\n",
           canvas->state,
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
