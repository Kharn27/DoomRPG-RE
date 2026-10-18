#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "esp_native_menu_state.h"
#include "Render.h"

#include "native_main_menu_160x120_layout.h"
#include "native_main_menu_actions.h"
#include "esp_native_backlight.h"
#include "native_main_menu_options_action.h"
#include "native_main_menu_options_back.h"
#include "native_main_menu_touch.h"
#include "native_main_menu_present.h"
#include "native_main_menu_touch_layout.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_touch_events.h"
#include "platform_video_config.h"

#ifndef DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
#define DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY 0
#endif

#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
#include "platform_video_c_bridge.h"
#endif

#define OPTIONS_ITEM_COUNT 4
#define OPTIONS_BACK_ITEM 0
#define OPTIONS_VIDEO_ITEM 1

static DoomRPG_t* optionsDoomRpg = NULL;
static int optionsBackActive = 0;
static int armedItem = -1; /* Parent only: -1=none, 0=Back, 1=Luminosite */
static uint8_t luminositeOpen = 0U; /* Child uses direct single taps. */
static uint32_t optionsTapCount = 0;
static uint32_t optionsExpectedFrameFNV = 0U;

static int optionsItemAt(int16_t screenX, int16_t screenY) {
    const int logicalX = screenX / DOOMRPG_INTEGER_SCALE;
    const int logicalY = screenY / DOOMRPG_INTEGER_SCALE;
    int item;
    for (item = 0; item < OPTIONS_ITEM_COUNT; ++item) {
        int left, top, right, bottom;
        DoomRPG_esp32MainMenuCardRect(item, &left, &top, &right, &bottom);
        if (logicalX >= left && logicalX <= right &&
            logicalY >= top && logicalY <= bottom) return item;
    }
    return -1;
}

static int luminositeItemAt(int16_t screenX, int16_t screenY) {
    const int logicalX = screenX / DOOMRPG_INTEGER_SCALE;
    const int logicalY = screenY / DOOMRPG_INTEGER_SCALE;
    int item;
    for (item = 0; item < DOOMRPG_ESP32_BRIGHTNESS_CARD_COUNT; ++item) {
        int left, top, right, bottom;
        DoomRPG_esp32BrightnessCardRect(item, &left, &top, &right, &bottom);
        if (logicalX >= left && logicalX <= right &&
            logicalY >= top && logicalY <= bottom) return item;
    }
    return -1;
}

#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
static void registerOptionsHitboxOverlay(void) {
    int item;
    Esp32PlatformVideo_debugOverlayClear();
    const int count = luminositeOpen ?
        DOOMRPG_ESP32_BRIGHTNESS_CARD_COUNT : OPTIONS_ITEM_COUNT;
    for (item = 0; item < count; ++item) {
        int left, top, right, bottom;
        if (luminositeOpen) {
            DoomRPG_esp32BrightnessCardRect(item, &left, &top, &right, &bottom);
        } else {
            DoomRPG_esp32MainMenuCardRect(item, &left, &top, &right, &bottom);
        }
        Esp32PlatformVideo_debugOverlaySetZone(item,
                                               (int16_t)left,
                                               (int16_t)top,
                                               (int16_t)right,
                                               (int16_t)bottom);
    }
    Esp32PlatformVideo_debugOverlayRefresh();
    printf("[HITBOX] OPTIONS split-rail overlay zones=%d layer=%s framebuffer=untouched\n",
           count, luminositeOpen ? "luminosite" : "options");
}

#endif

static int paintOptionsState(int selectedItem, int armed) {
    uint32_t nextHash = 0U;

    if (!DoomRPG_esp32PaintMainMenuOptionsDashboardSelection(
            optionsDoomRpg, selectedItem, armed, &nextHash)) {
        printf("[OPTIONBACK] FAILED repaint item=%d armed=%d\n",
               selectedItem, armed);
        return 0;
    }

    optionsExpectedFrameFNV = nextHash;
    SDL_RenderPresent(NULL);
    printf("[OPTIONBACK] VISUAL item=%d armed=%d brightness=%u framebufferFNV=%08x\n",
           selectedItem, armed, (unsigned int)Esp32Backlight_percent(),
           (unsigned int)nextHash);
    return 1;
}

static int paintLuminositeState(void) {
    uint32_t nextHash = 0U;

    if (!DoomRPG_esp32PaintMainMenuBrightnessDashboard(
            optionsDoomRpg, &nextHash)) {
        printf("[OPTIONSBRIGHTNESS] FAILED paint child\n");
        return 0;
    }
    optionsExpectedFrameFNV = nextHash;
    SDL_RenderPresent(NULL);
    printf("[OPTIONSBRIGHTNESS] VISUAL percent=%u framebufferFNV=%08x\n",
           (unsigned int)Esp32Backlight_percent(), (unsigned int)nextHash);
    return 1;
}

static int returnToMainMenu(DoomRPG_t* doomRpg) {
    EspNativeMenuState_t* menuSystem = doomRpg->menuSystem;
    Render_t* render = doomRpg->render;
    uint32_t finalHash = 0U;

    printf("\n=== Doom RPG ESP32 fast Options -> MENU_MAIN Back ===\n");
    printf("[OPTIONBACK] Begin menu=%d selected=%d old=%d framebufferFNV=%08x shapeData=%p mediaTexels=%p\n",
           menuSystem->menu,
           menuSystem->selectedIndex,
           menuSystem->oldMenu,
           (unsigned int)DoomRPG_esp32MainMenuFramebufferHash(render),
           (void*)render->shapeData,
           (void*)render->mediaTexels);

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg) ||
        menuSystem->menu != MENU_MAIN_OPTIONS ||
        menuSystem->selectedIndex != OPTIONS_BACK_ITEM ||
        luminositeOpen != 0U ||
        optionsExpectedFrameFNV == 0U ||
        DoomRPG_esp32MainMenuFramebufferHash(render) != optionsExpectedFrameFNV) {
        printf("[OPTIONBACK] FAILED precondition safe=%d menu=%d selected=%d framebuffer=%08x expected=%08x\n",
               DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg),
               menuSystem->menu,
               menuSystem->selectedIndex,
               (unsigned int)DoomRPG_esp32MainMenuFramebufferHash(render),
               (unsigned int)optionsExpectedFrameFNV);
        return 0;
    }

    if (!DoomRPG_esp32MainMenuReturnToMain(
            doomRpg,
            MENU_MAIN_OPTIONS,
            "options",
            &finalHash)) {
        printf("[OPTIONBACK] FAILED native Back action\n");
        return 0;
    }

    printf("[OPTIONBACK] FAST End framebufferFNV=%08x expected=%08x runtimeFNV=%08x menu=%d selected=%d touchActive=%d shapeData=%p mediaTexels=%p\n",
           (unsigned int)finalHash,
           (unsigned int)DoomRPG_esp32MainMenuSelectionFramebufferFNV(0),
           (unsigned int)DoomRPG_esp32MainMenuFramebufferHash(render),
           menuSystem->menu,
           menuSystem->selectedIndex,
           DoomRPG_esp32MainMenuTouchIsActive(),
           (void*)render->shapeData,
           (void*)render->mediaTexels);
    printf("[OPTIONBACK] READY native semantic Back + opaque bounded repaint; no MenuSystem_back, no MENUWALL/MENUSPRITE replay\n");
    printf("[OPTIONBACK] READY MENU_MAIN touch re-armed for another complete cycle\n");
    return 1;
}

static void optionsBackTap(int16_t screenX,
                           int16_t screenY,
                           uint16_t pressure,
                           uint16_t rawX,
                           uint16_t rawY) {
    int item;

    if (!optionsBackActive || optionsDoomRpg == NULL) return;

    optionsTapCount++;
    item = luminositeOpen ? luminositeItemAt(screenX, screenY) :
                            optionsItemAt(screenX, screenY);
    printf("[OPTIONBACK] TAP n=%u raw=%u,%u pressure=%u physical=%d,%d logical=%d,%d layer=%s item=%d armedItem=%d\n",
           (unsigned int)optionsTapCount, rawX, rawY, pressure,
           screenX, screenY, screenX / DOOMRPG_INTEGER_SCALE,
           screenY / DOOMRPG_INTEGER_SCALE,
           luminositeOpen ? "luminosite" : "options", item, armedItem);

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(optionsDoomRpg) ||
        optionsDoomRpg->menuSystem->menu != MENU_MAIN_OPTIONS ||
        optionsExpectedFrameFNV == 0U ||
        DoomRPG_esp32MainMenuFramebufferHash(optionsDoomRpg->render) !=
            optionsExpectedFrameFNV) {
        printf("[OPTIONBACK] FAILED runtime boundary menu=%d framebuffer=%08x expected=%08x\n",
               optionsDoomRpg->menuSystem->menu,
               (unsigned int)DoomRPG_esp32MainMenuFramebufferHash(optionsDoomRpg->render),
               (unsigned int)optionsExpectedFrameFNV);
        optionsBackActive = 0;
        PlatformInput_setTapCallback(NULL);
        DoomRPG_esp32MainMenuRecover(optionsDoomRpg, "options-touch-precondition");
        return;
    }

    if (luminositeOpen) {
        if (item == 0) {
            /* BACK returns to Options, not directly to MENU_MAIN. */
            if (!paintOptionsState(OPTIONS_BACK_ITEM, 0)) {
                optionsBackActive = 0;
                PlatformInput_setTapCallback(NULL);
                DoomRPG_esp32MainMenuRecover(optionsDoomRpg,
                                             "luminosite-back-repaint-failed");
                return;
            }
            luminositeOpen = 0U;
            armedItem = -1;
#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
            registerOptionsHitboxOverlay();
#endif
            printf("[OPTIONSBRIGHTNESS] BACK to=Options percent=%u mutation=ui-only\n",
                   (unsigned int)Esp32Backlight_percent());
            return;
        }
        if (item >= 1 && item < DOOMRPG_ESP32_BRIGHTNESS_CARD_COUNT) {
            const uint8_t before = Esp32Backlight_percent();
            const uint8_t after = (uint8_t)(item * 25U);
            if (after != before) {
                if (!Esp32Backlight_setPercent(after)) {
                    printf("[OPTIONSBRIGHTNESS] FAILED set percent=%u->%u\n",
                           (unsigned int)before, (unsigned int)after);
                    return;
                }
                if (!paintLuminositeState()) {
                    optionsBackActive = 0;
                    PlatformInput_setTapCallback(NULL);
                    DoomRPG_esp32MainMenuRecover(optionsDoomRpg,
                                                 "luminosite-set-repaint-failed");
                    return;
                }
            }
            printf("[OPTIONSBRIGHTNESS] SELECT percent=%u->%u changed=%s directTap=yes persistence=session gameplayTurn=no framebufferPixels=rail-only\n",
                   (unsigned int)before, (unsigned int)after,
                   after == before ? "no" : "yes");
            return;
        }
        printf("[OPTIONSBRIGHTNESS] MISS outside-presets no-action\n");
        return;
    }

    /* Input/Sound stay fail-closed. Unavailable PWM also makes Luminosite inert. */
    if (item != OPTIONS_BACK_ITEM &&
        (item != OPTIONS_VIDEO_ITEM || !Esp32Backlight_isReady())) {
        if (armedItem != -1) {
            if (!paintOptionsState(OPTIONS_BACK_ITEM, 0)) {
                optionsBackActive = 0;
                PlatformInput_setTapCallback(NULL);
                DoomRPG_esp32MainMenuRecover(optionsDoomRpg,
                                             "options-disarm-repaint-failed");
                return;
            }
            armedItem = -1;
        }
        printf("[OPTIONBACK] DEFER item=%d action=disabled-or-outside\n", item);
        return;
    }

    /* Only parent navigation requires two released taps. */
    if (armedItem != item) {
        if (!paintOptionsState(item, 1)) {
            optionsBackActive = 0;
            PlatformInput_setTapCallback(NULL);
            DoomRPG_esp32MainMenuRecover(optionsDoomRpg,
                                         "options-arm-repaint-failed");
            return;
        }
        armedItem = item;
        printf("[OPTIONBACK] ARM item=%d awaitingReleasedSecondTap=yes\n", item);
        return;
    }

    if (item == OPTIONS_VIDEO_ITEM) {
        if (!paintLuminositeState()) {
            optionsBackActive = 0;
            PlatformInput_setTapCallback(NULL);
            DoomRPG_esp32MainMenuRecover(optionsDoomRpg,
                                         "luminosite-open-repaint-failed");
            return;
        }
        luminositeOpen = 1U;
        armedItem = -1;
#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
        registerOptionsHitboxOverlay();
#endif
        printf("[OPTIONSBRIGHTNESS] OPEN from=Options percent=%u presets=25/50/75/100 directTap=yes back=parent noDesktopMenu=yes\n",
               (unsigned int)Esp32Backlight_percent());
        return;
    }

    printf("[OPTIONBACK] CONFIRM Back action=native-semantic-return\n");
    armedItem = -1;
    optionsBackActive = 0;
    PlatformInput_setTapCallback(NULL);
    if (!returnToMainMenu(optionsDoomRpg)) {
        printf("[OPTIONBACK] FAILED executing fast Back roundtrip\n");
        DoomRPG_esp32MainMenuRecover(optionsDoomRpg, "options-back-failed");
    }
}

int DoomRPG_esp32OptionsBackActivate(struct DoomRPG_s* doomRpgBase,
                                     uint32_t optionsFramebufferFNV) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;

    optionsDoomRpg = NULL;
    optionsBackActive = 0;
    armedItem = -1;
    luminositeOpen = 0U;
    optionsTapCount = 0;
    optionsExpectedFrameFNV = 0U;

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg) ||
        doomRpg->menuSystem->menu != MENU_MAIN_OPTIONS ||
        doomRpg->menuSystem->selectedIndex != 0 ||
        optionsFramebufferFNV == 0U ||
        DoomRPG_esp32MainMenuFramebufferHash(doomRpg->render) != optionsFramebufferFNV) {
        printf("[OPTIONBACK] FAILED activate safe=%d menu=%d selected=%d supplied=%08x framebuffer=%08x\n",
               DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg),
               doomRpg != NULL && doomRpg->menuSystem != NULL
                   ? doomRpg->menuSystem->menu : -999,
               doomRpg != NULL && doomRpg->menuSystem != NULL
                   ? doomRpg->menuSystem->selectedIndex : -999,
               (unsigned int)optionsFramebufferFNV,
               doomRpg != NULL && doomRpg->render != NULL
                   ? (unsigned int)DoomRPG_esp32MainMenuFramebufferHash(doomRpg->render) : 0U);
        return 0;
    }

    optionsDoomRpg = doomRpg;
    optionsBackActive = 1;
    optionsExpectedFrameFNV = optionsFramebufferFNV;
#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
    registerOptionsHitboxOverlay();
#endif
    PlatformInput_setTapCallback(optionsBackTap);

    {
        int left, top, right, bottom;
        DoomRPG_esp32MainMenuCardRect(OPTIONS_BACK_ITEM,
                                     &left, &top, &right, &bottom);
        printf("[OPTIONBACK] READY Back card logical=x%d..%d y%d..%d physical=x%d..%d y%d..%d firstTap=red-arm secondReleasedTap=confirm Luminosite=child-presets-25-50-75-100 Input/Sound=deferred fastOpaqueReturn=yes\n",
               left, right, top, bottom,
               left * DOOMRPG_INTEGER_SCALE,
               ((right + 1) * DOOMRPG_INTEGER_SCALE) - 1,
               top * DOOMRPG_INTEGER_SCALE,
               ((bottom + 1) * DOOMRPG_INTEGER_SCALE) - 1);
    }
    return 1;
}
