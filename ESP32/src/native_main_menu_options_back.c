#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Render.h"

#include "native_main_menu_options_back.h"
#include "native_main_menu_touch.h"
#include "native_main_menu_touch_layout.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_touch_events.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#ifndef DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
#define DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY 0
#endif

#define EXPECTED_OPTIONS_FRAMEBUFFER_FNV 0x6058d47dU
#define EXPECTED_OPAQUE_MAIN_FNV 0x58a11171U
#define OPTIONS_ITEM_COUNT 4
#define OPTIONS_BACK_ITEM 0

/* One source of truth for Options touch geometry. Back is the only active
 * action today; Video/Input/Sound use the same zones for diagnostics and remain
 * deliberately deferred.
 */
static const int16_t optionsHitLeft[OPTIONS_ITEM_COUNT] = {
    15, 15, 15, 15
};
static const int16_t optionsHitRight[OPTIONS_ITEM_COUNT] = {
    119, 119, 119, 119
};
static const int16_t optionsHitTop[OPTIONS_ITEM_COUNT] = {
    64, 79, 91, 103
};
static const int16_t optionsHitBottom[OPTIONS_ITEM_COUNT] = {
    78, 90, 102, 114
};

static DoomRPG_t* optionsDoomRpg = NULL;
static int optionsBackActive = 0;
static int backArmed = 0;
static uint32_t optionsTapCount = 0;

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

static int optionsHitAt(int16_t screenX, int16_t screenY) {
    const int logicalX = screenX / DOOMRPG_INTEGER_SCALE;
    const int logicalY = screenY / DOOMRPG_INTEGER_SCALE;
    int item;

    for (item = 0; item < OPTIONS_ITEM_COUNT; ++item) {
        if (logicalX >= optionsHitLeft[item] &&
            logicalX <= optionsHitRight[item] &&
            logicalY >= optionsHitTop[item] &&
            logicalY <= optionsHitBottom[item]) {
            return item;
        }
    }

    return -1;
}

static void registerOptionsHitboxOverlay(void) {
    int item;

    Esp32PlatformVideo_debugOverlayClear();
    for (item = 0; item < OPTIONS_ITEM_COUNT; ++item) {
        Esp32PlatformVideo_debugOverlaySetZone(item,
                                               optionsHitLeft[item],
                                               optionsHitTop[item],
                                               optionsHitRight[item],
                                               optionsHitBottom[item]);
    }
    Esp32PlatformVideo_debugOverlayRefresh();

#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
    printf("[HITBOX] OPTIONS overlay registered from runtime zones=%d Back=active others=deferred framebuffer=untouched\n",
           OPTIONS_ITEM_COUNT);
#endif
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
        framebufferHash(render) != EXPECTED_OPTIONS_FRAMEBUFFER_FNV) {
        printf("[OPTIONBACK] FAILED precondition safe=%d menu=%d selected=%d framebuffer=%08x expected=%08x\n",
               graphicsBoundaryIsSafe(doomRpg),
               menuSystem->menu,
               menuSystem->selectedIndex,
               (unsigned int)framebufferHash(render),
               (unsigned int)EXPECTED_OPTIONS_FRAMEBUFFER_FNV);
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
           (unsigned int)EXPECTED_OPAQUE_MAIN_FNV,
           (unsigned int)framebufferHash(render),
           menuSystem->menu,
           menuSystem->selectedIndex,
           DoomRPG_esp32MainMenuTouchIsActive(),
           (unsigned int)repaintMs,
           (void*)render->shapeData,
           (void*)render->mediaTexels);

    if (finalHash != EXPECTED_OPAQUE_MAIN_FNV ||
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
    int row;

    if (!optionsBackActive || optionsDoomRpg == NULL) {
        return;
    }

    optionsTapCount++;
    row = optionsHitAt(screenX, screenY);

    printf("[OPTIONBACK] TAP n=%u raw=%u,%u pressure=%u physical=%d,%d logical=%d,%d row=%d armed=%d\n",
           (unsigned int)optionsTapCount,
           rawX,
           rawY,
           pressure,
           screenX,
           screenY,
           screenX / DOOMRPG_INTEGER_SCALE,
           screenY / DOOMRPG_INTEGER_SCALE,
           row,
           backArmed);

    if (!graphicsBoundaryIsSafe(optionsDoomRpg) ||
        optionsDoomRpg->menuSystem->menu != MENU_MAIN_OPTIONS ||
        framebufferHash(optionsDoomRpg->render) != EXPECTED_OPTIONS_FRAMEBUFFER_FNV) {
        printf("[OPTIONBACK] FAILED runtime boundary menu=%d framebuffer=%08x\n",
               optionsDoomRpg->menuSystem->menu,
               (unsigned int)framebufferHash(optionsDoomRpg->render));
        optionsBackActive = 0;
        PlatformInput_setTapCallback(NULL);
        return;
    }

    if (row != OPTIONS_BACK_ITEM) {
        backArmed = 0;
        if (row >= 1 && row <= 3) {
            printf("[OPTIONBACK] DEFER row=%d text=\"%s\" action=disabled-this-increment\n",
                   row,
                   optionsDoomRpg->menuSystem->items[row].textField);
        }
        else {
            printf("[OPTIONBACK] MISS Back disarmed\n");
        }
        return;
    }

    if (!backArmed) {
        backArmed = 1;
        printf("[OPTIONBACK] ARM Back awaitingReleasedSecondTap=yes\n");
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

    if (!graphicsBoundaryIsSafe(doomRpg) ||
        doomRpg->menuSystem->menu != MENU_MAIN_OPTIONS ||
        doomRpg->menuSystem->selectedIndex != 0 ||
        optionsFramebufferFNV != EXPECTED_OPTIONS_FRAMEBUFFER_FNV ||
        framebufferHash(doomRpg->render) != EXPECTED_OPTIONS_FRAMEBUFFER_FNV) {
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
    registerOptionsHitboxOverlay();
    PlatformInput_setTapCallback(optionsBackTap);

    printf("[OPTIONBACK] READY Back hit logical=x%d..%d y%d..%d physical=x%d..%d y%d..%d visualRowY=67..78 topTolerance=3 firstTap=arm secondReleasedTap=back Video/Input/Sound=deferred fastOpaqueReturn=yes\n",
           optionsHitLeft[OPTIONS_BACK_ITEM],
           optionsHitRight[OPTIONS_BACK_ITEM],
           optionsHitTop[OPTIONS_BACK_ITEM],
           optionsHitBottom[OPTIONS_BACK_ITEM],
           optionsHitLeft[OPTIONS_BACK_ITEM] * DOOMRPG_INTEGER_SCALE,
           ((optionsHitRight[OPTIONS_BACK_ITEM] + 1) * DOOMRPG_INTEGER_SCALE) - 1,
           optionsHitTop[OPTIONS_BACK_ITEM] * DOOMRPG_INTEGER_SCALE,
           ((optionsHitBottom[OPTIONS_BACK_ITEM] + 1) * DOOMRPG_INTEGER_SCALE) - 1);
    return 1;
}
