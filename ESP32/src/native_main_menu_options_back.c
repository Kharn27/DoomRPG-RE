#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Render.h"

#include "native_main_menu_160x120_layout.h"
#include "native_main_menu_options_action.h"
#include "native_main_menu_options_back.h"
#include "native_main_menu_touch.h"
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

static DoomRPG_t* optionsDoomRpg = NULL;
static int optionsBackActive = 0;
static int backArmed = 0;
static uint32_t optionsTapCount = 0;
static uint32_t optionsExpectedFrameFNV = 0U;

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

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->menuSystem == NULL || doomRpg->render == NULL) {
        return 0;
    }

    render = doomRpg->render;
    return render->framebuffer != NULL &&
           render->shapeData == NULL &&
           render->mediaTexels == NULL &&
           !EspNativeWallCache_isActive() &&
           !EspNativeSpriteCache_isActive();
}

static int optionsItemAt(int16_t screenX, int16_t screenY) {
    const int logicalX = screenX / DOOMRPG_INTEGER_SCALE;
    const int logicalY = screenY / DOOMRPG_INTEGER_SCALE;
    int col;
    int row;

    if (logicalX >= DOOMRPG_ESP32_MAIN_MENU_CARD_COL0_LEFT &&
        logicalX <= DOOMRPG_ESP32_MAIN_MENU_CARD_COL0_RIGHT) {
        col = 0;
    }
    else if (logicalX >= DOOMRPG_ESP32_MAIN_MENU_CARD_COL1_LEFT &&
             logicalX <= DOOMRPG_ESP32_MAIN_MENU_CARD_COL1_RIGHT) {
        col = 1;
    }
    else {
        return -1;
    }

    if (logicalY >= DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_TOP &&
        logicalY <= DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_BOTTOM) {
        row = 0;
    }
    else if (logicalY >= DOOMRPG_ESP32_MAIN_MENU_CARD_ROW1_TOP &&
             logicalY <= DOOMRPG_ESP32_MAIN_MENU_CARD_ROW1_BOTTOM) {
        row = 1;
    }
    else {
        return -1;
    }

    return (row << 1) | col;
}

#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
static void registerOptionsHitboxOverlay(void) {
    int item;

    Esp32PlatformVideo_debugOverlayClear();
    for (item = 0; item < OPTIONS_ITEM_COUNT; ++item) {
        const int col = item & 1;
        const int row = item >> 1;
        const int left = col ? DOOMRPG_ESP32_MAIN_MENU_CARD_COL1_LEFT
                             : DOOMRPG_ESP32_MAIN_MENU_CARD_COL0_LEFT;
        const int right = col ? DOOMRPG_ESP32_MAIN_MENU_CARD_COL1_RIGHT
                              : DOOMRPG_ESP32_MAIN_MENU_CARD_COL0_RIGHT;
        const int top = row ? DOOMRPG_ESP32_MAIN_MENU_CARD_ROW1_TOP
                            : DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_TOP;
        const int bottom = row ? DOOMRPG_ESP32_MAIN_MENU_CARD_ROW1_BOTTOM
                               : DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_BOTTOM;
        Esp32PlatformVideo_debugOverlaySetZone(item,
                                               (int16_t)left,
                                               (int16_t)top,
                                               (int16_t)right,
                                               (int16_t)bottom);
    }

    Esp32PlatformVideo_debugOverlayRefresh();
    printf("[HITBOX] OPTIONS dashboard overlay zones=%d Back=active others=deferred framebuffer=untouched\n",
           OPTIONS_ITEM_COUNT);
}
#endif

static int paintBackState(int armed) {
    uint32_t nextHash = 0U;

    if (!DoomRPG_esp32PaintMainMenuOptionsDashboard(
            optionsDoomRpg, armed, &nextHash)) {
        printf("[OPTIONBACK] FAILED repaint Back armed=%d\n", armed);
        return 0;
    }

    optionsExpectedFrameFNV = nextHash;
    SDL_RenderPresent(NULL);
    printf("[OPTIONBACK] VISUAL Back armed=%d framebufferFNV=%08x\n",
           armed,
           (unsigned int)nextHash);
    return 1;
}

static int repaintMainMenuAfterBack(DoomRPG_t* doomRpg) {
    MenuSystem_t* menuSystem = doomRpg->menuSystem;
    Render_t* render = doomRpg->render;
    uint32_t finalHash = 0;
    uint32_t repaintStart;
    uint32_t repaintMs;

    printf("\n=== Doom RPG ESP32 fast Options -> MENU_MAIN Back ===\n");
    printf("[OPTIONBACK] Begin menu=%d selected=%d old=%d framebufferFNV=%08x shapeData=%p mediaTexels=%p\n",
           menuSystem->menu,
           menuSystem->selectedIndex,
           menuSystem->oldMenu,
           (unsigned int)framebufferHash(render),
           (void*)render->shapeData,
           (void*)render->mediaTexels);

    if (!graphicsBoundaryIsSafe(doomRpg) ||
        menuSystem->menu != MENU_MAIN_OPTIONS ||
        menuSystem->selectedIndex != OPTIONS_BACK_ITEM ||
        optionsExpectedFrameFNV == 0U ||
        framebufferHash(render) != optionsExpectedFrameFNV) {
        printf("[OPTIONBACK] FAILED precondition safe=%d menu=%d selected=%d framebuffer=%08x expected=%08x\n",
               graphicsBoundaryIsSafe(doomRpg),
               menuSystem->menu,
               menuSystem->selectedIndex,
               (unsigned int)framebufferHash(render),
               (unsigned int)optionsExpectedFrameFNV);
        return 0;
    }

    repaintStart = (uint32_t)DoomRPG_GetTimeMS();

    /* Preserve the original hierarchy transition. Only presentation changes:
     * after MenuSystem_back(), repaint the real MENU_MAIN model directly on an
     * opaque framebuffer instead of replaying BSP walls and sprites.
     */
    MenuSystem_back(menuSystem);

    if (!graphicsBoundaryIsSafe(doomRpg) ||
        menuSystem->menu != MENU_MAIN ||
        menuSystem->selectedIndex != 0 ||
        menuSystem->numItems != 4) {
        printf("[OPTIONBACK] FAILED real MenuSystem_back menu=%d selected=%d items=%d state=%d shapeData=%p mediaTexels=%p\n",
               menuSystem->menu,
               menuSystem->selectedIndex,
               menuSystem->numItems,
               doomRpg->doomCanvas->state,
               (void*)render->shapeData,
               (void*)render->mediaTexels);
        return 0;
    }

    printf("[OPTIONBACK] MODEL menu=%d type=%d old=%d selected=%d items=%d state=%d\n",
           menuSystem->menu,
           menuSystem->type,
           menuSystem->oldMenu,
           menuSystem->selectedIndex,
           menuSystem->numItems,
           doomRpg->doomCanvas->state);

    if (!DoomRPG_esp32RepaintOpaqueMainMenu(doomRpg, &finalHash)) {
        printf("[OPTIONBACK] FAILED bounded opaque MENU_MAIN repaint\n");
        return 0;
    }

    repaintMs = (uint32_t)DoomRPG_GetTimeMS() - repaintStart;

    printf("[OPTIONBACK] FAST End framebufferFNV=%08x expected=%08x runtimeFNV=%08x menu=%d selected=%d touchActive=%d repaintMs=%u shapeData=%p mediaTexels=%p\n",
           (unsigned int)finalHash,
           (unsigned int)DoomRPG_esp32MainMenuSelectionFramebufferFNV(0),
           (unsigned int)framebufferHash(render),
           menuSystem->menu,
           menuSystem->selectedIndex,
           DoomRPG_esp32MainMenuTouchIsActive(),
           (unsigned int)repaintMs,
           (void*)render->shapeData,
           (void*)render->mediaTexels);

    if (finalHash == 0U ||
        finalHash != DoomRPG_esp32MainMenuSelectionFramebufferFNV(0) ||
        finalHash != framebufferHash(render) ||
        menuSystem->menu != MENU_MAIN ||
        menuSystem->selectedIndex != 0 ||
        !DoomRPG_esp32MainMenuTouchIsActive() ||
        !graphicsBoundaryIsSafe(doomRpg)) {
        printf("[OPTIONBACK] FAILED fast roundtrip invariant\n");
        return 0;
    }

    printf("[OPTIONBACK] READY real MenuSystem_back + opaque bounded repaint; no MENUWALL/MENUSPRITE replay\n");
    printf("[OPTIONBACK] READY MENU_MAIN touch re-armed for another complete cycle\n");
    return 1;
}

static void optionsBackTap(int16_t screenX,
                           int16_t screenY,
                           uint16_t pressure,
                           uint16_t rawX,
                           uint16_t rawY) {
    int item;

    if (!optionsBackActive || optionsDoomRpg == NULL) {
        return;
    }

    optionsTapCount++;
    item = optionsItemAt(screenX, screenY);

    printf("[OPTIONBACK] TAP n=%u raw=%u,%u pressure=%u physical=%d,%d logical=%d,%d item=%d armed=%d\n",
           (unsigned int)optionsTapCount,
           rawX,
           rawY,
           pressure,
           screenX,
           screenY,
           screenX / DOOMRPG_INTEGER_SCALE,
           screenY / DOOMRPG_INTEGER_SCALE,
           item,
           backArmed);

    if (!graphicsBoundaryIsSafe(optionsDoomRpg) ||
        optionsDoomRpg->menuSystem->menu != MENU_MAIN_OPTIONS ||
        optionsExpectedFrameFNV == 0U ||
        framebufferHash(optionsDoomRpg->render) != optionsExpectedFrameFNV) {
        printf("[OPTIONBACK] FAILED runtime boundary menu=%d framebuffer=%08x expected=%08x\n",
               optionsDoomRpg->menuSystem->menu,
               (unsigned int)framebufferHash(optionsDoomRpg->render),
               (unsigned int)optionsExpectedFrameFNV);
        optionsBackActive = 0;
        PlatformInput_setTapCallback(NULL);
        return;
    }

    if (item != OPTIONS_BACK_ITEM) {
        if (backArmed) {
            if (!paintBackState(0)) {
                optionsBackActive = 0;
                PlatformInput_setTapCallback(NULL);
                return;
            }
            backArmed = 0;
        }
        if (item >= 1 && item < OPTIONS_ITEM_COUNT) {
            printf("[OPTIONBACK] DEFER item=%d text=\"%s\" action=disabled-this-increment\n",
                   item,
                   optionsDoomRpg->menuSystem->items[item].textField);
        }
        else {
            printf("[OPTIONBACK] MISS Back disarmed\n");
        }
        return;
    }

    if (!backArmed) {
        if (!paintBackState(1)) {
            optionsBackActive = 0;
            PlatformInput_setTapCallback(NULL);
            return;
        }
        backArmed = 1;
        printf("[OPTIONBACK] ARM Back awaitingReleasedSecondTap=yes visual=bright-card\n");
        return;
    }

    printf("[OPTIONBACK] CONFIRM Back action=MenuSystem_back+opaque-repaint\n");
    backArmed = 0;
    optionsBackActive = 0;
    PlatformInput_setTapCallback(NULL);

    if (!repaintMainMenuAfterBack(optionsDoomRpg)) {
        printf("[OPTIONBACK] FAILED executing fast Back roundtrip\n");
    }
}

int DoomRPG_esp32OptionsBackActivate(struct DoomRPG_s* doomRpgBase,
                                     uint32_t optionsFramebufferFNV) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;

    optionsDoomRpg = NULL;
    optionsBackActive = 0;
    backArmed = 0;
    optionsTapCount = 0;
    optionsExpectedFrameFNV = 0U;

    if (!graphicsBoundaryIsSafe(doomRpg) ||
        doomRpg->menuSystem->menu != MENU_MAIN_OPTIONS ||
        doomRpg->menuSystem->selectedIndex != 0 ||
        optionsFramebufferFNV == 0U ||
        framebufferHash(doomRpg->render) != optionsFramebufferFNV) {
        printf("[OPTIONBACK] FAILED activate safe=%d menu=%d selected=%d supplied=%08x framebuffer=%08x\n",
               graphicsBoundaryIsSafe(doomRpg),
               doomRpg != NULL && doomRpg->menuSystem != NULL
                   ? doomRpg->menuSystem->menu : -999,
               doomRpg != NULL && doomRpg->menuSystem != NULL
                   ? doomRpg->menuSystem->selectedIndex : -999,
               (unsigned int)optionsFramebufferFNV,
               doomRpg != NULL && doomRpg->render != NULL
                   ? (unsigned int)framebufferHash(doomRpg->render) : 0U);
        return 0;
    }

    optionsDoomRpg = doomRpg;
    optionsBackActive = 1;
    optionsExpectedFrameFNV = optionsFramebufferFNV;
#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
    registerOptionsHitboxOverlay();
#endif
    PlatformInput_setTapCallback(optionsBackTap);

    printf("[OPTIONBACK] READY Back card logical=x%d..%d y%d..%d physical=x%d..%d y%d..%d firstTap=bright-arm secondReleasedTap=back Video/Input/Sound=deferred fastOpaqueReturn=yes\n",
           DOOMRPG_ESP32_MAIN_MENU_CARD_COL0_LEFT,
           DOOMRPG_ESP32_MAIN_MENU_CARD_COL0_RIGHT,
           DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_TOP,
           DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_BOTTOM,
           DOOMRPG_ESP32_MAIN_MENU_CARD_COL0_LEFT * DOOMRPG_INTEGER_SCALE,
           ((DOOMRPG_ESP32_MAIN_MENU_CARD_COL0_RIGHT + 1) *
                DOOMRPG_INTEGER_SCALE) - 1,
           DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_TOP * DOOMRPG_INTEGER_SCALE,
           ((DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_BOTTOM + 1) *
                DOOMRPG_INTEGER_SCALE) - 1);
    return 1;
}
