#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "MenuItem.h"
#include "MenuSystem.h"
#include "Render.h"

#include "native_main_menu_160x120_layout.h"
#include "native_main_menu_options_action.h"
#include "native_main_menu_touch.h"
#include "native_main_menu_touch_layout.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_video_config.h"

/* Keep ESP-IDF's stdbool macros after DoomRPG's legacy boolean enum. */
#include <esp_heap_caps.h>

#define OPTIONS_ITEM_COUNT 4
#define OPTIONS_BACK_ITEM 0
#define OPTIONS_ENABLED_MASK (1U << OPTIONS_BACK_ITEM)

static const char* expectedOptionsItems[OPTIONS_ITEM_COUNT] = {
    "Back",
    "Video",
    "Input",
    "Sound"
};

static const char* optionsDashboardLabels[OPTIONS_ITEM_COUNT] = {
    "BACK",
    "VIDEO",
    "INPUT",
    "SOUND"
};

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

static uint32_t framebufferHash(const Render_t* render) {
    if (render == NULL || render->framebuffer == NULL || render->pitch <= 0) {
        return 0U;
    }

    return fnv1a32((const uint8_t*)render->framebuffer,
                   (uint32_t)render->pitch * DOOMRPG_LOGICAL_HEIGHT);
}

static uint32_t modelHash(const MenuSystem_t* menuSystem) {
    uint32_t hash = 2166136261U;
    int i;

    hash ^= (uint32_t)menuSystem->menu;
    hash *= 16777619U;
    hash ^= (uint32_t)menuSystem->type;
    hash *= 16777619U;
    hash ^= (uint32_t)menuSystem->oldMenu;
    hash *= 16777619U;
    hash ^= (uint32_t)menuSystem->selectedIndex;
    hash *= 16777619U;
    hash ^= (uint32_t)menuSystem->scrollIndex;
    hash *= 16777619U;
    hash ^= (uint32_t)menuSystem->numItems;
    hash *= 16777619U;

    for (i = 0; i < menuSystem->numItems; ++i) {
        const unsigned char* p =
            (const unsigned char*)menuSystem->items[i].textField;
        while (*p != 0) {
            hash ^= *p++;
            hash *= 16777619U;
        }
        hash ^= 0U;
        hash *= 16777619U;
        hash ^= (uint32_t)menuSystem->items[i].flags;
        hash *= 16777619U;
        hash ^= (uint32_t)menuSystem->items[i].action;
        hash *= 16777619U;
    }

    return hash;
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

static int validateOptionsModel(const MenuSystem_t* menuSystem) {
    int i;

    if (menuSystem->menu != MENU_MAIN_OPTIONS ||
        menuSystem->type != 7 ||
        menuSystem->oldMenu != MENU_MAIN ||
        menuSystem->selectedIndex != 0 ||
        menuSystem->scrollIndex != 0 ||
        menuSystem->numItems != OPTIONS_ITEM_COUNT) {
        return 0;
    }

    for (i = 0; i < OPTIONS_ITEM_COUNT; ++i) {
        if (strcmp(menuSystem->items[i].textField, expectedOptionsItems[i]) != 0 ||
            menuSystem->items[i].textField2[0] != '\0' ||
            menuSystem->items[i].flags != 0 ||
            menuSystem->items[i].action != 0) {
            return 0;
        }
    }

    return 1;
}

int DoomRPG_esp32PaintMainMenuOptionsDashboard(
    struct DoomRPG_s* doomRpgBase,
    int backArmed,
    uint32_t* framebufferFNV) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;

    if (!graphicsBoundaryIsSafe(doomRpg) ||
        !validateOptionsModel(doomRpg->menuSystem)) {
        return 0;
    }

    return DoomRPG_esp32PaintMenuDashboardCards(
        doomRpg,
        optionsDashboardLabels,
        OPTIONS_BACK_ITEM,
        backArmed,
        OPTIONS_ENABLED_MASK,
        framebufferFNV);
}

static int paintOptionsBounded(DoomRPG_t* doomRpg,
                               uint32_t* logoHashOut,
                               uint32_t* finalHashOut) {
    DoomCanvas_t* doomCanvas = doomRpg->doomCanvas;
    MenuSystem_t* menuSystem = doomRpg->menuSystem;
    SDL_Rect logoDst;
    uint32_t logoHash;
    uint32_t finalHash;

    DoomRPG_setColor(doomRpg, 0x000000);
    DoomRPG_fillRect(doomRpg,
                     0,
                     0,
                     doomCanvas->displayRect.w,
                     doomCanvas->displayRect.h);

    logoDst.x = doomCanvas->displayRect.x +
                ((doomCanvas->displayRect.w -
                  DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_WIDTH) >> 1);
    logoDst.y = doomCanvas->displayRect.y +
                DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_Y;
    logoDst.w = DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_WIDTH;
    logoDst.h = DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_HEIGHT;

    if (SDL_RenderCopy(NULL, menuSystem->imgLogo.imgBitmap, NULL, &logoDst) != 0) {
        return 0;
    }
    logoHash = framebufferHash(doomRpg->render);
    if (logoHash == 0U ||
        !DoomRPG_esp32PaintMainMenuOptionsDashboard(
            doomRpg, 0, &finalHash) ||
        finalHash == logoHash) {
        return 0;
    }

    if (logoHashOut != NULL) *logoHashOut = logoHash;
    if (finalHashOut != NULL) *finalHashOut = finalHash;
    return 1;
}

int DoomRPG_esp32ActivateMainMenuOptions(struct DoomRPG_s* doomRpgBase,
                                         uint32_t* finalFramebufferFNV) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    MenuSystem_t* menuSystem;
    Render_t* render;
    uint32_t logoHash = 0U;
    uint32_t inputHash;
    uint32_t finalHash;
    uint32_t optionsModelHash;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint32_t expectedInputHash;
    int i;

    if (finalFramebufferFNV != NULL) *finalFramebufferFNV = 0U;

    printf("\n=== Doom RPG ESP32 real MENU_MAIN -> Options action ===\n");

    if (!graphicsBoundaryIsSafe(doomRpg)) {
        printf("[MAINOPTIONS] FAILED core/graphics boundary unavailable\n");
        return 0;
    }

    menuSystem = doomRpg->menuSystem;
    render = doomRpg->render;
    inputHash = framebufferHash(render);
    expectedInputHash =
        DoomRPG_esp32MainMenuSelectionFramebufferFNV(2);

    printf("[MAINOPTIONS] Begin menu=%d selected=%d framebufferFNV=%08x expectedSelectedOptionsFNV=%08x heap8=%u largest8=%u shapeData=%p mediaTexels=%p\n",
           menuSystem->menu,
           menuSystem->selectedIndex,
           (unsigned int)inputHash,
           (unsigned int)expectedInputHash,
           (unsigned int)heap8Free(),
           (unsigned int)largest8Block(),
           (void*)render->shapeData,
           (void*)render->mediaTexels);

    if (menuSystem->menu != MENU_MAIN ||
        menuSystem->selectedIndex != 2 ||
        expectedInputHash == 0U || inputHash != expectedInputHash) {
        printf("[MAINOPTIONS] FAILED precondition menu=%d selected=%d framebuffer=%08x expected=%08x\n",
               menuSystem->menu,
               menuSystem->selectedIndex,
               (unsigned int)inputHash,
               (unsigned int)expectedInputHash);
        return 0;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();

    /* The retained J2ME Menu_select() still assigns Options to semantic row 1.
     * The ESP32 presentation places Load Game there, so translate only for the
     * instant in which the original transition is invoked. */
    menuSystem->selectedIndex = 1;
    MenuSystem_select(menuSystem);

    if (!graphicsBoundaryIsSafe(doomRpg) || !validateOptionsModel(menuSystem)) {
        printf("[MAINOPTIONS] FAILED real transition menu=%d type=%d old=%d selected=%d scroll=%d items=%d state=%d shapeData=%p mediaTexels=%p\n",
               menuSystem->menu,
               menuSystem->type,
               menuSystem->oldMenu,
               menuSystem->selectedIndex,
               menuSystem->scrollIndex,
               menuSystem->numItems,
               doomRpg->doomCanvas->state,
               (void*)render->shapeData,
               (void*)render->mediaTexels);
        return 0;
    }

    optionsModelHash = modelHash(menuSystem);
    printf("[MAINOPTIONS] Model menu=%d type=%d old=%d selected=%d scroll=%d items=%d state=%d modelFNV=%08x\n",
           menuSystem->menu,
           menuSystem->type,
           menuSystem->oldMenu,
           menuSystem->selectedIndex,
           menuSystem->scrollIndex,
           menuSystem->numItems,
           doomRpg->doomCanvas->state,
           (unsigned int)optionsModelHash);

    for (i = 0; i < OPTIONS_ITEM_COUNT; ++i) {
        printf("[MAINOPTIONS] CARD index=%d col=%d row=%d text=\"%s\" flags=%d action=%d enabled=%s selected=%s\n",
               i,
               i & 1,
               i >> 1,
               menuSystem->items[i].textField,
               menuSystem->items[i].flags,
               menuSystem->items[i].action,
               i == OPTIONS_BACK_ITEM ? "yes" : "no",
               i == menuSystem->selectedIndex ? "yes" : "no");
    }

    if (!paintOptionsBounded(doomRpg, &logoHash, &finalHash)) {
        printf("[MAINOPTIONS] FAILED bounded Options paint\n");
        return 0;
    }

    printf("[MAINOPTIONS] HASH stage=logo fnv=%08x\n",
           (unsigned int)logoHash);
    printf("[MAINOPTIONS] HASH stage=dashboard fnv=%08x\n",
           (unsigned int)finalHash);
    heapAfter = heap8Free();
    largestAfter = largest8Block();

    printf("[MAINOPTIONS] framebufferFNV=%08x inputFNV=%08x changed=%s shapeData=%p mediaTexels=%p\n",
           (unsigned int)finalHash,
           (unsigned int)inputHash,
           finalHash != inputHash ? "yes" : "NO",
           (void*)render->shapeData,
           (void*)render->mediaTexels);
    printf("[MAINOPTIONS] End heap8=%u largest8=%u deltaFromStart=%d largestDelta=%d\n",
           (unsigned int)heapAfter,
           (unsigned int)largestAfter,
           (int)heapBefore - (int)heapAfter,
           (int)largestBefore - (int)largestAfter);

    if (finalHash == 0 || finalHash == inputHash ||
        render->shapeData != NULL || render->mediaTexels != NULL ||
        EspNativeWallCache_isActive() || EspNativeSpriteCache_isActive() ||
        heapAfter != heapBefore || largestAfter != largestBefore) {
        printf("[MAINOPTIONS] FAILED bounded transition invariant\n");
        return 0;
    }

    SDL_RenderPresent(NULL);
    if (finalFramebufferFNV != NULL) *finalFramebufferFNV = finalHash;
    printf("[MAINOPTIONS] Presented real MENU_MAIN_OPTIONS model with shared 2x2 dashboard paint\n");
    printf("[MAINOPTIONS] READY MenuSystem_select executed for Options; no legacy Render_render, no map reload, no gameplay loader\n");
    return 1;
}
