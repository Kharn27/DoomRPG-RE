#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Render.h"

#include "native_intro_clock.h"
#include "native_intro_input.h"
#include "native_sprite_lru_cache.h"
#include "native_story_fit.h"
#include "native_wall_lru_cache.h"
#include "platform_touch_events.h"
#include "platform_video_config.h"

#include <esp_heap_caps.h>

#define INTRO_PROMPT_HIT_HEIGHT 18

typedef struct Esp32IntroInputState_s {
    DoomRPG_t* doomRpg;
    uint32_t taps;
    uint32_t misses;
    int active;
} Esp32IntroInputState;

static Esp32IntroInputState inputState;

static uint32_t heap8Free(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8Block(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static int storyPositionIsSafe(const DoomCanvas_t* canvas) {
    if (canvas == NULL) {
        return 0;
    }

    switch (canvas->storyPage) {
    case 0:
        return canvas->storyTextPage >= 0 && canvas->storyTextPage <= 1;
    case 1:
    case 2:
        return canvas->storyTextPage == 0;
    default:
        return 0;
    }
}

static int boundaryIsSafe(const DoomRPG_t* doomRpg) {
    const DoomCanvas_t* canvas;
    const Render_t* render;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->menuSystem == NULL) {
        return 0;
    }

    canvas = doomRpg->doomCanvas;
    render = doomRpg->render;

    return canvas->state == ST_INTRO &&
           doomRpg->menuSystem->menu == MENU_NONE &&
           storyPositionIsSafe(canvas) &&
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

static int insideStoryViewport(int logicalX, int logicalY) {
    return logicalX >= ESP32_STORY_VIEWPORT_X &&
           logicalX < ESP32_STORY_VIEWPORT_X + ESP32_STORY_VIEWPORT_SIZE &&
           logicalY >= ESP32_STORY_VIEWPORT_Y &&
           logicalY < ESP32_STORY_VIEWPORT_Y + ESP32_STORY_VIEWPORT_SIZE;
}

static int insidePromptBand(int logicalX, int logicalY) {
    return insideStoryViewport(logicalX, logicalY) &&
           logicalY >= ESP32_STORY_VIEWPORT_Y + ESP32_STORY_VIEWPORT_SIZE -
                           INTRO_PROMPT_HIT_HEIGHT;
}

static void disarmInternal(void) {
    inputState.active = 0;
    PlatformInput_setTapCallback(NULL);
}

static void onTap(int16_t screenX,
                  int16_t screenY,
                  uint16_t pressure,
                  uint16_t rawX,
                  uint16_t rawY) {
    DoomCanvas_t* canvas;
    int logicalX;
    int logicalY;
    int accepted;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;

    if (!inputState.active || inputState.doomRpg == NULL) {
        return;
    }

    if (!Esp32IntroClock_isActive() || !boundaryIsSafe(inputState.doomRpg)) {
        printf("[INTROIN] FAILED runtime boundary clock=%d\n",
               Esp32IntroClock_isActive());
        disarmInternal();
        Esp32IntroClock_park("input-boundary-changed");
        return;
    }

    canvas = inputState.doomRpg->doomCanvas;
    logicalX = screenX / DOOMRPG_INTEGER_SCALE;
    logicalY = screenY / DOOMRPG_INTEGER_SCALE;
    accepted = canvas->storyPage == 1
                   ? insideStoryViewport(logicalX, logicalY)
                   : insidePromptBand(logicalX, logicalY);

    ++inputState.taps;
    printf("[INTROIN] TAP n=%u raw=%u,%u pressure=%u physical=%d,%d logical=%d,%d page=%d textPage=%d textDone=%d accepted=%d\n",
           (unsigned int)inputState.taps,
           rawX,
           rawY,
           pressure,
           screenX,
           screenY,
           logicalX,
           logicalY,
           canvas->storyPage,
           canvas->storyTextPage,
           canvas->showTextDone ? 1 : 0,
           accepted);

    if (!accepted) {
        ++inputState.misses;
        printf("[INTROIN] MISS n=%u page=%d promptBandY=%d..%d\n",
               (unsigned int)inputState.misses,
               canvas->storyPage,
               ESP32_STORY_VIEWPORT_Y + ESP32_STORY_VIEWPORT_SIZE -
                   INTRO_PROMPT_HIT_HEIGHT,
               ESP32_STORY_VIEWPORT_Y + ESP32_STORY_VIEWPORT_SIZE - 1);
        return;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();

    if (canvas->storyPage == 0 || canvas->storyPage == 2) {
        if (!canvas->showTextDone) {
            canvas->showTextDone = true;
            printf("[INTROIN] REVEAL page=%d textPage=%d t=%d\n",
                   canvas->storyPage,
                   canvas->storyTextPage,
                   canvas->time);
        }
        else if (canvas->storyPage == 0 && canvas->storyTextPage == 0) {
            canvas->storyTextPage = 1;
            canvas->showTextDone = false;
            if (!Esp32IntroClock_rebaseTextEpoch()) {
                printf("[INTROIN] FAILED More text epoch rebase\n");
                disarmInternal();
                Esp32IntroClock_park("input-text-rebase-failed");
                return;
            }
            printf("[INTROIN] MORE textPage=0->1 t=%d textEpoch=%d\n",
                   canvas->time,
                   canvas->storyTextTime);
        }
        else if (canvas->storyPage == 0 && canvas->storyTextPage == 1) {
            canvas->storyPage = 1;
            canvas->storyTextPage = 0;
            canvas->showTextDone = false;
            if (!Esp32IntroClock_rebasePageEpochs()) {
                printf("[INTROIN] FAILED Continue page epoch rebase\n");
                disarmInternal();
                Esp32IntroClock_park("input-page-rebase-failed");
                return;
            }
            printf("[INTROIN] CONTINUE storyPage=0->1 t=%d epoch=%d\n",
                   canvas->time,
                   canvas->storyAnimTime);
        }
        else {
            heapAfter = heap8Free();
            largestAfter = largest8Block();
            if (heapAfter != heapBefore || largestAfter != largestBefore) {
                printf("[INTROIN] FAILED final input heap changed heap8=%u->%u largest8=%u->%u\n",
                       (unsigned int)heapBefore,
                       (unsigned int)heapAfter,
                       (unsigned int)largestBefore,
                       (unsigned int)largestAfter);
                disarmInternal();
                Esp32IntroClock_park("input-final-allocation");
                return;
            }

            printf("[INTROIN] FINAL-CONTINUE page=2 textPage=0 t=%d\n",
                   canvas->time);
            Esp32IntroClock_park("intro-exit-ready");
            disarmInternal();
            printf("[INTROIN] READY-TO-EXIT state=%d page=%d textPage=%d heap8=%u largest8=%u assets=retained noDispose=yes noMapLoad=yes\n",
                   canvas->state,
                   canvas->storyPage,
                   canvas->storyTextPage,
                   (unsigned int)heapAfter,
                   (unsigned int)largestAfter);
            return;
        }
    }
    else {
        canvas->storyPage = 2;
        canvas->storyTextPage = 0;
        canvas->showTextDone = false;
        if (!Esp32IntroClock_rebasePageEpochs()) {
            printf("[INTROIN] FAILED animation skip epoch rebase\n");
            disarmInternal();
            Esp32IntroClock_park("input-page-rebase-failed");
            return;
        }
        printf("[INTROIN] SKIP-ANIM storyPage=1->2 t=%d epoch=%d\n",
               canvas->time,
               canvas->storyAnimTime);
    }

    heapAfter = heap8Free();
    largestAfter = largest8Block();

    if (heapAfter != heapBefore || largestAfter != largestBefore ||
        !boundaryIsSafe(inputState.doomRpg)) {
        printf("[INTROIN] FAILED transition invariant page=%d textPage=%d heap8=%u->%u largest8=%u->%u\n",
               canvas->storyPage,
               canvas->storyTextPage,
               (unsigned int)heapBefore,
               (unsigned int)heapAfter,
               (unsigned int)largestBefore,
               (unsigned int)largestAfter);
        disarmInternal();
        Esp32IntroClock_park("input-transition-invariant");
        return;
    }

    printf("[INTROIN] READY page=%d textPage=%d textDone=%d heap8=%u largest8=%u\n",
           canvas->storyPage,
           canvas->storyTextPage,
           canvas->showTextDone ? 1 : 0,
           (unsigned int)heapAfter,
           (unsigned int)largestAfter);
}

int Esp32IntroInput_arm(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;

    inputState.doomRpg = NULL;
    inputState.taps = 0;
    inputState.misses = 0;
    inputState.active = 0;
    PlatformInput_setTapCallback(NULL);

    if (!Esp32IntroClock_isActive() || !boundaryIsSafe(doomRpg) ||
        doomRpg->doomCanvas->storyPage != 0 ||
        doomRpg->doomCanvas->storyTextPage != 0) {
        printf("[INTROIN] FAILED arm boundary clock=%d\n",
               Esp32IntroClock_isActive());
        return 0;
    }

    inputState.doomRpg = doomRpg;
    inputState.active = 1;
    PlatformInput_setTapCallback(onTap);

    printf("[INTROIN] READY semantic press-edge tap armed; stable release rearms next tap; promptLogical=x%d..%d y%d..%d animLogical=x%d..%d y%d..%d\n",
           ESP32_STORY_VIEWPORT_X,
           ESP32_STORY_VIEWPORT_X + ESP32_STORY_VIEWPORT_SIZE - 1,
           ESP32_STORY_VIEWPORT_Y + ESP32_STORY_VIEWPORT_SIZE -
               INTRO_PROMPT_HIT_HEIGHT,
           ESP32_STORY_VIEWPORT_Y + ESP32_STORY_VIEWPORT_SIZE - 1,
           ESP32_STORY_VIEWPORT_X,
           ESP32_STORY_VIEWPORT_X + ESP32_STORY_VIEWPORT_SIZE - 1,
           ESP32_STORY_VIEWPORT_Y,
           ESP32_STORY_VIEWPORT_Y + ESP32_STORY_VIEWPORT_SIZE - 1);
    printf("[INTROIN] CONTRACT reveal -> More -> page1 animation -> page2 -> final PARK; dispose/map load blocked\n");
    return 1;
}

void Esp32IntroInput_disarm(void) {
    disarmInternal();
}

int Esp32IntroInput_isActive(void) {
    return inputState.active;
}
