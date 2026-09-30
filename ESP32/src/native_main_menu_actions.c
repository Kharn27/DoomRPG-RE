#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Render.h"
#include "Sound.h"

#include "native_main_menu_actions.h"
#include "native_main_menu_load_action.h"
#include "native_main_menu_model.h"
#include "native_main_menu_options_action.h"
#include "native_main_menu_options_back.h"
#include "native_main_menu_start_action.h"
#include "native_main_menu_touch.h"
#include "native_main_menu_touch_layout.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_touch_events.h"
#include "platform_video_config.h"

#define MAIN_HELP_LINE_HEIGHT 12

static DoomRPG_t* helpDoomRpg;
static uint32_t helpExpectedFrameFNV;

static uint32_t fnv1a32(const uint8_t* data, uint32_t length) {
    uint32_t hash = 2166136261U;
    uint32_t i;
    for (i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t framebufferHash(const Render_t* render) {
    if (render == NULL || render->framebuffer == NULL || render->pitch <= 0) {
        return 0U;
    }
    return fnv1a32((const uint8_t*)render->framebuffer,
                   (uint32_t)render->pitch * DOOMRPG_LOGICAL_HEIGHT);
}

static int graphicsBoundaryIsSafe(const DoomRPG_t* doomRpg) {
    const Render_t* render;
    if (doomRpg == NULL || doomRpg->render == NULL ||
        doomRpg->doomCanvas == NULL || doomRpg->menuSystem == NULL ||
        doomRpg->menu == NULL) {
        return 0;
    }
    render = doomRpg->render;
    return render->framebuffer != NULL &&
           render->shapeData == NULL &&
           render->mediaTexels == NULL &&
           !EspNativeWallCache_isActive() &&
           !EspNativeSpriteCache_isActive();
}

static int paintHelp(DoomRPG_t* doomRpg, uint32_t* outFrameFNV) {
    DoomCanvas_t* canvas = doomRpg->doomCanvas;
    MenuSystem_t* menuSystem = doomRpg->menuSystem;
    int visible;
    int i;
    int x;
    int y;
    uint32_t frameFNV;

    if (outFrameFNV != NULL) *outFrameFNV = 0U;
    if (!graphicsBoundaryIsSafe(doomRpg) ||
        menuSystem->menu != MENU_MAIN_HELP_ABOUT ||
        menuSystem->type != 5 ||
        menuSystem->oldMenu != MENU_MAIN ||
        menuSystem->numItems <= 0) {
        return 0;
    }

    DoomRPG_setColor(doomRpg, 0x000000);
    DoomRPG_fillRect(doomRpg,
                     0,
                     0,
                     canvas->displayRect.w,
                     canvas->displayRect.h);
    DoomRPG_setFontColor(doomRpg, 0xffffffff);

    menuSystem->scrollIndex = 0;
    menuSystem->maxItems = canvas->displayRect.h / MAIN_HELP_LINE_HEIGHT;
    visible = menuSystem->numItems < menuSystem->maxItems
                  ? menuSystem->numItems
                  : menuSystem->maxItems;
    x = canvas->SCR_CX - 64;
    y = 0;

    for (i = 0; i < visible; ++i) {
        if (menuSystem->items[i].textField[0] != '\0') {
            DoomCanvas_drawFont(canvas,
                                menuSystem->items[i].textField,
                                x,
                                y,
                                0,
                                0,
                                -1,
                                false);
        }
        y += MAIN_HELP_LINE_HEIGHT;
    }

    DoomRPG_setFontColor(doomRpg, 0xffffffff);
    frameFNV = framebufferHash(doomRpg->render);
    if (frameFNV == 0U) return 0;

    SDL_RenderPresent(NULL);
    if (outFrameFNV != NULL) *outFrameFNV = frameFNV;

    printf("[MAINHELP] PAINT lines=%d visible=%d maxItems=%d framebufferFNV=%08x background=opaque-black renderer=native-list\n",
           menuSystem->numItems,
           visible,
           menuSystem->maxItems,
           (unsigned int)frameFNV);
    return 1;
}

static void helpBackTap(int16_t screenX,
                        int16_t screenY,
                        uint16_t pressure,
                        uint16_t rawX,
                        uint16_t rawY) {
    DoomRPG_t* doomRpg = helpDoomRpg;
    MenuSystem_t* menuSystem;
    uint32_t before;
    uint32_t finalFNV = 0U;

    (void)pressure;

    if (doomRpg == NULL || doomRpg->menuSystem == NULL) return;
    menuSystem = doomRpg->menuSystem;
    before = framebufferHash(doomRpg->render);

    printf("[MAINHELP] BACK tap raw=%u,%u physical=%d,%d frame=%08x expected=%08x menu=%d\n",
           rawX,
           rawY,
           screenX,
           screenY,
           (unsigned int)before,
           (unsigned int)helpExpectedFrameFNV,
           menuSystem->menu);

    PlatformInput_setTapCallback(NULL);

    if (!graphicsBoundaryIsSafe(doomRpg) ||
        menuSystem->menu != MENU_MAIN_HELP_ABOUT ||
        helpExpectedFrameFNV == 0U ||
        before != helpExpectedFrameFNV) {
        printf("[MAINHELP] FAILED back precondition\n");
        helpDoomRpg = NULL;
        helpExpectedFrameFNV = 0U;
        return;
    }

    /* This is intentionally the remaining next seam. The SELECT milestone owns
     * entry/dispatch; MenuSystem_back() is retained for the following bounded
     * cleanup and is shared with Options until then.
     */
    MenuSystem_back(menuSystem);

    if (menuSystem->menu != MENU_MAIN ||
        !DoomRPG_esp32RepaintOpaqueMainMenu(doomRpg, &finalFNV)) {
        printf("[MAINHELP] FAILED return to MENU_MAIN menu=%d\n",
               menuSystem->menu);
        helpDoomRpg = NULL;
        helpExpectedFrameFNV = 0U;
        return;
    }

    printf("[MAINHELP] READY back=MenuSystem_back-transitional mainFNV=%08x touch=armed\n",
           (unsigned int)finalFNV);
    helpDoomRpg = NULL;
    helpExpectedFrameFNV = 0U;
}

static int activateHelp(DoomRPG_t* doomRpg) {
    MenuSystem_t* menuSystem;
    uint32_t inputFNV;
    uint32_t expectedFNV;
    uint32_t helpFNV = 0U;

    if (!graphicsBoundaryIsSafe(doomRpg)) {
        printf("[MAINHELP] FAILED graphics boundary\n");
        return 0;
    }

    menuSystem = doomRpg->menuSystem;
    inputFNV = framebufferHash(doomRpg->render);
    expectedFNV = DoomRPG_esp32MainMenuSelectionFramebufferFNV(
        DOOMRPG_ESP32_MAIN_MENU_ACTION_HELP);

    if (menuSystem->menu != MENU_MAIN ||
        menuSystem->selectedIndex != DOOMRPG_ESP32_MAIN_MENU_ACTION_HELP ||
        expectedFNV == 0U || inputFNV != expectedFNV) {
        printf("[MAINHELP] FAILED precondition menu=%d selected=%d frame=%08x expected=%08x\n",
               menuSystem->menu,
               menuSystem->selectedIndex,
               (unsigned int)inputFNV,
               (unsigned int)expectedFNV);
        return 0;
    }

    Sound_playSound(doomRpg->sound, 5046, 0, 3);
    if (!DoomRPG_esp32MainMenuModelEnter(
            doomRpg, MENU_MAIN_HELP_ABOUT) ||
        !paintHelp(doomRpg, &helpFNV)) {
        printf("[MAINHELP] FAILED model/presentation\n");
        return 0;
    }

    helpDoomRpg = doomRpg;
    helpExpectedFrameFNV = helpFNV;
    PlatformInput_setTapCallback(helpBackTap);

    printf("[MAINHELP] READY menu=%d type=%d old=%d frame=%08x input=any-tap-back backOwner=legacy-next-seam\n",
           menuSystem->menu,
           menuSystem->type,
           menuSystem->oldMenu,
           (unsigned int)helpFNV);
    return 1;
}

DoomRpgEsp32MainMenuDispatchResult
DoomRPG_esp32MainMenuDispatchConfirmed(struct DoomRPG_s* doomRpgBase,
                                       int action) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    uint32_t optionsFNV = 0U;

    if (doomRpg == NULL) {
        return DOOMRPG_ESP32_MAIN_MENU_DISPATCH_FAILED;
    }

    printf("[MAINACTION] DISPATCH action=%d menu=%d selected=%d\n",
           action,
           doomRpg->menuSystem != NULL ? doomRpg->menuSystem->menu : -999,
           doomRpg->menuSystem != NULL
               ? doomRpg->menuSystem->selectedIndex : -999);

    switch (action) {
        case DOOMRPG_ESP32_MAIN_MENU_ACTION_START:
            return DoomRPG_esp32ActivateMainMenuStart(doomRpg)
                       ? DOOMRPG_ESP32_MAIN_MENU_DISPATCH_TRANSITIONED
                       : DOOMRPG_ESP32_MAIN_MENU_DISPATCH_FAILED;

        case DOOMRPG_ESP32_MAIN_MENU_ACTION_LOAD:
            if (DoomRPG_esp32ActivateMainMenuLoad(doomRpg)) {
                return DOOMRPG_ESP32_MAIN_MENU_DISPATCH_TRANSITIONED;
            }
            if (doomRpg->menuSystem != NULL &&
                doomRpg->menuSystem->menu == MENU_MAIN) {
                return DOOMRPG_ESP32_MAIN_MENU_DISPATCH_STAY_MAIN;
            }
            return DOOMRPG_ESP32_MAIN_MENU_DISPATCH_FAILED;

        case DOOMRPG_ESP32_MAIN_MENU_ACTION_OPTIONS:
            if (!DoomRPG_esp32ActivateMainMenuOptions(
                    doomRpg, &optionsFNV) ||
                !DoomRPG_esp32OptionsBackActivate(
                    doomRpg, optionsFNV)) {
                return DOOMRPG_ESP32_MAIN_MENU_DISPATCH_FAILED;
            }
            return DOOMRPG_ESP32_MAIN_MENU_DISPATCH_TRANSITIONED;

        case DOOMRPG_ESP32_MAIN_MENU_ACTION_HELP:
            return activateHelp(doomRpg)
                       ? DOOMRPG_ESP32_MAIN_MENU_DISPATCH_TRANSITIONED
                       : DOOMRPG_ESP32_MAIN_MENU_DISPATCH_FAILED;

        default:
            printf("[MAINACTION] FAILED unsupported action=%d mutation=no\n",
                   action);
            return DOOMRPG_ESP32_MAIN_MENU_DISPATCH_FAILED;
    }
}
