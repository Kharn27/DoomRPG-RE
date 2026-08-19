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
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_video_config.h"

/* Keep ESP-IDF's stdbool macros after DoomRPG's legacy boolean enum. */
#include <esp_heap_caps.h>

#define EXPECTED_MAIN_OPTIONS_SELECTED_FNV 0x0cf107b1U
#define OPTIONS_ITEM_COUNT 4
#define OPTIONS_TEXT_X 28
#define OPTIONS_GLYPH_HEIGHT 12

static const char* expectedOptionsItems[OPTIONS_ITEM_COUNT] = {
    "Back",
    "Video",
    "Input",
    "Sound"
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

static int paintOptionsBounded(DoomRPG_t* doomRpg,
                               uint32_t stageHashes[OPTIONS_ITEM_COUNT + 1]) {
    DoomCanvas_t* doomCanvas = doomRpg->doomCanvas;
    MenuSystem_t* menuSystem = doomRpg->menuSystem;
    SDL_Rect logoDst;
    int i;

    DoomRPG_setColor(doomRpg, 0x000000);
    DoomRPG_fillRect(doomRpg,
                     0,
                     0,
                     doomCanvas->displayRect.w,
                     doomCanvas->displayRect.h);

    logoDst.x = doomCanvas->displayRect.x +
                ((doomCanvas->displayRect.w -
                  DOOMRPG_ESP32_MAIN_MENU_LOGO_WIDTH) >> 1);
    logoDst.y = doomCanvas->displayRect.y +
                DOOMRPG_ESP32_MAIN_MENU_LOGO_Y;
    logoDst.w = DOOMRPG_ESP32_MAIN_MENU_LOGO_WIDTH;
    logoDst.h = DOOMRPG_ESP32_MAIN_MENU_LOGO_HEIGHT;

    if (SDL_RenderCopy(NULL, menuSystem->imgLogo.imgBitmap, NULL, &logoDst) != 0) {
        return 0;
    }
    stageHashes[0] = framebufferHash(doomRpg->render);

    DoomRPG_setFontColor(doomRpg, 0xffffffff);

    for (i = 0; i < OPTIONS_ITEM_COUNT; ++i) {
        int x = OPTIONS_TEXT_X;
        const int y = DOOMRPG_ESP32_MAIN_MENU_ITEM_START_Y +
                      (i * DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT);

        if (i == menuSystem->selectedIndex) {
            DoomCanvas_drawImage(doomCanvas,
                                 &menuSystem->imgHand,
                                 x,
                                 y + (OPTIONS_GLYPH_HEIGHT >> 1),
                                 40);
            x += 2;
        }

        DoomCanvas_drawFont(doomCanvas,
                            menuSystem->items[i].textField,
                            x,
                            y,
                            0,
                            0,
                            -1,
                            false);
        stageHashes[i + 1] = framebufferHash(doomRpg->render);
    }

    DoomRPG_setFontColor(doomRpg, 0xffffffff);
    return 1;
}

int DoomRPG_esp32ActivateMainMenuOptions(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    MenuSystem_t* menuSystem;
    Render_t* render;
    uint32_t stageHashes[OPTIONS_ITEM_COUNT + 1] = {0};
    uint32_t inputHash;
    uint32_t finalHash;
    uint32_t optionsModelHash;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    int i;

    printf("\n=== Doom RPG ESP32 real MENU_MAIN -> Options action ===\n");

    if (!graphicsBoundaryIsSafe(doomRpg)) {
        printf("[MAINOPTIONS] FAILED core/graphics boundary unavailable\n");
        return 0;
    }

    menuSystem = doomRpg->menuSystem;
    render = doomRpg->render;
    inputHash = framebufferHash(render);

    printf("[MAINOPTIONS] Begin menu=%d selected=%d framebufferFNV=%08x expectedSelectedOptionsFNV=%08x heap8=%u largest8=%u shapeData=%p mediaTexels=%p\n",
           menuSystem->menu,
           menuSystem->selectedIndex,
           (unsigned int)inputHash,
           (unsigned int)EXPECTED_MAIN_OPTIONS_SELECTED_FNV,
           (unsigned int)heap8Free(),
           (unsigned int)largest8Block(),
           (void*)render->shapeData,
           (void*)render->mediaTexels);

    if (menuSystem->menu != MENU_MAIN ||
        menuSystem->selectedIndex != 1 ||
        inputHash != EXPECTED_MAIN_OPTIONS_SELECTED_FNV) {
        printf("[MAINOPTIONS] FAILED precondition menu=%d selected=%d framebuffer=%08x expected=%08x\n",
               menuSystem->menu,
               menuSystem->selectedIndex,
               (unsigned int)inputHash,
               (unsigned int)EXPECTED_MAIN_OPTIONS_SELECTED_FNV);
        return 0;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();

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
        printf("[MAINOPTIONS] ITEM index=%d y=%d text=\"%s\" flags=%d action=%d selected=%s\n",
               i,
               DOOMRPG_ESP32_MAIN_MENU_ITEM_START_Y +
                   (i * DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT),
               menuSystem->items[i].textField,
               menuSystem->items[i].flags,
               menuSystem->items[i].action,
               i == menuSystem->selectedIndex ? "yes" : "no");
    }

    if (!paintOptionsBounded(doomRpg, stageHashes)) {
        printf("[MAINOPTIONS] FAILED bounded Options paint\n");
        return 0;
    }

    printf("[MAINOPTIONS] HASH stage=logo fnv=%08x\n",
           (unsigned int)stageHashes[0]);
    for (i = 0; i < OPTIONS_ITEM_COUNT; ++i) {
        printf("[MAINOPTIONS] HASH stage=item%d text=\"%s\" fnv=%08x\n",
               i,
               menuSystem->items[i].textField,
               (unsigned int)stageHashes[i + 1]);
    }

    finalHash = framebufferHash(render);
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

    for (i = 1; i < OPTIONS_ITEM_COUNT + 1; ++i) {
        if (stageHashes[i] == stageHashes[i - 1]) {
            printf("[MAINOPTIONS] FAILED item %d did not change framebuffer\n", i - 1);
            return 0;
        }
    }

    SDL_RenderPresent(NULL);
    printf("[MAINOPTIONS] Presented real MENU_MAIN_OPTIONS model with bounded ESP32 paint\n");
    printf("[MAINOPTIONS] READY MenuSystem_select executed for Options; no legacy Render_render, no map reload, no gameplay loader\n");
    return 1;
}
