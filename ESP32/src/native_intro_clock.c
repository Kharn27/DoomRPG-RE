#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Game.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Render.h"

#include "native_intro_clock.h"
#include "native_sprite_lru_cache.h"
#include "native_story_fit.h"
#include "native_wall_lru_cache.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#include <esp_heap_caps.h>

#define EXPECTED_FIRST_FITTED_FNV 0x56438966U
#define INTRO_CLOCK_CHECKPOINT_TICKS 20U

typedef struct Esp32IntroClockState_s {
    DoomRPG_t* doomRpg;
    uint32_t wallStartMs;
    uint32_t lastTick;
    uint32_t renderedFrames;
    uint32_t skippedTicks;
    uint32_t armedHeap8;
    uint32_t armedLargest8;
    int active;
    int textDoneLogged;
} Esp32IntroClockState;

static Esp32IntroClockState clockState;

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

static int boundaryIsSafe(const DoomRPG_t* doomRpg) {
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

static void parkClock(const char* reason) {
    DoomCanvas_t* canvas = clockState.doomRpg != NULL
                               ? clockState.doomRpg->doomCanvas
                               : NULL;
    printf("[INTROCLK] PARK reason=%s tick=%u frames=%u skipped=%u state=%d page=%d textPage=%d heap8=%u largest8=%u\n",
           reason != NULL ? reason : "unknown",
           (unsigned int)clockState.lastTick,
           (unsigned int)clockState.renderedFrames,
           (unsigned int)clockState.skippedTicks,
           canvas != NULL ? canvas->state : -1,
           canvas != NULL ? canvas->storyPage : -1,
           canvas != NULL ? canvas->storyTextPage : -1,
           (unsigned int)heap8Free(),
           (unsigned int)largest8Block());
    clockState.active = 0;
}

int Esp32IntroClock_arm(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const uint32_t frameHash = framebufferHash();

    SDL_memset(&clockState, 0, sizeof(clockState));

    if (!boundaryIsSafe(doomRpg)) {
        printf("[INTROCLK] FAILED arm boundary unavailable\n");
        return 0;
    }

    if (frameHash != EXPECTED_FIRST_FITTED_FNV) {
        printf("[INTROCLK] FAILED arm FNV=%08x expected=%08x\n",
               (unsigned int)frameHash,
               (unsigned int)EXPECTED_FIRST_FITTED_FNV);
        return 0;
    }

    clockState.doomRpg = doomRpg;
    clockState.wallStartMs = DoomRPG_GetUpTimeMS();
    clockState.lastTick = 0;
    clockState.renderedFrames = 0;
    clockState.skippedTicks = 0;
    clockState.armedHeap8 = heap8Free();
    clockState.armedLargest8 = largest8Block();
    clockState.active = 1;

    printf("[INTROCLK] ARMED step=%u ms startFNV=%08x heap8=%u largest8=%u wallStart=%u\n",
           (unsigned int)ESP32_INTRO_CLOCK_STEP_MS,
           (unsigned int)frameHash,
           (unsigned int)clockState.armedHeap8,
           (unsigned int)clockState.armedLargest8,
           (unsigned int)clockState.wallStartMs);
    return 1;
}

void Esp32IntroClock_service(void) {
    DoomCanvas_t* canvas;
    uint32_t now;
    uint32_t elapsed;
    uint32_t targetTick;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    int wasTextDone;

    if (!clockState.active) {
        return;
    }

    if (!boundaryIsSafe(clockState.doomRpg)) {
        parkClock("boundary-changed");
        return;
    }

    now = DoomRPG_GetUpTimeMS();
    elapsed = now - clockState.wallStartMs;
    targetTick = elapsed / ESP32_INTRO_CLOCK_STEP_MS;

    if (targetTick <= clockState.lastTick) {
        return;
    }

    if (targetTick > clockState.lastTick + 1U) {
        clockState.skippedTicks += targetTick - clockState.lastTick - 1U;
    }
    clockState.lastTick = targetTick;

    canvas = clockState.doomRpg->doomCanvas;
    canvas->time = (int)(targetTick * ESP32_INTRO_CLOCK_STEP_MS);

    heapBefore = heap8Free();
    largestBefore = largest8Block();
    wasTextDone = canvas->showTextDone != 0;

    Esp32StoryFit_draw(canvas);

    heapAfter = heap8Free();
    largestAfter = largest8Block();
    ++clockState.renderedFrames;

    if (heapAfter != heapBefore || largestAfter != largestBefore) {
        printf("[INTROCLK] FAILED frame allocation tick=%u t=%d heap8=%u->%u largest8=%u->%u\n",
               (unsigned int)targetTick,
               canvas->time,
               (unsigned int)heapBefore,
               (unsigned int)heapAfter,
               (unsigned int)largestBefore,
               (unsigned int)largestAfter);
        parkClock("frame-allocation");
        return;
    }

    if (!boundaryIsSafe(clockState.doomRpg)) {
        parkClock("boundary-changed-after-draw");
        return;
    }

    DoomRPG_flushGraphics(clockState.doomRpg);

    if (clockState.renderedFrames <= 3U ||
        (targetTick % INTRO_CLOCK_CHECKPOINT_TICKS) == 0U) {
        printf("[INTROCLK] frame=%u tick=%u t=%d FNV=%08x heap8=%u largest8=%u skipped=%u textDone=%d\n",
               (unsigned int)clockState.renderedFrames,
               (unsigned int)targetTick,
               canvas->time,
               (unsigned int)framebufferHash(),
               (unsigned int)heapAfter,
               (unsigned int)largestAfter,
               (unsigned int)clockState.skippedTicks,
               canvas->showTextDone ? 1 : 0);
    }

    if (!wasTextDone && canvas->showTextDone && !clockState.textDoneLogged) {
        clockState.textDoneLogged = 1;
        printf("[INTROCLK] TEXT DONE tick=%u t=%d frames=%u skipped=%u heap8=%u largest8=%u\n",
               (unsigned int)targetTick,
               canvas->time,
               (unsigned int)clockState.renderedFrames,
               (unsigned int)clockState.skippedTicks,
               (unsigned int)heapAfter,
               (unsigned int)largestAfter);
    }
}

int Esp32IntroClock_isActive(void) {
    return clockState.active;
}
