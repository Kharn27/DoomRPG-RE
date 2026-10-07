#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Render.h"

#include "esp_native_intro_state.h"
#include "native_intro_clock.h"
#include "native_intro_input.h"
#include "native_sprite_lru_cache.h"
#include "native_story_fit.h"
#include "native_wall_lru_cache.h"
#include "platform_touch_events.h"
#include "platform_video_config.h"

#include <esp_heap_caps.h>


typedef struct Esp32IntroInputState_s {
    DoomRPG_t* doomRpg;
    uint32_t taps;
    uint32_t misses;
    int active;
    int finalTextPresented;
} Esp32IntroInputState;

static Esp32IntroInputState inputState;

static uint32_t heap8Free(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8Block(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static int storyPositionIsSafe(const EspNativeIntroState_t* introState) {
    if (introState == NULL) {
        return 0;
    }

    switch (introState->storyPage) {
    case 0:
        return introState->storyTextPage >= 0 && introState->storyTextPage <= 1;
    case 1:
    case 2:
        return introState->storyTextPage == 0;
    default:
        return 0;
    }
}

static int boundaryIsSafe(const DoomRPG_t* doomRpg) {
    const DoomCanvas_t* canvas;
    const EspNativeIntroState_t* introState;
    const Render_t* render;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->menuSystem == NULL) {
        return 0;
    }

    canvas = doomRpg->doomCanvas;
    introState = EspNativeIntroState_view(doomRpg);
    render = doomRpg->render;

    return canvas->state == ST_INTRO &&
           doomRpg->menuSystem->menu == MENU_NONE &&
           storyPositionIsSafe(introState) &&
           introState->storyText1[0] != NULL &&
           introState->storyText1[1] != NULL &&
           introState->storyText2 != NULL &&
           introState->imgSpaceBG.imgBitmap != NULL &&
           introState->imgLinesLayer.imgBitmap != NULL &&
           introState->imgPlanetLayer.imgBitmap != NULL &&
           introState->imgSpaceship.imgBitmap != NULL &&
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
    EspNativeIntroState_t* introState;
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
    introState = EspNativeIntroState_get(inputState.doomRpg);
    if (introState == NULL) {
        printf("[INTROIN] FAILED native intro state unavailable\n");
        disarmInternal();
        Esp32IntroClock_park("input-native-state-missing");
        return;
    }
    logicalX = screenX / DOOMRPG_INTEGER_SCALE;
    logicalY = screenY / DOOMRPG_INTEGER_SCALE;
    accepted = logicalX >= 0 && logicalX < DOOMRPG_LOGICAL_WIDTH &&
               logicalY >= 0 && logicalY < DOOMRPG_LOGICAL_HEIGHT;

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
           introState->storyPage,
           introState->storyTextPage,
           introState->showTextDone ? 1 : 0,
           accepted);

    if (!accepted) {
        ++inputState.misses;
        printf("[INTROIN] MISS n=%u page=%d logical=%d,%d domain=full-screen\n",
               (unsigned int)inputState.misses,
               introState->storyPage,
               logicalX,
               logicalY);
        return;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();

    if (introState->storyPage == 0 || introState->storyPage == 2) {
        if (!introState->showTextDone) {
            introState->showTextDone = true;
            if (introState->storyPage == 2) {
                inputState.finalTextPresented = 0;
            }
            printf("[INTROIN] REVEAL page=%d textPage=%d t=%d\n",
                   introState->storyPage,
                   introState->storyTextPage,
                   canvas->time);
        }
        else if (introState->storyPage == 0 && introState->storyTextPage == 0) {
            introState->storyTextPage = 1;
            introState->showTextDone = false;
            if (!Esp32IntroClock_rebaseTextEpoch()) {
                printf("[INTROIN] FAILED More text epoch rebase\n");
                disarmInternal();
                Esp32IntroClock_park("input-text-rebase-failed");
                return;
            }
            printf("[INTROIN] MORE textPage=0->1 t=%d textEpoch=%d\n",
                   canvas->time,
                   introState->storyTextTime);
        }
        else if (introState->storyPage == 0 && introState->storyTextPage == 1) {
            introState->storyPage = 1;
            introState->storyTextPage = 0;
            introState->showTextDone = false;
            if (!Esp32IntroClock_rebasePageEpochs()) {
                printf("[INTROIN] FAILED Continue page epoch rebase\n");
                disarmInternal();
                Esp32IntroClock_park("input-page-rebase-failed");
                return;
            }
            printf("[INTROIN] CONTINUE storyPage=0->1 t=%d epoch=%d\n",
                   canvas->time,
                   introState->storyAnimTime);
        }
        else {
            if (!inputState.finalTextPresented) {
                printf("[INTROIN] FINAL-DEFER full final text has not been presented yet; keeping intro active\n");
                return;
            }

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

            printf("[INTROIN] FINAL-CONTINUE page=2 textPage=0 t=%d fullTextPresented=yes\n",
                   canvas->time);
            Esp32IntroClock_park("intro-exit-ready");
            disarmInternal();
            printf("[INTROIN] READY-TO-EXIT state=%d page=%d textPage=%d heap8=%u largest8=%u assets=retained noDispose=yes noMapLoad=yes\n",
                   canvas->state,
                   introState->storyPage,
                   introState->storyTextPage,
                   (unsigned int)heapAfter,
                   (unsigned int)largestAfter);
            return;
        }
    }
    else {
        introState->storyPage = 2;
        introState->storyTextPage = 0;
        introState->showTextDone = false;
        inputState.finalTextPresented = 0;
        if (!Esp32IntroClock_rebasePageEpochs()) {
            printf("[INTROIN] FAILED animation skip epoch rebase\n");
            disarmInternal();
            Esp32IntroClock_park("input-page-rebase-failed");
            return;
        }
        printf("[INTROIN] SKIP-ANIM storyPage=1->2 t=%d epoch=%d\n",
               canvas->time,
               introState->storyAnimTime);
    }

    heapAfter = heap8Free();
    largestAfter = largest8Block();

    if (heapAfter != heapBefore || largestAfter != largestBefore ||
        !boundaryIsSafe(inputState.doomRpg)) {
        printf("[INTROIN] FAILED transition invariant page=%d textPage=%d heap8=%u->%u largest8=%u->%u\n",
               introState->storyPage,
               introState->storyTextPage,
               (unsigned int)heapBefore,
               (unsigned int)heapAfter,
               (unsigned int)largestBefore,
               (unsigned int)largestAfter);
        disarmInternal();
        Esp32IntroClock_park("input-transition-invariant");
        return;
    }

    printf("[INTROIN] READY page=%d textPage=%d textDone=%d heap8=%u largest8=%u\n",
           introState->storyPage,
           introState->storyTextPage,
           introState->showTextDone ? 1 : 0,
           (unsigned int)heapAfter,
           (unsigned int)largestAfter);
}

int Esp32IntroInput_arm(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    EspNativeIntroState_t* introState;

    inputState.doomRpg = NULL;
    inputState.taps = 0;
    inputState.misses = 0;
    inputState.active = 0;
    inputState.finalTextPresented = 0;
    PlatformInput_setTapCallback(NULL);

    introState = EspNativeIntroState_get(doomRpg);
    if (!Esp32IntroClock_isActive() || !boundaryIsSafe(doomRpg) ||
        introState == NULL || introState->storyPage != 0 ||
        introState->storyTextPage != 0) {
        printf("[INTROIN] FAILED arm boundary clock=%d\n",
               Esp32IntroClock_isActive());
        return 0;
    }

    inputState.doomRpg = doomRpg;
    inputState.active = 1;
    PlatformInput_setTapCallback(onTap);

    printf("[INTROIN] READY semantic press-edge tap armed; stable release rearms next tap; tapDomain=full-screen logical=x0..%d y0..%d\n",
           DOOMRPG_LOGICAL_WIDTH - 1,
           DOOMRPG_LOGICAL_HEIGHT - 1);
    printf("[INTROIN] CONTRACT tap-anywhere reveal -> More -> page1 animation -> page2 -> visible full final text -> final PARK; dispose/map load blocked\n");
    return 1;
}

void Esp32IntroInput_notifyFramePresented(void) {
    DoomCanvas_t* canvas;
    EspNativeIntroState_t* introState;

    if (!inputState.active || inputState.doomRpg == NULL ||
        inputState.doomRpg->doomCanvas == NULL) {
        return;
    }

    canvas = inputState.doomRpg->doomCanvas;
    introState = EspNativeIntroState_get(inputState.doomRpg);
    if (introState == NULL) {
        return;
    }
    if (introState->storyPage == 2 && introState->storyTextPage == 0 &&
        introState->showTextDone && !inputState.finalTextPresented) {
        inputState.finalTextPresented = 1;
        printf("[INTROIN] FINAL-TEXT-PRESENTED t=%d continueUnlocked=yes\n",
               canvas->time);
    }
}

void Esp32IntroInput_disarm(void) {
    disarmInternal();
}

int Esp32IntroInput_isActive(void) {
    return inputState.active;
}
