#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Game.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Render.h"

#include "native_intro_first_frame.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

/* Keep ESP-IDF's stdbool macros after DoomRPG's legacy boolean enum. */
#include <esp_heap_caps.h>

#define EXPECTED_INTRO_ENTRY_FNV 0x485915c5U
#define INTRO_FIRST_FRAME_TIME_MS 0

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

static int introBoundaryIsSafe(const DoomRPG_t* doomRpg) {
    const DoomCanvas_t* canvas;
    const Render_t* render;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->game == NULL ||
        doomRpg->menuSystem == NULL) {
        return 0;
    }

    canvas = doomRpg->doomCanvas;
    render = doomRpg->render;

    return canvas->state == ST_INTRO &&
           doomRpg->menuSystem->menu == MENU_NONE &&
           canvas->storyPage == 0 &&
           canvas->storyTextPage == 0 &&
           canvas->storyText1[0] != NULL &&
           canvas->storyText1[1] != NULL &&
           canvas->storyText2 != NULL &&
           canvas->imgSpaceBG.imgBitmap != NULL &&
           canvas->imgLinesLayer.imgBitmap != NULL &&
           canvas->imgPlanetLayer.imgBitmap != NULL &&
           canvas->imgSpaceship.imgBitmap != NULL &&
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

int DoomRPG_esp32RenderFirstIntroFrame(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    DoomCanvas_t* canvas;
    Render_t* render;
    uint32_t inputHash;
    uint32_t outputHash;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;

    printf("\n=== Doom RPG ESP32 bounded first ST_INTRO frame ===\n");

    if (!introBoundaryIsSafe(doomRpg)) {
        printf("[INTRO1] FAILED intro boundary unavailable before draw\n");
        return 0;
    }

    canvas = doomRpg->doomCanvas;
    render = doomRpg->render;
    inputHash = framebufferHash();
    heapBefore = heap8Free();
    largestBefore = largest8Block();

    printf("[INTRO1] Begin state=%d menu=%d page=%d textPage=%d frameFNV=%08x expectedEntry=%08x heap8=%u largest8=%u\n",
           canvas->state,
           doomRpg->menuSystem->menu,
           canvas->storyPage,
           canvas->storyTextPage,
           (unsigned int)inputHash,
           (unsigned int)EXPECTED_INTRO_ENTRY_FNV,
           (unsigned int)heapBefore,
           (unsigned int)largestBefore);

    if (inputHash != EXPECTED_INTRO_ENTRY_FNV) {
        printf("[INTRO1] FAILED entry framebuffer changed got=%08x expected=%08x\n",
               (unsigned int)inputHash,
               (unsigned int)EXPECTED_INTRO_ENTRY_FNV);
        return 0;
    }

    /* DoomCanvas_drawStory() lazily initializes these timers from global
     * uptime. For this first bounded ESP32 proof we instead establish a local,
     * deterministic intro epoch. The next milestone will own a real intro clock.
     */
    canvas->time = INTRO_FIRST_FRAME_TIME_MS;
    canvas->storyTextTime = INTRO_FIRST_FRAME_TIME_MS;
    canvas->storyAnimTime = INTRO_FIRST_FRAME_TIME_MS;
    canvas->showTextDone = false;

    DoomCanvas_drawStory(canvas);

    outputHash = framebufferHash();
    heapAfter = heap8Free();
    largestAfter = largest8Block();

    printf("[INTRO1] Drawn t=%d frameFNV=%08x heap8=%u largest8=%u deltaHeap=%d deltaLargest=%d state=%d page=%d textPage=%d\n",
           INTRO_FIRST_FRAME_TIME_MS,
           (unsigned int)outputHash,
           (unsigned int)heapAfter,
           (unsigned int)largestAfter,
           (int)heapBefore - (int)heapAfter,
           (int)largestBefore - (int)largestAfter,
           canvas->state,
           canvas->storyPage,
           canvas->storyTextPage);

    if (!introBoundaryIsSafe(doomRpg)) {
        printf("[INTRO1] FAILED boundary changed after draw nodes=%p lines=%p mapSprites=%p shapeData=%p mediaTexels=%p wallCache=%d spriteCache=%d\n",
               (void*)render->nodes,
               (void*)render->lines,
               (void*)render->mapSprites,
               (void*)render->shapeData,
               (void*)render->mediaTexels,
               EspNativeWallCache_isActive(),
               EspNativeSpriteCache_isActive());
        return 0;
    }

    if (outputHash == 0U || outputHash == inputHash) {
        printf("[INTRO1] FAILED draw framebuffer invalid/unchanged FNV=%08x\n",
               (unsigned int)outputHash);
        return 0;
    }

    DoomRPG_flushGraphics(doomRpg);

    printf("[INTRO1] READY one deterministic ST_INTRO frame presented once FNV=%08x\n",
           (unsigned int)outputHash);
    printf("[INTRO1] PARK state=%d page=%d textPage=%d; no DoomCanvas_run, no input dispatch, no map load\n",
           canvas->state,
           canvas->storyPage,
           canvas->storyTextPage);
    return 1;
}
